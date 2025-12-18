#!/bin/bash
# Simple async server test with select backend

echo "Starting async server (select backend)..."
timeout 15 ./build/examples/async_server_select.exe &
SERVER_PID=$!

# Wait for server to start
sleep 3

echo ""
echo "Test 1: GET /"
curl -s -m 5 http://localhost:8080/ 2>&1 | head -5
echo ""

echo "Test 2: GET /api/hello"
curl -s -m 5 http://localhost:8080/api/hello 2>&1
echo ""

echo "Test 3: POST /api/echo"
curl -s -m 5 -X POST http://localhost:8080/api/echo -d '{"test":"data"}' 2>&1
echo ""

# Kill server
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo "Tests complete"
