#!/bin/bash
# Test script for async HTTP server in Docker (Linux environment)

echo "========================================="
echo "Async Server Test - Linux/Docker"
echo "========================================="
echo ""

# Server already built during Docker build
# Start server in background
echo "Starting async server..."
./build/examples/async_server &
SERVER_PID=$!

# Wait for server to start
sleep 2

echo ""
echo "========================================="
echo "Test 1: GET / (Homepage)"
echo "========================================="
curl -s -m 5 http://localhost:8080/ 2>&1 | head -10
echo ""

echo "========================================="
echo "Test 2: GET /api/hello (JSON API)"
echo "========================================="
curl -s -m 5 http://localhost:8080/api/hello 2>&1
echo ""
echo ""

echo "========================================="
echo "Test 3: GET /api/users (JSON List)"
echo "========================================="
curl -s -m 5 http://localhost:8080/api/users 2>&1
echo ""
echo ""

echo "========================================="
echo "Test 4: POST /api/echo (Echo Body)"
echo "========================================="
curl -s -m 5 -X POST http://localhost:8080/api/echo \
  -H "Content-Type: application/json" \
  -d '{"test":"data","number":42}' 2>&1
echo ""
echo ""

echo "========================================="
echo "Test 5: Concurrent Requests (5 parallel)"
echo "========================================="
for i in {1..5}; do
  curl -s -m 5 http://localhost:8080/api/hello 2>&1 &
done
wait
echo "All 5 requests completed"
echo ""

# Kill server
echo "Stopping server..."
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo ""
echo "========================================="
echo "All tests completed successfully!"
echo "========================================="
