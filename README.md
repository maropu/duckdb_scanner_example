[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build and test](https://github.com/maropu/duckdb_scanner_example/actions/workflows/build_and_tests.yml/badge.svg)](https://github.com/maropu/duckdb_scanner_example/actions/workflows/build_and_tests.yml)

This repository aims not to implenet a full-fledge CSV parser for DuckDB but to demonstrate a minimum table
function example for letting you know how to implement a custom scanner for your datasource.
This CSV parser implementation relies on a DuckDB table function and, if you would like to see how it works,
please see [maropu/duckdb_extension_example](https://github.com/maropu/duckdb_extension_example).

# Minimum Required Components

```bash
.
|-- CMakeLists.txt                  // Root CMake build file
|-- Makefile                        // Build script to wrap cmake
|-- data                            // Test data used in `test/sql/csv_scanner.test`
|   |-- random.csv
|   `-- test.csv
|-- duckdb                          // DuckDB source code that this extension depends on
|-- extension_config.cmake          // Extension configure file included by DuckDB's build system
|-- makefiles
|   `-- duckdb_extension.Makefile   // common build configuration to compile extention, copied from `duckdb/extension-ci-tools`
|-- src
|   |-- CMakeLists.txt              // CMake build file to list source files
|   |-- csv_file_storage.cpp        // CSV file storage implementation
|   |-- csv_scanner_extension.cpp   // CSV parser implmenetation
|   |-- include
|   |   |-- csv_file_storage.hpp    // Header file for CSV file storage
|   |   |-- csv_scanner.hpp         // Header file for CSV parser
|   |   `-- read_only_storage.hpp   // Header file for read-only storage
|   `-- scan_csv.cpp                // Entrypoint where DuckDB loads this extension
|-- test
|   `-- sql
|       `-- csv_scanner.test        // Test code
`-- vcpkg.json                      // Dependency definition file
```

# Specification of this toy CSV parser

 - Multi-threading for scanning CSV data supported
 - VARCHAR, BIGINT, and DOUBLE types only supported
 - Schema inference not supported

# Running this example

```bash
$ git submodule init
$ git submodule update
$ DUCKDB_GIT_VERSION=v1.1.3 make set_duckdb_version
$ make
mkdir -p build/release && \
	cmake  -DEXTENSION_STATIC_BUILD=1 -DDUCKDB_EXTENSION_NAMES="csv_scanner" -DDUCKDB_EXTENSION_CSV_SCANNER_PATH="./duckdb/duckdb_scanner_example/" -DDUCKDB_EXTENSION_CSV_SCANNER_SHOULD_LINK=0 -DDUCKDB_EXTENSION_CSV_SCANNER_LOAD_TESTS=1 -DDUCKDB_EXTENSION_CSV_SCANNER_TEST_PATH="./duckdb/duckdb_scanner_example/test/sql" -DOSX_BUILD_ARCH=  -DDUCKDB_EXPLICIT_PLATFORM='' -DCMAKE_BUILD_TYPE=Release -S ./duckdb/ -B build/release && \
	cmake --build build/release --config Release
-- git hash af39bd0dcf, version v1.1.3, extension folder v1.1.3
-- Extensions will be deployed to: ./duckdb/duckdb_scanner_example/build/release/repository
-- Load extension 'csv_scanner' from './duckdb/duckdb_scanner_example/'
-- Load extension 'parquet' from './duckdb/duckdb_scanner_example/duckdb/extensions' @ v1.1.3
-- Extensions linked into DuckDB: [parquet]
-- Extensions built but not linked: [csv_scanner]
-- Tests loaded for extensions: [csv_scanner]
-- Configuring done
-- Generating done
-- Build files have been written to: ./duckdb/duckdb_scanner_example/build/release
[  1%] Built target duckdb_platform_binary
[  1%] Built target duckdb_platform
...
[100%] Linking CXX executable unittest
[100%] Built target unittest
[100%] Built target imdb

$ make test
...
[1/1] (100%): test/sql/csv_scanner.test
===============================================================================
All tests passed (13 assertions in 1 test case)

$ cat data/test.csv
aaa,1,1.23
bbb,2,3.14
ccc,3,2.56

$ duckdb -unsigned
v1.1.3 19864453f7
Enter ".help" for usage hints.
D LOAD './build/release/extension/csv_scanner/csv_scanner.duckdb_extension';
D SELECT * FROM scan_csv_ex('data/test.csv', {'a': 'varchar', 'b': 'bigint', 'c': 'double'});
┌─────────┬───────┬────────┐
│    a    │   b   │   c    │
│ varchar │ int64 │ double │
├─────────┼───────┼────────┤
│ aaa     │     1 │   1.23 │
│ bbb     │     2 │   3.14 │
│ ccc     │     3 │   2.56 │
└─────────┴───────┴────────┘

D ATTACH 'file=data/test.csv relname=testrel schema={"a": "varchar", "b": "bigint", "c": "double"}' AS csv (TYPE CSV_SCANNER);
D .databases
memory:
csv:

D .tables
testrel

D SELECT * FROM csv.testrel;
┌─────────┬───────┬────────┐
│    a    │   b   │   c    │
│ varchar │ int64 │ double │
├─────────┼───────┼────────┤
│ aaa     │     1 │   1.23 │
│ bbb     │     2 │   3.14 │
│ ccc     │     3 │   2.56 │
└─────────┴───────┴────────┘
```

# External Resources

 - [Overview of implimentation information in Japanese](https://github.com/maropu/20241129_taroleo_study/blob/master/20241129_duckdb_study/duckdb_extension.md)

# Any Question?

If you have any question, please feel free to leave it on [Issues](https://github.com/maropu/duckdb_scanner_example/issues)
or Twitter ([@maropu](http://twitter.com/#!/maropu)).
