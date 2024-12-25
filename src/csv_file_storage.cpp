#include <iostream>
#include <regex>
#include <string>

#include "csv_file_storage.hpp"
#include "csv_scanner.hpp"

#include "duckdb/common/constants.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {

static string ParseNamedParameter(const string &name, const string &connection_string) {
    std::regex pattern(R"((\S+?)=(\{.*?\}|\S+))");
    std::smatch match;
    std::string::const_iterator searchStart(connection_string.cbegin());

    // Use regex to find the key-value pair
    while (std::regex_search(searchStart, connection_string.cend(), match, pattern)) {
        if (match.size() == 3) { // match[1] is the key, match[2] is the value
            std::string key = match[1].str();
            std::string value = match[2].str();
            if (key == name) {
                return value;
            }
        }
        searchStart = match.suffix().first;
    }

	throw BinderException("Could not find parameter %s in connection string", name);
}

static void ParseSchemaString(ClientContext &context, const string &schema_string,
							  std::vector<LogicalType> &column_types, std::vector<string> &column_names) {
	string current_key;
	string current_value;
	bool in_key = true;
	bool in_quotes = false;

	for (char c : schema_string) {
		if (c == '"') {
			in_quotes = !in_quotes;
		} else if (in_quotes) {
			if (in_key) {
				current_key += c;
			} else {
				current_value += c;
			}
		} else if (c == ':') {
			in_key = false;
		} else if (c == ',' || c == '}') {
			column_names.push_back(current_key);
			column_types.push_back(TransformStringToLogicalType(current_value));
			current_key.clear();
			current_value.clear();
			in_key = true;
		}
	}
}

static void ParseSchemaFromSchemaString(ClientContext &context, const string &schema_string,
										vector<LogicalType> &column_types, vector<string> &column_names) {
	auto test = TransformStringToLogicalType(schema_string, context);
	auto schema = StringUtil::ParseJSONMap(schema_string);
	for (auto &entry : schema) {
		auto type = TransformStringToLogicalType(entry.second, context);
		column_types.push_back(type);
		column_names.push_back(entry.first);
	}
}

// ATTACH 'file=data/test.csv relname=testrel schema={"a": "varchar", "b": "bigint", "c": "double"}' AS csv (TYPE CSV_SCANNER);
static unique_ptr<Catalog> CsvFileAttach(StorageExtensionInfo *storage_info, ClientContext &context,
										 AttachedDatabase &db, const string &name, AttachInfo &info,
										 AccessMode access_mode) {
	string connection_string = info.path;
	string file = ParseNamedParameter("file", connection_string);
	string relname = ParseNamedParameter("relname", connection_string);
	string schema = ParseNamedParameter("schema", connection_string);
	string schname = "csv_file";
	vector<LogicalType> column_types;
	vector<string> column_names;
	ParseSchemaString(context, schema, column_types, column_names);
	return make_uniq<CsvFileCatalog>(db, file, schname, relname, column_types, column_names);
}

CsvFileStorageExtension::CsvFileStorageExtension() {
	attach = CsvFileAttach;
}

CsvFileCatalog::CsvFileCatalog(AttachedDatabase &db_p, const string &file_p, const string &schname_p,
							   const string &relname_p, const vector<LogicalType> &column_types_p,
							   const vector<string> &column_names_p)
	: ReadOnlyCatalog(db_p), default_schema(schname_p) {
	CreateSchemaInfo info;
	auto schema = make_uniq<CsvFileSchemaEntry>(*this, info, file_p, relname_p, column_types_p, column_names_p);
	entries[schname_p] = move(schema);
}

void CsvFileCatalog::Initialize(bool load_builtin) {
}

void CsvFileCatalog::ScanSchemas(ClientContext &context, std::function<void(SchemaCatalogEntry &)> callback) {
	lock_guard<mutex> l(entry_lock);
	for (auto &e : entries) {
		callback(*reinterpret_cast<SchemaCatalogEntry *>(e.second.get()));
	}
}

optional_ptr<SchemaCatalogEntry> CsvFileCatalog::GetSchema(CatalogTransaction transaction, const string &schema_name,
														   OnEntryNotFound if_not_found,
														   QueryErrorContext error_context) {
	if (schema_name == DEFAULT_SCHEMA) {
		return GetSchema(transaction, default_schema, if_not_found, error_context);
	}
	auto it = entries.find(schema_name);
    if (it != entries.end()) {
		return reinterpret_cast<SchemaCatalogEntry *>(it->second.get());
    }
	if (if_not_found != OnEntryNotFound::RETURN_NULL) {
		throw BinderException("Schema with name \"%s\" not found", schema_name);
	}
    return nullptr;
}

CsvFileTableEntry::CsvFileTableEntry(Catalog &catalog_p, SchemaCatalogEntry &schema_p, CreateTableInfo &info_p,
									 const string &file_p, const string &relname_p,
									 const vector<LogicalType> &column_types_p, const vector<string> &column_names_p)
	: ReadOnlyTableCatalogEntry(catalog_p, schema_p, info_p), file(file_p), relname(relname_p),
	  column_types(column_types_p), column_names(column_names_p) {
}

unique_ptr<BaseStatistics> CsvFileTableEntry::GetStatistics(ClientContext &context, column_t column_id) {
	return nullptr;
}

TableFunction CsvFileTableEntry::GetScanFunction(ClientContext &context, unique_ptr<FunctionData> &bind_data) {
	auto &fs = FileSystem::GetFileSystem(context);
	auto file_handle = fs.OpenFile(file, FileFlags::FILE_FLAGS_READ);
	auto result = make_uniq<ScanCsvBindData>(column_names, column_types, ScanCsvOptions(), std::move(file_handle));
	bind_data = std::move(result);
	auto function = CsvScanFunction();
	return function;
}

TableStorageInfo CsvFileTableEntry::GetStorageInfo(ClientContext &context) {
	TableStorageInfo info;
	info.cardinality = 0;
	return info;
}

CsvFileSchemaEntry::CsvFileSchemaEntry(Catalog &catalog, CreateSchemaInfo &info, const string &file_p,
									   const string &relname_p, const vector<LogicalType> &column_types_p,
									   const vector<string> &column_names_p)
	: ReadOnlySchemaCatalogEntry(catalog, info) {
	CreateTableInfo table_info(*this, relname_p);
	for (idx_t i = 0; i < column_names_p.size(); i++) {
		ColumnDefinition c(column_names_p[i], column_types_p[i]);
		table_info.columns.AddColumn(std::move(c));
	}
	table = make_uniq<CsvFileTableEntry>(catalog, *this, table_info, file_p, relname_p, column_types_p, column_names_p);
}

void CsvFileSchemaEntry::Scan(ClientContext &context, CatalogType type,
							  const std::function<void(CatalogEntry &)> &callback) {
	if (!CatalogTypeIsSupported(type)) {
		return;
	}
	callback(*table.get());
}

optional_ptr<CatalogEntry> CsvFileSchemaEntry::GetEntry(CatalogTransaction transaction, CatalogType type,
														const string &name) {
	if (CatalogTypeIsSupported(type) && name == table->relname) {
		return table.get();
	}
	return nullptr;
}

}