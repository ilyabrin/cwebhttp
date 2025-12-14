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

# Build all targets
RUN make clean && make all

# Run unit tests
RUN make test

# Build and run async tests
RUN make async-tests

# Default command - run all tests including async
CMD ["/bin/bash", "-c", "echo '=== Running all unit tests ===' && make test && echo '\n=== Running async event loop tests ===' && make async-tests"]