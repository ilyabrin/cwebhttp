@echo off
REM Build cwebhttp with TLS support (Windows)
REM Requires mbedTLS installed (e.g., via vcpkg or MinGW)

echo Building cwebhttp with TLS support...

REM Clean previous builds
if exist build rmdir /s /q build

REM Build with TLS enabled
mingw32-make ENABLE_TLS=1 CFLAGS="-DCWEBHTTP_ENABLE_TLS=1 -Wall -Wextra -std=c11 -O2 -Iinclude -Itests" LDFLAGS="-lws2_32 -lz -lmbedtls -lmbedx509 -lmbedcrypto"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Build successful!
    echo TLS support: ENABLED
    echo ========================================
    echo.
    echo Examples:
    echo   build\examples\https_client.exe www.example.com
    echo.
    echo Tests:
    echo   build\tests\test_tls.exe
    echo ========================================
) else (
    echo.
    echo ========================================
    echo Build FAILED!
    echo.
    echo Make sure mbedTLS is installed:
    echo   vcpkg install mbedtls
    echo   OR
    echo   pacman -S mingw-w64-x86_64-mbedtls (MSYS2)
    echo ========================================
)

pause
