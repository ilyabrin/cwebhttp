#!/bin/bash
set -e  # Exit on error
# Run C10K benchmark in Docker with proper configuration

echo "=== Running C10K Benchmark in Docker ==="

# Build Docker image
echo "Building Docker image..."
docker build -t cwebhttp-c10k .

# Run C10K benchmark with increased limits
echo "Starting C10K benchmark container..."
docker run --rm \
    --ulimit nofile=65536:65536 \
    --ulimit nproc=32768:32768 \
    --memory=2g \
    --cpus=2 \
    -w /app \
    cwebhttp-c10k \
    /bin/bash -c "
        echo 'Container limits:';
        ulimit -n;
        ulimit -u;
        echo 'Running C10K benchmark...';
        ./build/benchmarks/bench_c10k
    "

echo "C10K benchmark completed!"