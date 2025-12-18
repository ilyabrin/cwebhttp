@echo off
echo ========================================
echo Building All Targets
echo ========================================

echo.
echo [1/4] Building examples...
make examples
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Examples build failed!
    exit /b 1
)

echo.
echo [2/4] Building tests...
make test
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Tests build failed!
    exit /b 1
)

echo.
echo [3/4] Building benchmarks...
make benchmarks
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Benchmarks build failed!
    exit /b 1
)

echo.
echo [4/4] Running tests...
make test
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Tests failed!
    exit /b 1
)

echo.
echo ========================================
echo [SUCCESS] All builds completed!
echo ========================================
