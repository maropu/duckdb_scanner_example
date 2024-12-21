 - Start a http server for duckdb-wasm:

```
$ docker build --tag duckdb-wasm .
$ docker create -p 8080:8080 --name duckdb-wasm-app duckdb-wasm
$ docker cp csv_scanner.duckdb_extension.wasm duckdb-wasm-app:/workspace/duckdb-wasm/packages/duckdb-wasm-app/build/release/extension_repository/v1.1.1/wasm_eh
$ docker start duckdb-wasm-app
```

 - Access http://127.0.0.1:8080/ and then load the extension as follows:

 ```sql
DuckDB Web Shell
Database: v1.1.1
Package:  @duckdb/duckdb-wasm@1.11.0

Connected to a local transient in-memory database.
Enter .help for usage hints.

duckdb> SET custom_extension_repository='http://127.0.0.1:8080/extension_repository';
duckdb> LOAD csv_scanner;
duckdb> .files add
duckdb> SELECT * FROM scan_csv_ex('test.csv', {'a': 'varchar', 'b': 'bigint', 'c': 'double'});
┌─────────┬───────┬────────┐
│    a    │   b   │   c    │
│ varchar │ int64 │ double │
├─────────┼───────┼────────┤
│ aaa     │     1 │   1.23 │
│ bbb     │     2 │   3.14 │
│ ccc     │     3 │   2.56 │
└─────────┴───────┴────────┘
```
