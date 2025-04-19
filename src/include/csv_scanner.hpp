//===----------------------------------------------------------------------===//
//                         DuckDB
//
// csv_scanner.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb.hpp"

namespace duckdb {

#define CSV_SCANNER_EXTENSION_VERSION "1.0.0"

struct CsvScannerFunction : public TableFunction {
	static void RegisterFunction(DatabaseInstance &db);
};

class CsvScanFunction : public TableFunction {
public:
	CsvScanFunction();
};

struct CsvFileBuffer {
public:
	CsvFileBuffer(Allocator &allocator, uint64_t buffer_size)
	: allocator(allocator), buffer_size(buffer_size) {
		internal_buffer = allocator.AllocateData(buffer_size);
	}

	~CsvFileBuffer() {
		if (!internal_buffer) {
			return;
		}
		allocator.FreeData(internal_buffer, buffer_size);
	}

	//! Read into the internal buffer from the specified location.
	const void Read(FileHandle &handle, uint64_t location) {
		D_ASSERT(handle.GetFileSize() > location);
		auto nbytes = MinValue<uint64_t>(handle.GetFileSize() - location, buffer_size);
		handle.Read(internal_buffer, nbytes, location);
	}

	Allocator &allocator;
	data_ptr_t internal_buffer;
	uint64_t buffer_size;
};

struct CsvBlock {
public:
	CsvBlock(unique_ptr<CsvFileBuffer> data, idx_t actual_size_p)
	: actual_size(actual_size_p), data(std::move(data)) {
	};

	inline data_ptr_t GetData() {
		return data->internal_buffer;
	}

	const idx_t GetSize() const {
		return actual_size;
	}

private:
	const idx_t actual_size;
	unique_ptr<CsvFileBuffer> data;
};

struct CsvBlockIterator {
public:
	CsvBlockIterator(Allocator &allocator, shared_ptr<FileHandle> file_handle_p, idx_t buffer_size);

	unique_ptr<CsvBlock> Next();

	//! Returns Current Progress of this CSV Read
	const double GetProgress() const {
		return 100.0 * ((double)current_file_pos / file_handle->GetFileSize());
	}

	const idx_t GetBufferSize() const {
		return buffer_size;
	}

	const idx_t GetFileSize() const {
		return file_handle->GetFileSize();
	}

	//! TODO: Should benchmarks other values
	static constexpr idx_t CSV_BUFFER_SIZE = 32000000; // 32MB

private:
	Allocator &allocator;
	shared_ptr<FileHandle> file_handle;
	idx_t current_file_pos;
	idx_t buffer_size;
};

struct CsvReader {
public:
	explicit CsvReader(idx_t idx, const vector<string> &column_names_p, const vector<LogicalType> &column_types_p,
	                   unique_ptr<CsvBlock> block_p);

	//! Flushes the result to the chunk
	void Flush(DataChunk &chunk);

	void UpdateBlock(unique_ptr<CsvBlock> block_p) {
		block = std::move(block_p);
		current_buffer_pos = 0;
	}

	const idx_t GetReaderIndex() const {
		return reader_idx;
	}

private:
	const idx_t reader_idx;
	const vector<string> column_names;
	const vector<LogicalType> column_types;
	unique_ptr<CsvBlock> block;
	idx_t current_buffer_pos;
};

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

} // namespace duckdb
