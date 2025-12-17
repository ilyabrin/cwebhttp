@echo off
echo ========================================
echo Compilation Diagnostics
echo ========================================
echo.

echo [Check 1] GCC Version:
gcc --version
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: gcc not found in PATH!
    echo Please install MinGW or Git for Windows
    pause
    exit /b 1
)
echo.

echo [Check 2] Current Directory:
cd
echo.

echo [Check 3] Source Files:
if exist test_iocp_server.c (echo OK: test_iocp_server.c) else (echo MISSING: test_iocp_server.c)
if exist src\cwebhttp.c (echo OK: src\cwebhttp.c) else (echo MISSING: src\cwebhttp.c)
if exist src\async\loop.c (echo OK: src\async\loop.c) else (echo MISSING: src\async\loop.c)
if exist src\async\iocp.c (echo OK: src\async\iocp.c) else (echo MISSING: src\async\iocp.c)
if exist src\async\nonblock.c (echo OK: src\async\nonblock.c) else (echo MISSING: src\async\nonblock.c)
if exist src\async\client.c (echo OK: src\async\client.c) else (echo MISSING: src\async\client.c)
if exist src\async\server.c (echo OK: src\async\server.c) else (echo MISSING: src\async\server.c)
echo.

echo [Check 4] Attempting compilation with verbose output:
echo Command: gcc -v -Wall -Wextra -std=c11 -O2 -Iinclude test_iocp_server.c src/cwebhttp.c src/async/loop.c src/async/iocp.c src/async/nonblock.c src/async/client.c src/async/server.c -o build/test_iocp_server.exe -lws2_32 -lz
echo.

if not exist build mkdir build

gcc -v -Wall -Wextra -std=c11 -O2 -Iinclude test_iocp_server.c src/cwebhttp.c src/async/loop.c src/async/iocp.c src/async/nonblock.c src/async/client.c src/async/server.c -o build/test_iocp_server.exe -lws2_32 -lz 2>&1

echo.
echo ========================================
if exist build\test_iocp_server.exe (
    echo [SUCCESS] Executable created!
    echo File: build\test_iocp_server.exe
    dir build\test_iocp_server.exe
) else (
    echo [FAILED] Executable not created
    echo.
    echo Please copy the error messages above and share them.
)
echo ========================================
pause
