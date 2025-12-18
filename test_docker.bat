@echo off
echo ========================================
echo Building Docker image...
echo ========================================
docker build -t cwebhttp-test .

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Docker build failed!
    exit /b 1
)

echo.
echo ========================================
echo Running Docker tests...
echo ========================================
docker run --rm cwebhttp-test

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Docker tests failed!
    exit /b 1
)

echo.
echo ========================================
echo [SUCCESS] All Docker tests passed!
echo ========================================
