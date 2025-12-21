#!/usr/bin/env pwsh
# Test all feature flag combinations to ensure no build breaks

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Feature Flag Combination Testing" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$srcFiles = "src/cwebhttp.c src/memcheck.c src/log.c src/error.c"
$testFile = "tests/test_parse.c tests/unity.c"
$baseFlags = "-Wall -Wextra -O2 -Iinclude -Itests"
$ldFlags = "-lws2_32 -lz"

# Feature combinations to test
$combinations = @(
    @{Name="All ON (default)"; Flags=""},
    @{Name="All OFF"; Flags="-DCWEBHTTP_ENABLE_COOKIES=0 -DCWEBHTTP_ENABLE_COMPRESSION=0 -DCWEBHTTP_ENABLE_CHUNKED=0 -DCWEBHTTP_ENABLE_CONNECTION_POOL=0"},
    @{Name="No Cookies"; Flags="-DCWEBHTTP_ENABLE_COOKIES=0"},
    @{Name="No Compression"; Flags="-DCWEBHTTP_ENABLE_COMPRESSION=0"},
    @{Name="No Chunked"; Flags="-DCWEBHTTP_ENABLE_CHUNKED=0"},
    @{Name="No Pool"; Flags="-DCWEBHTTP_ENABLE_CONNECTION_POOL=0"},
    @{Name="No WebSocket"; Flags="-DCWEBHTTP_ENABLE_WEBSOCKET=0"},
    @{Name="No Logging"; Flags="-DCWEBHTTP_ENABLE_LOGGING=0"},
    @{Name="No Memcheck"; Flags="-DCWEBHTTP_ENABLE_MEMCHECK=0"},
    @{Name="Minimal Client"; Flags="-DCWEBHTTP_ENABLE_FILE_SERVING=0 -DCWEBHTTP_ENABLE_WEBSOCKET=0"},
    @{Name="Minimal Server"; Flags="-DCWEBHTTP_ENABLE_COOKIES=0 -DCWEBHTTP_ENABLE_COMPRESSION=0 -DCWEBHTTP_ENABLE_CONNECTION_POOL=0"},
    @{Name="Embedded"; Flags="-DCWEBHTTP_ENABLE_COOKIES=0 -DCWEBHTTP_ENABLE_COMPRESSION=0 -DCWEBHTTP_ENABLE_CHUNKED=0 -DCWEBHTTP_ENABLE_CONNECTION_POOL=0 -DCWEBHTTP_ENABLE_WEBSOCKET=0 -DCWEBHTTP_ENABLE_LOGGING=0 -DCWEBHTTP_ENABLE_MEMCHECK=0"}
)

$passed = 0
$failed = 0
$results = @()

foreach ($combo in $combinations) {
    Write-Host "Testing: $($combo.Name)..." -ForegroundColor Yellow
    
    $output = "build/test_flags/$($combo.Name -replace ' ','_').exe"
    $null = New-Item -ItemType Directory -Force -Path "build/test_flags"
    
    $cmd = "gcc $baseFlags $($combo.Flags) $testFile $srcFiles -o $output $ldFlags 2>&1"
    $buildOutput = Invoke-Expression $cmd
    
    if ($LASTEXITCODE -eq 0) {
        # Build succeeded, try to run test
        $testOutput = & $output 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  ✅ PASS - Build and test successful" -ForegroundColor Green
            $passed++
            $results += [PSCustomObject]@{
                Configuration = $combo.Name
                Status = "✅ PASS"
                Size = [math]::Round((Get-Item $output).Length / 1024, 2)
            }
        } else {
            Write-Host "  ❌ FAIL - Test execution failed" -ForegroundColor Red
            Write-Host "    $testOutput" -ForegroundColor Red
            $failed++
            $results += [PSCustomObject]@{
                Configuration = $combo.Name
                Status = "❌ TEST FAIL"
                Size = 0
            }
        }
    } else {
        Write-Host "  ❌ FAIL - Build failed" -ForegroundColor Red
        $errorLines = $buildOutput | Select-String "error:" | Select-Object -First 3
        foreach ($err in $errorLines) {
            Write-Host "    $err" -ForegroundColor Red
        }
        $failed++
        $results += [PSCustomObject]@{
            Configuration = $combo.Name
            Status = "❌ BUILD FAIL"
            Size = 0
        }
    }
    Write-Host ""
}

# Summary
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Test Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Total Configurations: $($combinations.Count)" -ForegroundColor White
Write-Host "Passed: $passed" -ForegroundColor Green
Write-Host "Failed: $failed" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Red" })
Write-Host ""

$results | Format-Table -AutoSize Configuration, Status, @{Label="Size (KB)"; Expression={$_.Size}}

if ($failed -eq 0) {
    Write-Host "🎉 All feature flag combinations working!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "⚠️  Some configurations failed" -ForegroundColor Red
    exit 1
}
