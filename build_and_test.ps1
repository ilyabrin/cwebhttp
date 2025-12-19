# build_and_test.ps1 - Cross-platform build and test script
# Works in both native PowerShell and MSYS2

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  cwebhttp Build & Test (gnu17)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Detect environment
$isMSYS2 = Test-Path C:\msys64\usr\bin\bash.exe

if ($isMSYS2) {
    Write-Host "Environment: MSYS2 detected" -ForegroundColor Green
    Write-Host "Using: C:\msys64\usr\bin\bash.exe" -ForegroundColor Gray
    Write-Host ""
    
    # Run in MSYS2
    C:\msys64\usr\bin\bash.exe -lc "cd '$PWD' && make clean && make all && ./build/tests/test_parse.exe && ./build/tests/test_url.exe && ./build/tests/test_chunked.exe"
    
} else {
    Write-Host "Environment: Native Windows" -ForegroundColor Yellow
    Write-Host "Note: Please run in MSYS2 MinGW64 for full functionality" -ForegroundColor Yellow
    Write-Host ""
    
    # Manual build for native Windows
    Write-Host "Building manually with gcc..." -ForegroundColor Yellow
    
    # Clean
    if (Test-Path build) {
        Remove-Item -Recurse -Force build
    }
    New-Item -ItemType Directory -Force -Path build/examples, build/tests | Out-Null
    
    # Build sources
    gcc -Wall -Wextra -std=gnu17 -O2 -Iinclude -Itests `
        -c src/cwebhttp.c -o build/cwebhttp.o
    gcc -Wall -Wextra -std=gnu17 -O2 -Iinclude -Itests `
        -c src/memcheck.c -o build/memcheck.o
    gcc -Wall -Wextra -std=gnu17 -O2 -Iinclude -Itests `
        -c src/log.c -o build/log.o
    gcc -Wall -Wextra -std=gnu17 -O2 -Iinclude -Itests `
        -c src/error.c -o build/error.o
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed" -ForegroundColor Red
        exit 1
    }
    
    # Build Unity test framework
    gcc -Wall -Wextra -std=gnu17 -O2 -Iinclude -Itests `
        -c tests/unity.c -o build/unity.o
    
    # Build tests
    gcc -Wall -Wextra -std=gnu17 -O2 -Iinclude -Itests `
        tests/test_parse.c build/*.o -o build/tests/test_parse.exe -lws2_32 -lz
    
    gcc -Wall -Wextra -std=gnu17 -O2 -Iinclude -Itests `
        tests/test_url.c build/*.o -o build/tests/test_url.exe -lws2_32 -lz
    
    gcc -Wall -Wextra -std=gnu17 -O2 -Iinclude -Itests `
        tests/test_chunked.c build/*.o -o build/tests/test_chunked.exe -lws2_32 -lz
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Build successful" -ForegroundColor Green
        Write-Host ""
        
        # Run tests
        Write-Host "Running tests..." -ForegroundColor Yellow
        .\build\tests\test_parse.exe
        .\build\tests\test_url.exe
        .\build\tests\test_chunked.exe
    }
}

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "🎉 All tests passed with gnu17!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Cyan
} else {
    Write-Host ""
    Write-Host "❌ Tests failed" -ForegroundColor Red
    exit 1
}
