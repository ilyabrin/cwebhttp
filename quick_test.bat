@echo off
REM Quick IOCP Test - Single command to build and run

echo ============================================
echo IOCP Async Server Quick Test
echo ============================================
echo.

REM Ensure build directory exists
if not exist build mkdir build

echo [Step 1] Compiling test server...
gcc -Wall -Wextra -std=c11 -O2 -Iinclude test_iocp_server.c src/cwebhttp.c src/async/loop.c src/async/iocp.c src/async/nonblock.c src/async/client.c src/async/server.c -o build/test_iocp_server.exe -lws2_32 -lz 2>&1

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Compilation failed!
    echo Please check that gcc is installed and in PATH
    pause
    exit /b 1
)

echo [SUCCESS] Compilation complete!
echo.
echo [Step 2] Starting IOCP test server...
echo.
echo ============================================
echo Server will start on http://localhost:8080
echo.
echo TO TEST:
echo   1. Open browser: http://localhost:8080/
echo   2. Or run: curl http://localhost:8080/
echo   3. Expected: "Hello, IOCP!"
echo.
echo Press Ctrl+C to stop server
echo ============================================
echo.

build\test_iocp_server.exe
