@echo off
echo ========================================
echo Testing New Examples Build
echo ========================================
echo.

echo [1/4] Cleaning build directory...
make clean

echo.
echo [2/4] Building all examples...
make examples

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo [3/4] Checking if new examples were built...

set ALL_OK=1

if exist "build\examples\json_api_server.exe" (
    echo   [OK] json_api_server.exe
) else (
    echo   [FAIL] json_api_server.exe not found
    set ALL_OK=0
)

if exist "build\examples\static_file_server.exe" (
    echo   [OK] static_file_server.exe
) else (
    echo   [FAIL] static_file_server.exe not found
    set ALL_OK=0
)

if exist "build\examples\benchmark_client.exe" (
    echo   [OK] benchmark_client.exe
) else (
    echo   [FAIL] benchmark_client.exe not found
    set ALL_OK=0
)

echo.
if %ALL_OK%==1 (
    echo [SUCCESS] All new examples built successfully!
    echo.
    echo ========================================
    echo You can now run:
    echo   .\build\examples\json_api_server.exe
    echo   .\build\examples\static_file_server.exe
    echo   .\build\examples\benchmark_client.exe http://localhost:8080/ 100 10
    echo ========================================
) else (
    echo [ERROR] Some examples failed to build
)

echo.
pause
