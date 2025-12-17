# PowerShell 7+ Test Script for IOCP Async Server
# Run with: pwsh -File Run-IOCPTest.ps1

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Windows IOCP Async Server Test" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

Set-Location $PSScriptRoot

# Create build directory
Write-Host "[1/3] Creating build directory..." -ForegroundColor Yellow
if (!(Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}
Write-Host "OK" -ForegroundColor Green
Write-Host ""

# Compile
Write-Host "[2/3] Compiling test server..." -ForegroundColor Yellow
$sourceFiles = @(
    "test_iocp_server.c",
    "src/cwebhttp.c",
    "src/async/loop.c",
    "src/async/iocp.c",
    "src/async/nonblock.c",
    "src/async/client.c",
    "src/async/server.c"
)

$gccCmd = "gcc -Wall -Wextra -std=c11 -O2 -Iinclude " + ($sourceFiles -join " ") + " -o build/test_iocp_server.exe -lws2_32 -lz"
Write-Host "Command: $gccCmd" -ForegroundColor Gray
Write-Host ""

$result = Invoke-Expression $gccCmd 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Compilation failed!" -ForegroundColor Red
    Write-Host $result -ForegroundColor Red
    Write-Host ""
    Write-Host "Please check:" -ForegroundColor Yellow
    Write-Host "- Is gcc installed? Run: gcc --version" -ForegroundColor Yellow
    Write-Host "- Is gcc in PATH?" -ForegroundColor Yellow
    Write-Host "- Are all source files present?" -ForegroundColor Yellow
    exit 1
}

Write-Host "[SUCCESS] Compilation complete!" -ForegroundColor Green
Write-Host "File: build\test_iocp_server.exe" -ForegroundColor Green
Write-Host ""

# Run server
Write-Host "[3/3] Starting test server..." -ForegroundColor Yellow
Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "Server starting on http://localhost:8080" -ForegroundColor Cyan
Write-Host ""
Write-Host "TO TEST:" -ForegroundColor White
Write-Host "  1. Keep this window open" -ForegroundColor White
Write-Host "  2. Open browser: http://localhost:8080/" -ForegroundColor White
Write-Host "  3. OR run in another terminal: curl http://localhost:8080/" -ForegroundColor White
Write-Host "  4. Expected response: " -NoNewline -ForegroundColor White
Write-Host '"Hello, IOCP!"' -ForegroundColor Green
Write-Host ""
Write-Host "Watch for: " -NoNewline -ForegroundColor White
Write-Host "[HANDLER] Got request: GET /" -ForegroundColor Green
Write-Host ""
Write-Host "Press Ctrl+C to stop server" -ForegroundColor Yellow
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

& ".\build\test_iocp_server.exe"

Write-Host ""
Write-Host "Server stopped." -ForegroundColor Yellow
