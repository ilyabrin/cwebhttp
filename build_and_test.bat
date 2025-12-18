@echo off
REM Quick build and test script for Windows

echo Building IOCP test server...
if not exist build mkdir build

gcc -Wall -Wextra -std=c11 -O2 -Iinclude ^
    test_iocp_server.c ^
    src/cwebhttp.c ^
    src/async/loop.c ^
    src/async/iocp.c ^
    src/async/nonblock.c ^
    src/async/client.c ^
    src/async/server.c ^
    -o build/test_iocp_server.exe ^
    -lws2_32 -lz

if %ERRORLEVEL% EQU 0 (
    echo Build SUCCESS!
    echo.
    echo Starting test server on port 8080...
    echo Press Ctrl+C to stop
    echo.
    build\test_iocp_server.exe
) else (
    echo Build FAILED!
    exit /b 1
)
