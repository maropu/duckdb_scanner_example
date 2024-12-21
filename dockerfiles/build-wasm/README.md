```shell
$ docker build --tag duckdb-build-wasm ../.. -f ./Dockerfile
$ docker create --name temp duckdb-build-wasm
$ docker cp temp:/workspace/csv_scanner.duckdb_extension.wasm .
$ docker rm temp
```
