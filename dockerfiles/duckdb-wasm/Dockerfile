FROM  ubuntu:24.04

WORKDIR /workspace

# Install necessary basic packages
RUN apt-get update && apt-get install -y \
    build-essential \
    python3 \
    libssl-dev \
    ccache \
    git \
    curl \
    wget \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

# Install yarn
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

# Install cmake
RUN wget https://github.com/Kitware/CMake/releases/download/v3.30.0/cmake-3.30.0.tar.gz && \
  tar -xzf cmake-3.30.0.tar.gz && \
  cd cmake-3.30.0 && ./bootstrap && make && make install

# Install emscripten
RUN git clone https://github.com/emscripten-core/emsdk.git && \
  cd emsdk && \
  ./emsdk install 3.1.57 && \
  ./emsdk activate 3.1.57

# Compile duckdb-wasm
RUN git clone https://github.com/duckdb/duckdb-wasm.git && \
  PATH=$PATH:/workspace/emsdk/upstream/emscripten && \
  . $HOME/.cargo/env && \
  cd duckdb-wasm && \
  git checkout v1.29.0 && \
  git submodule init && \
  git submodule update && \
  make apply_patches && \
  sed -i 's/bool allow_unsigned_extensions = false;/bool allow_unsigned_extensions = true;/g' ./submodules/duckdb/src/include/duckdb/main/config.hpp && \
  sed -i 's/.allow_unsigned_extensions = false,/.allow_unsigned_extensions = true,/g' ./lib/src/config.cc && \
  sed -i 's/bool allow_unsigned_extensions = false;/bool allow_unsigned_extensions = true;/g' ./lib/include/duckdb/web/config.h && \
  make wasm_relsize && \
  make build_loadable && \
  make serve_loadable_base

# Run duckdb-wasm app server
ENTRYPOINT ["python3", "-m", "http.server"]
CMD ["8080", "--directory", "./duckdb-wasm/packages/duckdb-wasm-app/build/release/"]
