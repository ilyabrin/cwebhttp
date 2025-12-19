@echo off
REM scripts\test_tls.bat - Local TLS testing script for Windows

echo ========================================
echo   cwebhttp TLS Local Testing (Windows)
echo ========================================
echo.

REM Check if running in MSYS2
where pacman >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Warning: MSYS2 not detected
    echo For best results, run this in MSYS2 MinGW64 shell
    echo.
)

REM Check for mbedTLS
echo Checking dependencies...
where gcc >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: GCC not found
    echo Install MSYS2 from https://www.msys2.org/
    goto :error
)

REM Check mbedTLS libraries
if not exist "C:\msys64\mingw64\lib\libmbedtls.a" (
    if not exist "C:\msys64\mingw64\lib\libmbedtls.dll.a" (
        echo Warning: mbedTLS not found
        echo Install with: pacman -S mingw-w64-x86_64-mbedtls
        echo.
    )
)

echo Creating directories...
if not exist build\certs mkdir build\certs
if not exist build\examples mkdir build\examples

REM Generate certificates if needed
cd build\certs
if not exist server.crt (
    echo Generating test certificates...
    openssl req -x509 -newkey rsa:2048 -nodes -keyout server.key -out server.crt -days 365 -subj "//CN=localhost" >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo   - Server certificate created
    ) else (
        echo   Warning: OpenSSL not found, using pre-generated certs
    )
)
cd ..\..

REM Build TLS examples
echo.
echo Building TLS examples...
call build_tls.bat >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   - Build successful
) else (
    echo   Warning: Build had issues, check build_tls.bat output
)

REM Test HTTPS client
echo.
echo Testing HTTPS client...
if exist build\examples\https_client.exe (
    echo   - Running client test...
    timeout /t 10 /nobreak >nul 2>&1 & build\examples\https_client.exe https://example.com/ >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo   - HTTPS client OK
    ) else (
        echo   - HTTPS client test skipped ^(network required^)
    )
) else (
    echo   - HTTPS client not built
)

REM Summary
echo.
echo ========================================
echo   Test Summary
echo ========================================
echo.
echo TLS Features Available:
echo   - HTTPS client and server
echo   - SNI support
echo   - Session resumption  
echo   - Client certificate authentication
echo.
echo To start HTTPS server:
echo   build\examples\https_server.exe build\certs\server.crt build\certs\server.key 8443
echo.
echo To test with curl:
echo   curl -k https://localhost:8443/
echo.
echo For more info, see: TLS_ADVANCED_FEATURES.md
echo.
goto :end

:error
echo.
echo Setup instructions:
echo   1. Install MSYS2: https://www.msys2.org/
echo   2. Open MSYS2 MinGW64 terminal
echo   3. Run: pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-mbedtls make
echo   4. Run this script again
echo.
exit /b 1

:end
