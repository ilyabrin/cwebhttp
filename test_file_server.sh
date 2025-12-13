#!/bin/bash

# Start file server in background
./build/examples/file_server.exe 7777 ./www &
PID=$!
sleep 2

echo "Testing HTML file (index.html):"
curl -s http://localhost:7777/ | head -3

echo ""
echo "Testing JSON file:"
curl -s http://localhost:7777/data.json

echo ""
echo "Testing CSS file:"
curl -s http://localhost:7777/style.css

echo ""
echo "Testing text file:"
curl -s http://localhost:7777/test.txt | head -2

echo ""
echo "Testing 404 (non-existent file):"
curl -s -o /dev/null -w "HTTP Status: %{http_code}\n" http://localhost:7777/notfound.html

echo ""
echo "Testing MIME types with headers:"
curl -s -I http://localhost:7777/data.json | grep -i "content-type"

# Kill server
kill $PID 2>/dev/null
wait $PID 2>/dev/null

echo ""
echo "Test complete!"
