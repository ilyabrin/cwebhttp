@echo off
echo ========================================
echo Windows Async Server Test Suite
echo ========================================
echo.

cd /d %~dp0

REM Create build directory if it doesn't exist
if not exist build mkdir build

echo [1/5] Building IOCP test server...
gcc -Wall -Wextra -std=c11 -O2 -Iinclude test_iocp_server.c src/cwebhttp.c src/async/loop.c src/async/epoll.c src/async/kqueue.c src/async/iocp.c src/async/wsapoll.c src/async/select.c src/async/nonblock.c src/async/client.c src/async/server.c -o build/test_iocp_server.exe -lws2_32 -lz 2>&1

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed!
    exit /b 1
)
echo SUCCESS: Build complete
echo.

echo [2/5] Building async server example...
gcc -Wall -Wextra -std=c11 -O2 -Iinclude examples/async_server.c src/cwebhttp.c src/async/loop.c src/async/epoll.c src/async/kqueue.c src/async/iocp.c src/async/wsapoll.c src/async/select.c src/async/nonblock.c src/async/client.c src/async/server.c -o build/examples/async_server.exe -lws2_32 -lz 2>&1

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Example build failed!
    exit /b 1
)
echo SUCCESS: Example build complete
echo.

echo [3/5] Running core tests...
if exist build\tests\test_parse.exe (
    build\tests\test_parse.exe
) else (
    echo SKIP: Core tests not built yet
)
echo.

echo [4/5] Running async event loop tests...
if exist build\tests\test_async_loop.exe (
    build\tests\test_async_loop.exe
) else (
    echo SKIP: Async tests not built yet
)
echo.

echo [5/5] Starting IOCP test server...
echo Server will start on port 8080
echo.
echo ========================================
echo TEST INSTRUCTIONS:
echo 1. Server starting in 3 seconds...
echo 2. Open browser to http://localhost:8080/
echo 3. Or run: curl http://localhost:8080/
echo 4. Press Ctrl+C to stop server
echo ========================================
echo.

timeout /t 3 /nobreak >nul

build\test_iocp_server.exe
