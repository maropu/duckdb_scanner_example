# Use a base image for dependencies
FROM ubuntu:24.04 AS base

WORKDIR /workspace

ARG DUCKDB_WASM_PTHREAD=0
ARG DUCKDB_WASM_NUM_THREADS=4

# Install necessary basic packages with caching
RUN --mount=type=cache,id=duckdb-wasm-apt,target=/var/cache/apt \
    apt-get update && apt-get install -y \
    build-essential \
    python3 \
    libssl-dev \
    ccache \
    git \
    curl \
    wget \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

# Install nodejs(v21.7.3) & yarn
RUN curl -fsSL https://deb.nodesource.com/setup_21.x | bash - && \
  apt-get install -y nodejs && \
  curl -sS https://dl.yarnpkg.com/debian/pubkey.gpg | apt-key add - && \
  echo "deb https://dl.yarnpkg.com/debian/ stable main" | tee /etc/apt/sources.list.d/yarn.list && \
  apt-get update && apt-get install -y yarn=1.22.22-1

# Install rust
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y && \
  . $HOME/.cargo/env && \
  rustup install 1.83.0 && \
  rustup default 1.83.0

# Install cmake with caching
RUN --mount=type=cache,target=/root/.ccache-cmake \
    wget https://github.com/Kitware/CMake/releases/download/v3.30.0/cmake-3.30.0.tar.gz && \
    tar -xzf cmake-3.30.0.tar.gz && \
    cd cmake-3.30.0 && \
    CCACHE_DIR=/root/.ccache-cmake && \
    ./bootstrap --parallel=4 --enable-ccache && \
    make && \
    make install

# Install emscripten
RUN git clone https://github.com/emscripten-core/emsdk.git && \
  cd emsdk && \
  ./emsdk install 3.1.57 && \
  ./emsdk activate 3.1.57

# TODO: This is workaround to bump up nodejs from v20.18.0 to v21.7.3
# In case of v20.18.0, SEGV can happen when compiling duckdb-wasm
RUN cd emsdk/node/20.18.0_64bit/bin && rm node && ln -s /usr/bin/node node

# Build stage for the extension
FROM base AS build

WORKDIR /workspace

# Preprocess duckdb-wasm (& Make unsigned extensions loadable)
RUN git clone https://github.com/duckdb/duckdb-wasm.git && \
  cd duckdb-wasm && \
  git checkout 41c03bef54cbc310ba2afa2e5f71f7a22b38364f && \
  git submodule init && \
  git submodule update && \
  make apply_patches && \
  sed -i 's/bool allow_unsigned_extensions = false;/bool allow_unsigned_extensions = true;/g' submodules/duckdb/src/include/duckdb/main/config.hpp && \
  sed -i 's/.allow_unsigned_extensions = false,/.allow_unsigned_extensions = true,/g' lib/src/config.cc && \
  sed -i 's/bool allow_unsigned_extensions = false;/bool allow_unsigned_extensions = true;/g' lib/include/duckdb/web/config.h

# Enable pthread when DUCKDB_WASM_PTHREAD=1
RUN if [ "$DUCKDB_WASM_PTHREAD" = "1" ]; then \
    cd duckdb-wasm && \
    sed -i 's/TARGET=eh/TARGET=coi/g' Makefile && \
    sed -i "s/-sPTHREAD_POOL_SIZE=4/-sPTHREAD_POOL_SIZE=${DUCKDB_WASM_NUM_THREADS}/g" lib/CMakeLists.txt; \
  fi

# Compile duckdb-wasm
RUN --mount=type=cache,target=/root/.ccache-duckdb-wasm \
  cd duckdb-wasm && \
  EMSDK_NODE=/usr/bin/node && \
  PATH=$PATH:/workspace/emsdk/upstream/emscripten && \
  CCACHE_DIR=/root/.ccache-duckdb-wasm && \
  . $HOME/.cargo/env && \
  make wasm_relsize && \
  make build_loadable && \
  make serve_loadable_base

# Final stage for a smaller image
FROM ubuntu:24.04 AS final

WORKDIR /workspace

# Copy necessary files from the build stage
COPY --from=build /workspace/duckdb-wasm/packages/duckdb-wasm-app/build/release duckdb-wasm/release
COPY --from=build /workspace/emsdk /workspace/emsdk

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
        python3 \
        nodejs=18.19.1+dfsg-6ubuntu5 \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Run duckdb-wasm app server
ENTRYPOINT ["./emsdk/upstream/emscripten/emrun"]
CMD ["--no_browser", "--port", "8080", "duckdb-wasm/release"]
