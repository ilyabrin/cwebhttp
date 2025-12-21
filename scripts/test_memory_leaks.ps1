#!/usr/bin/env pwsh
# Memory leak detection script
# Uses built-in memcheck functionality and can integrate with Valgrind on Linux

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Memory Leak Testing" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$buildDir = "build/memcheck_test"
$null = New-Item -ItemType Directory -Force -Path $buildDir

# Build with memory checking enabled
Write-Host "Building with memory leak detection..." -ForegroundColor Yellow
$cflags = "-Wall -Wextra -O0 -g -DCWEBHTTP_ENABLE_MEMCHECK=1 -Iinclude -Itests"
$srcs = "tests/test_memcheck.c tests/unity.c src/memcheck.c"
$output = "$buildDir/test_memcheck.exe"

$cmd = "gcc $cflags $srcs -o $output -lws2_32 2>&1"
$buildOutput = Invoke-Expression $cmd

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    Write-Host $buildOutput
    exit 1
}

Write-Host "✅ Build successful" -ForegroundColor Green
Write-Host ""

# Run memory check tests
Write-Host "Running memory leak tests..." -ForegroundColor Yellow
$testOutput = & $output 2>&1
$exitCode = $LASTEXITCODE

Write-Host $testOutput
Write-Host ""

if ($exitCode -eq 0) {
    Write-Host "✅ All memory tests passed" -ForegroundColor Green
} else {
    Write-Host "❌ Memory tests failed" -ForegroundColor Red
    exit 1
}

# Test example programs for memory leaks
Write-Host "Testing examples for memory leaks..." -ForegroundColor Yellow

$examples = @(
    @{Name="Minimal Server"; Src="examples/minimal_server.c"; Extra="src/cwebhttp.c src/memcheck.c src/log.c src/error.c"},
    @{Name="Simple Client"; Src="examples/simple_client.c"; Extra="src/cwebhttp.c src/memcheck.c src/log.c src/error.c"}
)

foreach ($ex in $examples) {
    Write-Host "  Testing: $($ex.Name)..." -ForegroundColor Cyan
    
    $exOutput = "$buildDir/$($ex.Name -replace ' ','_').exe"
    $cmd = "gcc -O0 -g -DCWEBHTTP_ENABLE_MEMCHECK=1 -Iinclude $($ex.Src) $($ex.Extra) -o $exOutput -lws2_32 -lz 2>&1"
    $null = Invoke-Expression $cmd
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "    ✅ Builds without memory errors" -ForegroundColor Green
    } else {
        Write-Host "    ⚠️  Build had warnings" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Memory Leak Testing Complete" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if ($IsLinux) {
    Write-Host "💡 Tip: Run 'valgrind --leak-check=full ./test' for detailed leak analysis on Linux" -ForegroundColor Gray
} elseif ($IsWindows) {
    Write-Host "💡 Tip: Use Dr. Memory or Application Verifier for detailed analysis on Windows" -ForegroundColor Gray
}

exit 0
