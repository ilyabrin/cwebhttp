@echo off
REM Direct test execution for IOCP async server
echo ========================================
echo Windows IOCP Async Server Test
echo ========================================
echo.


echo [1/3] Creating build directory...
if not exist build mkdir build
echo OK
echo.

echo [2/3] Compiling test server...
echo Command: gcc -Wall -Wextra -std=c11 -O2 -Iinclude test_iocp_server.c src/cwebhttp.c src/async/loop.c src/async/iocp.c src/async/nonblock.c src/async/client.c src/async/server.c -o build/test_iocp_server.exe -lws2_32 -lz
echo.

gcc -Wall -Wextra -std=c11 -O2 -Iinclude test_iocp_server.c src/cwebhttp.c src/async/loop.c src/async/iocp.c src/async/nonblock.c src/async/client.c src/async/server.c -o build/test_iocp_server.exe -lws2_32 -lz

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Compilation failed! Error code: %ERRORLEVEL%
    echo.
    echo Please check:
    echo - Is gcc installed? Run: gcc --version
    echo - Is gcc in PATH?
    echo - Are all source files present?
    echo.
    pause
    exit /b 1
)

echo.
echo [SUCCESS] Compilation complete!
echo File: build\test_iocp_server.exe
echo.

echo [3/3] Starting test server...
echo.
echo ============================================
echo Server starting on http://localhost:8080
echo.
echo TO TEST:
echo   1. Keep this window open
echo   2. Open browser: http://localhost:8080/
echo   3. OR run in another terminal: curl http://localhost:8080/
echo   4. Expected response: "Hello, IOCP!"
echo.
echo Watch for: [HANDLER] Got request: GET /
echo.
echo Press Ctrl+C to stop server
echo ============================================
echo.

build\test_iocp_server.exe

echo.
echo Server stopped.
pause
