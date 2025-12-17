FROM ubuntu:22.04

# Install build dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    libc6-dev \
    zlib1g-dev \
    telnet \
    curl \
    gdb \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build all targets including benchmarks
RUN make clean && make all && make benchmarks

# Run unit tests during build
RUN make test

# Make test scripts executable
RUN chmod +x test_async_server_docker.sh || true

# Copy and set permissions for C10K benchmark script (if exists)
COPY scripts/run_c10k_benchmark.sh /app/scripts/run_c10k_benchmark.sh 2>/dev/null || true
RUN chmod +x /app/scripts/run_c10k_benchmark.sh 2>/dev/null || true

# Default command: run async tests for CI
CMD ["sh", "-c", "make async-tests && echo 'All Docker tests passed!'"]