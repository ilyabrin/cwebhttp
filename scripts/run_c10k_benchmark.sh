#!/bin/bash
# C10K Benchmark Runner Script for Linux
# Automatically configures system limits and runs the benchmark

set -e

echo "=== cwebhttp C10K Benchmark Runner ==="
echo "Preparing system for C10K testing..."

# Check if running on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo "❌ C10K benchmark requires Linux"
    exit 1
fi

# Store original limits
ORIG_NOFILE=$(ulimit -n)
ORIG_NPROC=$(ulimit -u)

echo "Original limits:"
echo "  File descriptors: $ORIG_NOFILE"
echo "  Processes: $ORIG_NPROC"

# Function to restore limits on exit
cleanup() {
    echo "Restoring original limits..."
    ulimit -n $ORIG_NOFILE 2>/dev/null || true
    ulimit -u $ORIG_NPROC 2>/dev/null || true
}
trap cleanup EXIT

# Increase system limits for C10K testing
echo "Configuring system for C10K..."

# Increase file descriptor limit
ulimit -n 65536 2>/dev/null || {
    echo "⚠️  Could not increase file descriptor limit to 65536"
    echo "   Current limit: $(ulimit -n)"
}

# Show current limits
echo "Current limits:"
echo "  File descriptors: $(ulimit -n)"
echo "  Processes: $(ulimit -u)"

# Check if benchmark binary exists
BENCHMARK_BIN="./build/benchmarks/bench_c10k"
if [[ ! -f "$BENCHMARK_BIN" ]]; then
    echo "Building C10K benchmark..."
    make benchmarks
fi

if [[ ! -f "$BENCHMARK_BIN" ]]; then
    echo "❌ Failed to build benchmark binary"
    exit 1
fi

# Run the benchmark
echo ""
echo "Starting C10K benchmark..."
echo "Testing 10,000+ concurrent connections with epoll..."
echo ""

$BENCHMARK_BIN

echo ""
echo "✅ C10K benchmark completed"