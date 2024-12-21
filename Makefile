PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Configuration of extension
EXT_NAME=CSV_SCANNER
EXT_VERSION=v1.0.0
EXT_CONFIG=${PROJ_DIR}extension_config.cmake
EXT_FLAGS=\
-DDUCKDB_EXTENSION_NAMES="$(shell echo $(EXT_NAME) | tr 'A-Z' 'a-z')" \
-DDUCKDB_EXTENSION_${EXT_NAME}_PATH="$(PROJ_DIR)" \
-DDUCKDB_EXTENSION_${EXT_NAME}_SHOULD_LINK=0 \
-DDUCKDB_EXTENSION_${EXT_NAME}_LOAD_TESTS=1 \
-DDUCKDB_EXTENSION_${EXT_NAME}_TEST_PATH="$(PROJ_DIR)test/sql" \
-DDUCKDB_EXTENSION_${EXT_NAME}_EXT_VERSION="$(EXT_VERSION)"

# Include the Makefile from extension-ci-tools
include makefiles/duckdb_extension.Makefile
