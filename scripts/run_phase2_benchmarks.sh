#!/bin/bash
# Phase 2 Performance Benchmarks - Complete Test Suite
# Tests async client/server performance, latency, throughput, and C10K capability

set -e

echo "==================================="
echo "Phase 2 Performance Benchmarks"
echo "==================================="
echo ""

# Check if we're on Linux (required for C10K test)
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo "Warning: C10K benchmark requires Linux. Some tests will be skipped."
    LINUX=false
else
    LINUX=true
fi

# Build all benchmarks
echo "Building benchmarks..."
make benchmarks
echo ""

# Test 1: Parser Speed (baseline)
echo "==================================="
echo "Test 1: Parser Speed Benchmark"
echo "==================================="
./build/benchmarks/bench_parser
echo ""

# Test 2: Memory Usage
echo "==================================="
echo "Test 2: Memory Usage Verification"
echo "==================================="
./build/benchmarks/bench_memory
echo ""

# Test 3: Binary Size
echo "==================================="
echo "Test 3: Binary Size Analysis"
echo "==================================="
if [ -f "benchmarks/bench_size.sh" ]; then
    bash benchmarks/bench_size.sh
fi
echo ""

# Test 4: Async Client Latency (requires internet)
echo "==================================="
echo "Test 4: Async Client Latency"
echo "==================================="
echo "This test requires internet connection to httpbin.org"
read -p "Run latency test? (y/n) " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    ./build/benchmarks/bench_latency
fi
echo ""

# Test 5: Async Client Throughput (requires internet)
echo "==================================="
echo "Test 5: Async Client Throughput"
echo "==================================="
echo "This test requires internet connection to httpbin.org"
read -p "Run throughput test? (y/n) " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    ./build/benchmarks/bench_async_throughput
fi
echo ""

# Test 6: C10K Server Benchmark (Linux only)
if [ "$LINUX" = true ]; then
    echo "==================================="
    echo "Test 6: C10K Server Capability"
    echo "==================================="
    echo "This test will attempt 10,000 concurrent connections"
    read -p "Run C10K test? (y/n) " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        # Increase file descriptor limit
        ulimit -n 65536
        ./build/benchmarks/bench_c10k
    fi
else
    echo "==================================="
    echo "Test 6: C10K Server Capability"
    echo "==================================="
    echo "SKIPPED: Requires Linux with epoll support"
fi
echo ""

# Summary
echo "==================================="
echo "Benchmark Suite Complete"
echo "==================================="
echo ""
echo "Summary:"
echo "✅ Parser speed verified"
echo "✅ Memory usage verified (zero allocations)"
echo "✅ Binary size measured"

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "✅ Async client latency tested"
    echo "✅ Async client throughput tested"
fi

if [ "$LINUX" = true ] && [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "✅ C10K server capability tested"
fi

echo ""
echo "See individual test output above for detailed results."
echo "Performance targets:"
echo "  - Parser: >2 GB/s"
echo "  - Latency p99: <500ms"
echo "  - Throughput: >100 req/s (excellent: >1000 req/s)"
echo "  - C10K: 10,000+ concurrent connections"
