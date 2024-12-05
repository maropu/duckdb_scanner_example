#include "csv_scanner.hpp"

#include "duckdb/main/extension_util.hpp"

namespace duckdb {

struct ScanCsvOptions {
	idx_t buffer_size = CsvBlockIterator::CSV_BUFFER_SIZE;
};

struct ScanCsvBindData : public TableFunctionData {
public:
	explicit ScanCsvBindData(const vector<string> &column_names_p, const vector<LogicalType> &column_types_p,
							 const ScanCsvOptions &options_p, shared_ptr<FileHandle> file_handle_p)
		: column_names(column_names_p), column_types(column_types_p), options(options_p), file_handle(file_handle_p) {
	};

	const vector<string> column_names;
	const vector<LogicalType> column_types;
	const ScanCsvOptions options;

	shared_ptr<FileHandle> file_handle;
};

struct CsvGlobalState : public GlobalTableFunctionState {
public:
	CsvGlobalState(idx_t system_threads_p, unique_ptr<CsvBlockIterator> csv_block_iterator_p)
	: system_threads(system_threads_p), csv_block_iterator(std::move(csv_block_iterator_p)),
	  reader_idx(0), finished(false) {
	}

	unique_ptr<CsvBlock> Next() {
		lock_guard<mutex> lock(main_mutex);
		auto block = csv_block_iterator->Next();
		if (!block) {
			finished = true;
		}
		return block;
	}

	//! Returns Current Progress of this CSV Read
	double GetProgress() const {
		return csv_block_iterator->GetProgress();
	}

	//! Calculates the Max Threads that will be used by this CSV Scanner
	idx_t MaxThreads() const override {
		idx_t total_threads = csv_block_iterator->GetFileSize() / csv_block_iterator->GetBufferSize() + 1;
		if (total_threads < system_threads) {
			return total_threads;
		}
		return system_threads;
	}

	const idx_t NextCsvReaderIndex() {
		return reader_idx++;
	}

	bool IsDone() {
		lock_guard<mutex> lock(main_mutex);
		return finished;
	}

private:
	//! Because this global state can be accessed in Parallel we need a mutex.
	mutex main_mutex;

	//! Basically max number of threads in DuckDB
	idx_t system_threads;

	//! The CSV block iterator
	unique_ptr<CsvBlockIterator> csv_block_iterator;
	atomic<idx_t> reader_idx;
	bool finished;
};

struct CsvLocalState : public LocalTableFunctionState {
public:
	CsvLocalState(unique_ptr<CsvReader> csv_reader_p)
	: csv_reader(std::move(csv_reader_p)) {
	}

	//! The CSV reader
	unique_ptr<CsvReader> csv_reader;
	bool done = false;
};

static void ParseSchemaFromParam(ClientContext &context, const Value &param,
								 vector<LogicalType> &column_types, vector<string> &column_names) {
	auto &param_type = param.type();
	if (param_type.id() != LogicalTypeId::STRUCT) {
		throw BinderException("schema param requires a struct as input");
	}
	auto &struct_children = StructValue::GetChildren(param);
	D_ASSERT(StructType::GetChildCount(param_type) == struct_children.size());
	for (idx_t i = 0; i < struct_children.size(); i++) {
		auto &name = StructType::GetChildName(param_type, i);
		auto &val = struct_children[i];
		column_names.push_back(name);
		if (val.type().id() != LogicalTypeId::VARCHAR) {
			throw BinderException("schema param requires a type specification as string");
		}
		// Only support VARCHAR/BIGINT/DOUBLE for simplicity
		auto tpe = TransformStringToLogicalType(StringValue::Get(val), context);
		switch (tpe.id()) {
			case LogicalTypeId::VARCHAR:
			case LogicalTypeId::BIGINT:
			case LogicalTypeId::DOUBLE:
				break;
			default:
				throw BinderException("scan_csv_ex only supports VARCHAR, BIGINT, and DOUBLE types");
		}
		column_types.emplace_back(tpe);
	}
	if (column_names.empty()) {
		throw BinderException("schema param requires at least a single column as input!");
	}
}

static const ScanCsvOptions ParseNamedParameters(named_parameter_map_t &in, ClientContext &context) {
	ScanCsvOptions options;
	for (auto &kv : in) {
		auto loption = StringUtil::Lower(kv.first);
		if (loption == "buffer_size") {
			options.buffer_size = kv.second.GetValue<uint64_t>();
			if (options.buffer_size < 1024) {
				throw BinderException("buffer_size must be at least 1024 bytes");
			}
		} else {
			throw BinderException("Unknown parameter for scan_csv_ex: %s", loption);
		}
	}
	return options;
}

static duckdb::unique_ptr<FunctionData> ScanCsvBind(ClientContext &context, TableFunctionBindInput &input,
													vector<LogicalType> &return_types, vector<string> &names) {
	D_ASSERT(input.inputs.size() == 2);
	auto &file_path = StringValue::Get(input.inputs[0]);
	auto &fs = FileSystem::GetFileSystem(context);
	auto file_handle = fs.OpenFile(file_path, FileFlags::FILE_FLAGS_READ);
	ParseSchemaFromParam(context, input.inputs[1], return_types, names);
	auto options = ParseNamedParameters(input.named_parameters, context);
	auto bind_data = make_uniq<ScanCsvBindData>(names, return_types, options, std::move(file_handle));
	return std::move(bind_data);
}

static unique_ptr<GlobalTableFunctionState> ScanCsvInitGlobal(ClientContext &context, TableFunctionInitInput &input) {
	auto &bind_data = input.bind_data->Cast<ScanCsvBindData>();
	auto &allocator = BufferAllocator::Get(context);
	auto buffer_size = bind_data.options.buffer_size;
	auto csv_block_iterator = make_uniq<CsvBlockIterator>(allocator, bind_data.file_handle, buffer_size);
	return make_uniq<CsvGlobalState>(context.db->NumberOfThreads(), std::move(csv_block_iterator));
}

unique_ptr<LocalTableFunctionState> ScanCsvInitLocal(ExecutionContext &context, TableFunctionInitInput &input,
                                                     GlobalTableFunctionState *global_state_p) {
	if (!global_state_p) {
		return nullptr;
	}
	auto &global_state = global_state_p->Cast<CsvGlobalState>();
	if (global_state.IsDone()) {
		// nothing to do
		return nullptr;
	}
	auto csv_block = global_state.Next();
	if (!csv_block) {
		return nullptr;
	}
	auto reader_idx = global_state.NextCsvReaderIndex();
	auto &bind_data = input.bind_data->Cast<ScanCsvBindData>();
	auto csv_reader = make_uniq<CsvReader>(reader_idx, bind_data.column_names, bind_data.column_types,
										   std::move(csv_block));
	return make_uniq<CsvLocalState>(std::move(csv_reader));
}

static void ScanCsvFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &bind_data = data_p.bind_data->Cast<ScanCsvBindData>();
	if (!data_p.global_state) {
		return;
	}
	auto &csv_global_state = data_p.global_state->Cast<CsvGlobalState>();
	if (!data_p.local_state) {
		return;
	}
	auto &csv_local_state = data_p.local_state->Cast<CsvLocalState>();
	if (csv_local_state.done) {
		return;
	}

	csv_local_state.csv_reader->Flush(output);
	if (output.size() == 0) {
		auto csv_block = csv_global_state.Next();
		if (!csv_block) {
			csv_local_state.done = true;
			return;
		}
		csv_local_state.csv_reader->UpdateBlock(std::move(csv_block));
		csv_local_state.csv_reader->Flush(output);
		D_ASSERT(output.size() > 0);
	}
}

static string ScanCsvToString(const FunctionData *bind_data_p) {
	auto &bind_data = bind_data_p->Cast<ScanCsvBindData>();
	return bind_data.file_handle->file_system.ExtractName(bind_data.file_handle->GetPath());
}

static double ScanCsvProgress(ClientContext &context, const FunctionData *bind_data_p,
							  const GlobalTableFunctionState *global_state) {
	if (!global_state) {
		return 0.0;
	}
	auto &data = global_state->Cast<CsvGlobalState>();
	return data.GetProgress();
}

static idx_t ScanCsvGetBatchIndex(ClientContext &context, const FunctionData *bind_data_p,
								  LocalTableFunctionState *local_state, GlobalTableFunctionState *global_state) {
	return (local_state->Cast<CsvLocalState>()).csv_reader->GetReaderIndex();
}

static unique_ptr<NodeStatistics> ScanCsvCardinality(ClientContext &context, const FunctionData *bind_data_p) {
	auto &bind_data = bind_data_p->Cast<ScanCsvBindData>();
	auto estimated_row_width = bind_data.column_names.size() * 5;
	auto per_file_cardinality = bind_data.file_handle->GetFileSize() / estimated_row_width;
	return make_uniq<NodeStatistics>(per_file_cardinality);
}

static void ScanCsvSerializer(Serializer &serializer, const optional_ptr<FunctionData> bind_data_p,
							  const TableFunction &function) {
	throw NotImplementedException("ScanCsvSerialize");
}

static unique_ptr<FunctionData> ScanCsvDeserializer(Deserializer &deserializer, TableFunction &function) {
	throw NotImplementedException("ScanCsvDeserialize");
}

static void ScanCsvAddNamedParameters(TableFunction &table_function) {
	table_function.named_parameters["buffer_size"] = LogicalType::UBIGINT;
}

static TableFunction GetFunction() {
	TableFunction scan_csv("scan_csv_ex", {LogicalType::VARCHAR, LogicalType::ANY},
						   ScanCsvFunction, ScanCsvBind, ScanCsvInitGlobal, ScanCsvInitLocal);
	scan_csv.to_string = ScanCsvToString;
	scan_csv.table_scan_progress = ScanCsvProgress;
	scan_csv.get_batch_index = ScanCsvGetBatchIndex;
	scan_csv.cardinality = ScanCsvCardinality;
	scan_csv.serialize = ScanCsvSerializer;
	scan_csv.deserialize = ScanCsvDeserializer;
	scan_csv.global_initialization = TableFunctionInitialization::INITIALIZE_ON_EXECUTE;
	scan_csv.projection_pushdown = false;
	scan_csv.filter_pushdown = false;
	scan_csv.pushdown_complex_filter = nullptr;
	scan_csv.type_pushdown = nullptr;
	ScanCsvAddNamedParameters(scan_csv);
	return scan_csv;
}

void CsvScannerFunction::RegisterFunction(DatabaseInstance &db) {
	ExtensionUtil::RegisterFunction(db, GetFunction());
}

CsvBlockIterator::CsvBlockIterator(Allocator &allocator, shared_ptr<FileHandle> file_handle_p, idx_t buffer_size)
	: allocator(allocator), file_handle(std::move(file_handle_p)), current_file_pos(0), buffer_size(buffer_size) {
};

unique_ptr<CsvBlock> CsvBlockIterator::Next() {
	if (current_file_pos >= file_handle->GetFileSize()) {
		return nullptr;
	}

	auto buffer = make_uniq<CsvFileBuffer>(allocator, buffer_size);
	buffer->Read(*file_handle, current_file_pos);

	if (current_file_pos + buffer_size >= file_handle->GetFileSize()) {
		auto read_bytes = file_handle->GetFileSize() - current_file_pos;
		current_file_pos += read_bytes;
		return make_uniq<CsvBlock>(std::move(buffer), read_bytes);
	}

	// Rewind the byte read position to the last newline one
	auto buffer_ptr = char_ptr_cast(buffer->internal_buffer);
	auto read_bytes = buffer_size;
	while (read_bytes > 0) {
		if (buffer_ptr[read_bytes - 1] != '\n') {
			read_bytes--;
		} else {
			break;
		}
	}

	if (read_bytes == 0) {
		throw IOException("Could not read CSV block: too long single line in file");
	}

	current_file_pos += read_bytes;

	return make_uniq<CsvBlock>(std::move(buffer), read_bytes);
}

CsvReader::CsvReader(idx_t idx, const vector<string> &column_names_p, const vector<LogicalType> &column_types_p,
					 unique_ptr<CsvBlock> block_p)
	: reader_idx(idx), column_names(column_names_p), column_types(column_types_p),
	  block(std::move(block_p)), current_buffer_pos(0) {
}

inline idx_t FindNextTargetChar(const char *data, idx_t len, char target) {
	idx_t i = 0;
	while (i < len && data[i] != target) {
		i++;
	}
	return i;
}

void CsvReader::Flush(DataChunk &chunk) {
	auto data_ptr = char_ptr_cast(block->GetData());
	auto data_size = block->GetSize();
	if (current_buffer_pos >= data_size) {
		return;
	}

	for (idx_t i = 0; i < STANDARD_VECTOR_SIZE; i++) {
		for (idx_t j = 0; j < column_types.size(); j++) {
			auto next_sep = j == column_types.size() - 1 ? '\n' : ',';
			auto len = FindNextTargetChar(data_ptr + current_buffer_pos, data_size - current_buffer_pos, next_sep);
			if (len == 0) {
				// If the CSV parsing process is in the middle, set NULL to the remaining columns
				// and increase the number of rows.
				if (j != 0) {
					// TODO: handle left empty fields
					chunk.SetCardinality(i + 1);
				}
				return;
			}

			auto &out_vec = chunk.data[j];

			switch (column_types[j].id()) {
			case LogicalTypeId::VARCHAR: {
				auto str = StringVector::AddString(out_vec, data_ptr + current_buffer_pos, len);
				FlatVector::GetData<string_t>(out_vec)[i] = str;
				break;
			}

			case LogicalTypeId::BIGINT: {
				auto iv = std::stoll(string(data_ptr + current_buffer_pos, len));
				FlatVector::GetData<int64_t>(out_vec)[i] = iv;
				break;
			}

			case LogicalTypeId::DOUBLE: {
				auto dv = std::stod(string(data_ptr + current_buffer_pos, len));
				FlatVector::GetData<double>(out_vec)[i] = dv;
				break;
			}

			default:
				throw InternalException("Unsupported Type %s", column_types[j].ToString());
			}

			current_buffer_pos += len + 1;
		}

		chunk.SetCardinality(i + 1);
	}
}

} // namespace duckdb
