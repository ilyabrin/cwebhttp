@echo off
REM Build script for different cwebhttp configurations
REM Demonstrates binary size optimization with feature flags

echo ========================================
echo cwebhttp Build Configuration Script
echo ========================================
echo.

if "%1"=="" (
    echo Usage: build_config.bat [config]
    echo.
    echo Available configurations:
    echo   full      - All features enabled ^(default^)
    echo   minimal   - Minimal build ~20KB
    echo   client    - Client-only build ~40KB
    echo   server    - Server-only build ~40KB
    echo   notls     - All features except TLS
    echo   custom    - Custom configuration ^(edit script^)
    echo.
    goto :eof
)

set "CONFIG=%1"
set "BUILD_DIR=build\%CONFIG%"
set "CFLAGS=-Wall -Wextra -std=gnu17 -O2 -Iinclude -Itests"
set "LDFLAGS=-lws2_32 -lz"
set "SRCS=src/cwebhttp.c src/memcheck.c src/log.c src/error.c"

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Configure based on build type
if "%CONFIG%"=="minimal" (
    echo Building MINIMAL configuration...
    set "CFLAGS=%CFLAGS% -DCWEBHTTP_BUILD_MINIMAL"
    set "SRCS=src/cwebhttp.c"
) else if "%CONFIG%"=="client" (
    echo Building CLIENT-ONLY configuration...
    set "CFLAGS=%CFLAGS% -DCWEBHTTP_BUILD_CLIENT_ONLY"
) else if "%CONFIG%"=="server" (
    echo Building SERVER-ONLY configuration...
    set "CFLAGS=%CFLAGS% -DCWEBHTTP_BUILD_SERVER_ONLY"
) else if "%CONFIG%"=="notls" (
    echo Building NO-TLS configuration...
    set "CFLAGS=%CFLAGS% -DCWEBHTTP_ENABLE_TLS=0"
) else if "%CONFIG%"=="custom" (
    echo Building CUSTOM configuration...
    REM Add your custom flags here
    set "CFLAGS=%CFLAGS% -DCWEBHTTP_ENABLE_WEBSOCKET=0"
    set "CFLAGS=%CFLAGS% -DCWEBHTTP_ENABLE_MEMCHECK=0"
) else if "%CONFIG%"=="full" (
    echo Building FULL configuration...
    REM All features enabled by default
) else (
    echo Unknown configuration: %CONFIG%
    goto :eof
)

echo.
echo Compiler flags: %CFLAGS%
echo Linker flags: %LDFLAGS%
echo.

REM Build minimal server example
echo Building minimal_server...
gcc %CFLAGS% examples/minimal_server.c %SRCS% -o "%BUILD_DIR%/minimal_server.exe" %LDFLAGS%

if errorlevel 1 (
    echo Build failed!
    goto :eof
)

REM Show binary size
echo.
echo ========================================
echo Binary Size:
for %%F in ("%BUILD_DIR%\minimal_server.exe") do echo   %%~nxF: %%~zF bytes ^(%%~zF / 1024 = %%~zF KB^)
echo ========================================

REM Strip symbols for final size
echo.
echo Stripping symbols...
strip "%BUILD_DIR%/minimal_server.exe"
echo.
echo Final size after strip:
for %%F in ("%BUILD_DIR%\minimal_server.exe") do echo   %%~nxF: %%~zF bytes ^(%%~zF / 1024 KB^)
echo.

echo Build complete: %BUILD_DIR%/minimal_server.exe
