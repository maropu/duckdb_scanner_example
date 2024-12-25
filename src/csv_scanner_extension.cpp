#define DUCKDB_BUILD_LOADABLE_EXTENSION
#include "csv_file_storage.hpp"
#include "csv_scanner.hpp"

using namespace duckdb;

extern "C" {

DUCKDB_EXTENSION_API const char *csv_scanner_version() {
	return CSV_SCANNER_EXTENSION_VERSION;
}

// TODO: XXX_storage_init functions are not called by the DuckDB core?
DUCKDB_EXTENSION_API void csv_scanner_storage_init(DBConfig &config) {
	config.storage_extensions["csv_scanner"] = make_uniq<CsvFileStorageExtension>();
}

// XXX_init is called when loading extension binaries by duckdb/src/main/extension/extension_load.cpp
//  - https://github.com/duckdb/duckdb/blob/ab8c90985741ac68cd203c8396022894c1771d4b/src/main/extension/extension_load.cpp#L530-L552
DUCKDB_EXTENSION_API void csv_scanner_init(DatabaseInstance &db) {
	// Instead of csv_scanner_storage_init, we inject this extension here into DuckDB
	csv_scanner_storage_init(db.config);
	CsvScannerFunction::RegisterFunction(db);
}

}
