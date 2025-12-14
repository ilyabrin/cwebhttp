FROM ubuntu:22.04

# Install build dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    libc6-dev \
    zlib1g-dev \
    telnet \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build all targets
RUN make clean && make all

# Run unit tests
RUN make test

# Build simple async test for debugging
RUN gcc -Wall -Wextra -std=c11 -O2 -Iinclude tests/test_async_simple.c src/async/loop.c src/async/epoll.c src/async/nonblock.c -o build/tests/test_async_simple || true

# Default command - run simple async test
CMD ["/bin/bash", "-c", "echo 'Running simple async test...' && ./build/tests/test_async_simple 2>&1"]