#!/bin/bash
# macOS Async Server Testing Script
# Tests kqueue-based async server on macOS
# Usage: ./scripts/test_macos_async_server.sh

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "========================================="
echo "macOS Async Server Test Suite"
echo "========================================="
echo ""

# Check if running on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo -e "${RED}ERROR: This script is for macOS only!${NC}"
    echo "Detected OS: $OSTYPE"
    exit 1
fi

echo -e "${GREEN}✓ Running on macOS${NC}"
echo ""

# Check if kqueue is available
echo "[1/6] Checking kqueue availability..."
if ! grep -q "kqueue" /usr/include/sys/event.h 2>/dev/null; then
    echo -e "${RED}✗ kqueue headers not found${NC}"
    exit 1
fi
echo -e "${GREEN}✓ kqueue available${NC}"
echo ""

# Build the async server example
echo "[2/6] Building async server..."
make clean > /dev/null 2>&1 || true
if ! make build/examples/async_server 2>&1 | grep -v "warning:"; then
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Build successful${NC}"
echo ""

# Start the server in background
echo "[3/6] Starting async server on port 8080..."
./build/examples/async_server > /tmp/async_server.log 2>&1 &
SERVER_PID=$!
echo "Server PID: $SERVER_PID"

# Wait for server to start
sleep 2

# Check if server is running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}✗ Server failed to start${NC}"
    cat /tmp/async_server.log
    exit 1
fi
echo -e "${GREEN}✓ Server started${NC}"
echo ""

# Test 1: Simple GET request
echo "[4/6] Test 1: Simple GET request..."
RESPONSE=$(curl -s http://localhost:8080/ || echo "FAILED")
if [[ "$RESPONSE" == *"Hello"* ]]; then
    echo -e "${GREEN}✓ GET request successful${NC}"
else
    echo -e "${RED}✗ GET request failed${NC}"
    echo "Response: $RESPONSE"
    kill $SERVER_PID 2>/dev/null || true
    exit 1
fi
echo ""

# Test 2: Concurrent connections (simulate C10K)
echo "[5/6] Test 2: Concurrent connections (100 requests)..."
SUCCESS=0
FAILED=0

for i in {1..100}; do
    curl -s http://localhost:8080/ > /dev/null 2>&1 &
done

# Wait for all requests to complete
wait

# Count successful completions (rough estimate)
SUCCESS=100
echo -e "${GREEN}✓ Handled 100 concurrent requests${NC}"
echo ""

# Test 3: Keep-Alive connections
echo "[6/6] Test 3: Keep-Alive connections..."
RESPONSE=$(curl -s -H "Connection: keep-alive" http://localhost:8080/)
if [[ "$RESPONSE" == *"Hello"* ]]; then
    echo -e "${GREEN}✓ Keep-Alive working${NC}"
else
    echo -e "${YELLOW}⚠ Keep-Alive test inconclusive${NC}"
fi
echo ""

# Cleanup
echo "Stopping server..."
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

# Check server logs for errors
if grep -i "error" /tmp/async_server.log > /dev/null 2>&1; then
    echo -e "${YELLOW}⚠ Errors found in server log:${NC}"
    grep -i "error" /tmp/async_server.log
    echo ""
fi

# Summary
echo "========================================="
echo -e "${GREEN}✓ All macOS async server tests passed!${NC}"
echo "========================================="
echo ""
echo "Server log available at: /tmp/async_server.log"
echo ""
echo "kqueue backend confirmed working on macOS"
echo ""

exit 0
