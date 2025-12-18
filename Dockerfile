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
RUN make all || (echo "Build failed!" && exit 1)
RUN make benchmarks || (echo "Benchmark build failed!" && exit 1)

# Run unit tests during build (not integration tests which need internet)
RUN make test || (echo "Tests failed!" && exit 1)

# Try integration tests but don't fail build if they fail (network dependent)
RUN make integration || echo "Warning: Integration tests skipped (network required)"

# Make test scripts executable
RUN chmod +x test_async_server_docker.sh || true
RUN chmod +x scripts/*.sh || true

# Default command: run async tests for CI
CMD ["sh", "-c", "make async-tests && echo 'All Docker tests passed!'"]