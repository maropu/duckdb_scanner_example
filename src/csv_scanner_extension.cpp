#define DUCKDB_BUILD_LOADABLE_EXTENSION
#include "csv_scanner.hpp"

using namespace duckdb;

extern "C" {

DUCKDB_EXTENSION_API void csv_scanner_init(DatabaseInstance &db) {
	CsvScannerFunction::RegisterFunction(db);
}

DUCKDB_EXTENSION_API const char *csv_scanner_version() {
	return CSV_SCANNER_EXTENSION_VERSION;
}

}
