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

# Run unit tests
RUN make test

# Build and run async tests
RUN make async-tests

# Make test script executable
RUN chmod +x test_async_server_docker.sh

# Copy and set permissions for C10K benchmark script
COPY scripts/run_c10k_benchmark.sh /app/scripts/run_c10k_benchmark.sh
RUN chmod +x /app/scripts/run_c10k_benchmark.sh

# Set default command to run C10K benchmark
CMD ["/bin/bash", "/app/scripts/run_c10k_benchmark.sh"]