#!/usr/bin/env pwsh
# Performance regression testing
# Tracks parser performance over time to detect regressions

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Performance Regression Testing" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$buildDir = "build/perf_test"
$null = New-Item -ItemType Directory -Force -Path $buildDir

# Build benchmarks
Write-Host "Building performance benchmarks..." -ForegroundColor Yellow
$benchmarks = @(
    @{Name="Parser"; Src="benchmarks/bench_parser.c"},
    @{Name="Memory"; Src="benchmarks/bench_memory.c"}
)

$srcs = "src/cwebhttp.c src/memcheck.c src/log.c src/error.c"
$cflags = "-O2 -Iinclude -Itests"

foreach ($bench in $benchmarks) {
    $output = "$buildDir/$($bench.Name).exe"
    $cmd = "gcc $cflags $($bench.Src) $srcs -o $output -lws2_32 -lz 2>&1"
    $null = Invoke-Expression $cmd
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ Build failed: $($bench.Name)" -ForegroundColor Red
        exit 1
    }
}

Write-Host "✅ Builds successful" -ForegroundColor Green
Write-Host ""

# Run parser benchmark
Write-Host "Running parser benchmark..." -ForegroundColor Yellow
$parserOutput = & "$buildDir/Parser.exe" 2>&1
Write-Host $parserOutput
Write-Host ""

# Parse results
$parserSpeed = $parserOutput | Select-String "cwebhttp \(request parser\):\s+([\d.]+) MB/s" | ForEach-Object { $_.Matches.Groups[1].Value }
$responseSpeed = $parserOutput | Select-String "cwebhttp \(response parser\):\s+([\d.]+) MB/s" | ForEach-Object { $_.Matches.Groups[1].Value }

# Run memory benchmark
Write-Host "Running memory benchmark..." -ForegroundColor Yellow
$memOutput = & "$buildDir/Memory.exe" 2>&1
Write-Host $memOutput
Write-Host ""

# Check for zero allocations
$zeroAlloc = $memOutput | Select-String "Zero-allocation parsing: PASS"

# Results Summary
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Performance Results" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if ($parserSpeed) {
    Write-Host "Request Parser:  $parserSpeed MB/s" -ForegroundColor Green
} else {
    Write-Host "Request Parser:  Could not parse" -ForegroundColor Yellow
}

if ($responseSpeed) {
    Write-Host "Response Parser: $responseSpeed MB/s" -ForegroundColor Green
} else {
    Write-Host "Response Parser: Could not parse" -ForegroundColor Yellow
}

if ($zeroAlloc) {
    Write-Host "Zero Allocations: ✅ PASS" -ForegroundColor Green
} else {
    Write-Host "Zero Allocations: ❌ FAIL" -ForegroundColor Red
}

Write-Host ""

# Performance baselines (adjust based on your system)
$minParserSpeed = 1000  # MB/s
$minResponseSpeed = 1000 # MB/s

$failed = $false

if ($parserSpeed -and [double]$parserSpeed -lt $minParserSpeed) {
    Write-Host "⚠️  Warning: Parser speed below baseline ($minParserSpeed MB/s)" -ForegroundColor Yellow
    $failed = $true
}

if ($responseSpeed -and [double]$responseSpeed -lt $minResponseSpeed) {
    Write-Host "⚠️  Warning: Response parser speed below baseline ($minResponseSpeed MB/s)" -ForegroundColor Yellow
    $failed = $true
}

if (-not $zeroAlloc) {
    Write-Host "❌ Error: Zero-allocation guarantee violated" -ForegroundColor Red
    $failed = $true
}

Write-Host ""

if (-not $failed) {
    Write-Host "🎉 All performance benchmarks passed!" -ForegroundColor Green
    
    # Save results for historical tracking
    $timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"
    $resultsFile = "perf_history.csv"
    
    if (-not (Test-Path $resultsFile)) {
        "Timestamp,ParserSpeed,ResponseSpeed,ZeroAlloc" | Out-File $resultsFile
    }
    
    "$timestamp,$parserSpeed,$responseSpeed,$($zeroAlloc -ne $null)" | Out-File $resultsFile -Append
    Write-Host "📊 Results saved to $resultsFile" -ForegroundColor Gray
    
    exit 0
} else {
    Write-Host "❌ Performance regression detected!" -ForegroundColor Red
    exit 1
}
