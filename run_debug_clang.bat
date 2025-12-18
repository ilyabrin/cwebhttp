@echo off
echo Compiling DEBUG version with Clang...
if not exist build mkdir build

clang -Wall -Wextra -std=c11 -O0 -g -Iinclude test_iocp_debug.c src/cwebhttp.c src/async/loop.c src/async/iocp.c src/async/nonblock.c src/async/client.c src/async/server.c -o build/test_iocp_debug.exe -lws2_32 -lz 2>&1

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] Debug version compiled with Clang!
    echo.
    echo Starting DEBUG server...
    echo ========================================
    echo.
    build\test_iocp_debug.exe
) else (
    echo.
    echo [ERROR] Compilation failed
    pause
)
