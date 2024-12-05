.PHONY: all clean format debug release pull

all: release

DUCKDB_VERSION = 1.1.3

MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
PROJ_DIR := $(dir $(MKFILE_PATH))

ifeq ($(OS),Windows_NT)
	TEST_PATH="/test/Release/unittest.exe"
else
	TEST_PATH="/test/unittest"
endif

#### OSX config
OSX_BUILD_FLAG=
ifneq (${OSX_BUILD_ARCH}, "")
	OSX_BUILD_FLAG=-DOSX_BUILD_ARCH=${OSX_BUILD_ARCH}
endif

#### VCPKG config
VCPKG_TOOLCHAIN_PATH?=
ifneq ("${VCPKG_TOOLCHAIN_PATH}", "")
	TOOLCHAIN_FLAGS:=${TOOLCHAIN_FLAGS} -DVCPKG_MANIFEST_DIR='${PROJ_DIR}' -DVCPKG_BUILD=1 -DCMAKE_TOOLCHAIN_FILE='${VCPKG_TOOLCHAIN_PATH}'
endif
ifneq ("${VCPKG_TARGET_TRIPLET}", "")
	TOOLCHAIN_FLAGS:=${TOOLCHAIN_FLAGS} -DVCPKG_TARGET_TRIPLET='${VCPKG_TARGET_TRIPLET}'
endif

#### Enable Ninja as generator
ifeq ($(GEN),ninja)
	GENERATOR=-G "Ninja" -DFORCE_COLORED_OUTPUT=1
endif

#### Configuration for this extension
EXTENSION_NAME=CSV_SCANNER
EXTENSION_VERSION=v1.0.0
EXTENSION_FLAGS=\
-DDUCKDB_EXTENSION_NAMES="csv_scanner" \
-DDUCKDB_EXTENSION_${EXTENSION_NAME}_PATH="$(PROJ_DIR)" \
-DDUCKDB_EXTENSION_${EXTENSION_NAME}_SHOULD_LINK=0 \
-DDUCKDB_EXTENSION_${EXTENSION_NAME}_LOAD_TESTS=1 \
-DDUCKDB_EXTENSION_${EXTENSION_NAME}_TEST_PATH="$(PROJ_DIR)test/sql" \
-DDUCKDB_EXTENSION_${EXTENSION_NAME}_EXT_VERSION="$(EXTENSION_VERSION)"

BUILD_FLAGS=-DEXTENSION_STATIC_BUILD=1 $(EXTENSION_FLAGS) $(OSX_BUILD_FLAG) $(TOOLCHAIN_FLAGS) -DDUCKDB_EXPLICIT_PLATFORM='${DUCKDB_PLATFORM}'

duckdb:
	./duckconfigure $(DUCKDB_VERSION)

clean:
	rm -rf build
	[ -d "duckdb" ] && (cd duckdb && make clean)

# Main build
debug: duckdb
	mkdir -p  build/debug && \
	cmake $(GENERATOR) $(BUILD_FLAGS) -DCMAKE_BUILD_TYPE=Debug -S ./duckdb/ -B build/debug && \
	cmake --build build/debug --config Debug

reldebug: duckdb
	mkdir -p build/reldebug && \
	cmake $(GENERATOR) $(BUILD_FLAGS) -DCMAKE_BUILD_TYPE=RelWithDebInfo -S ./duckdb/ -B build/reldebug && \
	cmake --build build/reldebug --config RelWithDebInfo

release: duckdb
	mkdir -p build/release && \
	cmake $(GENERATOR) $(BUILD_FLAGS) -DCMAKE_BUILD_TYPE=Release -S ./duckdb/ -B build/release && \
	cmake --build build/release --config Release

test: test_release
test_release: release
	./build/release/$(TEST_PATH) --test-dir "$(PROJ_DIR)" "test/*"
test_debug: debug
	./build/debug/$(TEST_PATH) --test-dir "$(PROJ_DIR)" "test/*"

format:
	cp duckdb/.clang-format .
	find src/ -iname "*.hpp" -o -iname "*.cpp" | xargs clang-format --sort-includes=0 -style=file -i
	cmake-format -i CMakeLists.txt
	rm .clang-format
