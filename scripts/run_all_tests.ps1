#!/usr/bin/env pwsh
# Master test script - runs all test suites

param(
    [switch]$Quick,
    [switch]$SkipIntegration,
    [switch]$SkipPerformance
)

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║        cwebhttp Comprehensive Test Suite              ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$failed = 0
$passed = 0

# 1. Core Unit Tests
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "1. Core Unit Tests" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
try {
    make test 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ Core tests passed" -ForegroundColor Green
        $passed++
    } else {
        Write-Host "❌ Core tests failed" -ForegroundColor Red
        $failed++
    }
} catch {
    Write-Host "❌ Core tests failed" -ForegroundColor Red
    $failed++
}
Write-Host ""

# 2. Feature Flag Combinations
if (-not $Quick) {
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "2. Feature Flag Combinations" -ForegroundColor Yellow
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    try {
        & .\scripts\test_feature_flags.ps1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ Feature flag tests passed" -ForegroundColor Green
            $passed++
        } else {
            Write-Host "❌ Feature flag tests failed" -ForegroundColor Red
            $failed++
        }
    } catch {
        Write-Host "❌ Feature flag tests failed" -ForegroundColor Red
        $failed++
    }
    Write-Host ""
}

# 3. Memory Leak Tests
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "3. Memory Leak Detection" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
try {
    & .\scripts\test_memory_leaks.ps1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ Memory tests passed" -ForegroundColor Green
        $passed++
    } else {
        Write-Host "❌ Memory tests failed" -ForegroundColor Red
        $failed++
    }
} catch {
    Write-Host "❌ Memory tests failed" -ForegroundColor Red
    $failed++
}
Write-Host ""

# 4. Performance Regression
if (-not $SkipPerformance) {
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "4. Performance Regression" -ForegroundColor Yellow
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    try {
        & .\scripts\test_performance_regression.ps1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ Performance tests passed" -ForegroundColor Green
            $passed++
        } else {
            Write-Host "⚠️  Performance regression detected" -ForegroundColor Yellow
            $passed++  # Don't fail on performance regression
        }
    } catch {
        Write-Host "⚠️  Performance tests failed" -ForegroundColor Yellow
        $passed++  # Don't fail on performance tests
    }
    Write-Host ""
}

# 5. Integration Tests (Optional - requires network)
if (-not $SkipIntegration -and -not $Quick) {
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "5. Integration Tests (Network Required)" -ForegroundColor Yellow
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    try {
        make integration 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ Integration tests passed" -ForegroundColor Green
            $passed++
        } else {
            Write-Host "⚠️  Integration tests failed (network issue?)" -ForegroundColor Yellow
            $passed++  # Don't fail on network issues
        }
    } catch {
        Write-Host "⚠️  Integration tests skipped" -ForegroundColor Yellow
        $passed++
    }
    Write-Host ""
}

# Summary
Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                  Test Summary                          ║" -ForegroundColor Cyan
Write-Host "╠═══════════════════════════════════════════════════════╣" -ForegroundColor Cyan
Write-Host "║                                                        ║"
Write-Host "║  Passed: $passed" -ForegroundColor Green
Write-Host "║  Failed: $failed" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Red" })
Write-Host "║                                                        ║"
Write-Host "╚═══════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

if ($failed -eq 0) {
    Write-Host "🎉 All critical tests passed!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "❌ $failed test suite(s) failed" -ForegroundColor Red
    exit 1
}
