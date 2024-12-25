//===----------------------------------------------------------------------===//
//                         DuckDB
//
// csv_file_storage.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "read_only_storage.hpp"

namespace duckdb {

class CsvFileStorageExtension : public ReadOnlyStorageExtension {
public:
	CsvFileStorageExtension();
};

class CsvFileCatalog : public ReadOnlyCatalog {
public:
	explicit CsvFileCatalog(AttachedDatabase &db_p, const string &file_p, const string &schname_p,
							const string &relname_p, const vector<LogicalType> &column_types_p,
							const vector<string> &column_names_p);

	string GetCatalogType() override {
		return "csv_scanner";
	}

	idx_t GetDataBaseByteSize(ClientContext &context) override;

	void Initialize(bool load_builtin) override;
	void ScanSchemas(ClientContext &context, std::function<void(SchemaCatalogEntry &)> callback) override;
	optional_ptr<SchemaCatalogEntry> GetSchema(CatalogTransaction transaction, const string &schema_name,
	                                           OnEntryNotFound if_not_found,
	                                           QueryErrorContext error_context = QueryErrorContext()) override;

private:
	const string schema;
	// Catalog has a single schema/table entry corresponding to the CSV file
	unique_ptr<SchemaCatalogEntry> entry;
	idx_t database_size;
};

class CsvFileTableEntry : public ReadOnlyTableCatalogEntry {
public:
	CsvFileTableEntry(Catalog &catalog_p, SchemaCatalogEntry &schema_p, CreateTableInfo &info_p, const string &file_p,
					  const string &relname_p, const vector<LogicalType> &column_types_p,
					  const vector<string> &column_names_p);

	unique_ptr<BaseStatistics> GetStatistics(ClientContext &context, column_t column_id) override;
	TableFunction GetScanFunction(ClientContext &context, unique_ptr<FunctionData> &bind_data) override;
	TableStorageInfo GetStorageInfo(ClientContext &context) override;

	const string file;
	const string relname;
	const vector<LogicalType> column_types;
	const vector<string> column_names;
};

class CsvFileSchemaEntry : public ReadOnlySchemaCatalogEntry {
	friend class CsvFileCatalog;

public:
	CsvFileSchemaEntry(Catalog &catalog_p, CreateSchemaInfo &info_p, const string &file_p, const string &relname_p,
					   const vector<LogicalType> &column_types_p, const vector<string> &column_names_p);

	void Scan(ClientContext &context, CatalogType type, const std::function<void(CatalogEntry &)> &callback) override;
	optional_ptr<CatalogEntry> GetEntry(CatalogTransaction transaction, CatalogType type, const string &name_p) override;

protected:
	unique_ptr<CsvFileTableEntry> table;
};

} // namespace duckdb