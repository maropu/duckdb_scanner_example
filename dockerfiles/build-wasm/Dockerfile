# Use a base image for dependencies
FROM ubuntu:24.04 AS base

WORKDIR /workspace

ARG DUCKDB_VERSION=v1.2.0

# Install necessary basic packages with caching
RUN --mount=type=cache,id=duckdb-ext-apt,target=/var/cache/apt \
    apt-get update && apt-get install -y \
    build-essential \
    libssl-dev \
    python3 \
    ccache \
    git \
    wget \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

# Install cmake with caching
RUN --mount=type=cache,target=/root/.ccache-cmake \
    wget https://github.com/Kitware/CMake/releases/download/v3.30.0/cmake-3.30.0.tar.gz && \
    tar -xzf cmake-3.30.0.tar.gz && \
    cd cmake-3.30.0 && \
    CCACHE_DIR=/root/.ccache-cmake ./bootstrap --parallel=4 --enable-ccache && \
    CCACHE_DIR=/root/.ccache-cmake make && \
    make install

# Install emscripten with caching
RUN git clone https://github.com/emscripten-core/emsdk.git && \
    cd emsdk && \
    ./emsdk install 3.1.57 && \
    ./emsdk activate 3.1.57

# Build stage for the extension
FROM base AS build

WORKDIR /workspace

# Bind mount extension code into the image and compile the extension
RUN --mount=type=bind,source=./Makefile,target=/workspace/duckdb_scanner_example/Makefile \
    --mount=type=bind,source=./extension_config.cmake,target=/workspace/duckdb_scanner_example/extension_config.cmake \
    --mount=type=bind,source=./CMakeLists.txt,target=/workspace/duckdb_scanner_example/CMakeLists.txt \
    --mount=type=bind,source=./makefiles,target=/workspace/duckdb_scanner_example/makefiles \
    --mount=type=bind,source=./src,target=/workspace/duckdb_scanner_example/src \
    --mount=type=cache,target=/root/.ccache-duckdb-ext \
    cd duckdb_scanner_example && \
    git clone https://github.com/duckdb/duckdb.git && \
    DUCKDB_GIT_VERSION=${DUCKDB_VERSION} make set_duckdb_version && \
    CCACHE_DIR=/root/.ccache-duckdb-ext \
    PATH=$PATH:/workspace/emsdk/upstream/emscripten \
    make wasm_eh

# Final stage for a smaller image
FROM ubuntu:24.04 AS final

WORKDIR /workspace

# Copy necessary files from the build stage
COPY --from=build /workspace/duckdb_scanner_example/build/wasm_eh/extension/csv_scanner/csv_scanner.duckdb_extension.wasm .

# The final image contains only the compiled artifact
CMD ["/bin/bash"]
