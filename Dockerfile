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

# Make test script executable
RUN chmod +x test_async_server_docker.sh

# Default command - run async server tests
CMD ["/bin/bash", "./test_async_server_docker.sh"]