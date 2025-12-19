@echo off
echo Testing build...

echo.
echo Cleaning...
make clean

echo.
echo Building examples...
make examples

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] Build completed!
    echo.
    echo Running memcheck demo:
    echo ========================================
    .\build\examples\memcheck_demo.exe
) else (
    echo.
    echo [ERROR] Build failed with error code: %ERRORLEVEL%
)

pause
