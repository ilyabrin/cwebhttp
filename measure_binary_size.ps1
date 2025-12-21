#!/usr/bin/env pwsh
# PowerShell script to measure binary sizes with different configurations

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "cwebhttp Binary Size Comparison" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$buildDir = "build/size_test"
$null = New-Item -ItemType Directory -Force -Path $buildDir

# Common flags
$cflags = "-O2 -Iinclude -Itests"
$srcs = "examples/minimal_server.c src/cwebhttp.c src/memcheck.c src/log.c src/error.c"
$ldflags = "-lws2_32 -lz"

# Build configurations
$configs = @(
    @{
        Name = "Default (all features)"
        Flags = ""
        Output = "$buildDir/server_default.exe"
    },
    @{
        Name = "No WebSocket"
        Flags = "-DCWEBHTTP_ENABLE_WEBSOCKET=0"
        Output = "$buildDir/server_nows.exe"
    },
    @{
        Name = "No Cookies"
        Flags = "-DCWEBHTTP_ENABLE_COOKIES=0"
        Output = "$buildDir/server_nocookies.exe"
    },
    @{
        Name = "No Compression"
        Flags = "-DCWEBHTTP_ENABLE_COMPRESSION=0"
        Output = "$buildDir/server_nocomp.exe"
    },
    @{
        Name = "No Memcheck + No Logging"
        Flags = "-DCWEBHTTP_ENABLE_MEMCHECK=0 -DCWEBHTTP_ENABLE_LOGGING=0"
        Output = "$buildDir/server_minimal_overhead.exe"
    }
)

$results = @()

foreach ($config in $configs) {
    Write-Host "Building: $($config.Name)..." -ForegroundColor Yellow
    
    $fullCmd = "gcc $cflags $($config.Flags) $srcs -o $($config.Output) $ldflags 2>&1"
    $output = Invoke-Expression $fullCmd
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ❌ Build failed" -ForegroundColor Red
        continue
    }
    
    if (Test-Path $config.Output) {
        $size = (Get-Item $config.Output).Length
        $sizeKB = [math]::Round($size / 1024, 2)
        
        # Strip symbols for final size
        & strip $config.Output 2>$null
        $strippedSize = (Get-Item $config.Output).Length
        $strippedKB = [math]::Round($strippedSize / 1024, 2)
        
        Write-Host "  ✓ Compiled: $sizeKB KB" -ForegroundColor Green
        Write-Host "  ✓ Stripped: $strippedKB KB" -ForegroundColor Green
        
        $results += [PSCustomObject]@{
            Configuration = $config.Name
            Size_KB = $sizeKB
            Stripped_KB = $strippedKB
            Savings = if ($results.Count -gt 0) { 
                [math]::Round(($results[0].Stripped_KB - $strippedKB) / $results[0].Stripped_KB * 100, 1) 
            } else { 
                0 
            }
        }
    }
    
    Write-Host ""
}

# Display results table
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Results Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$results | Format-Table -AutoSize Configuration, 
    @{Label="Size (KB)"; Expression={$_.Size_KB}; Align="Right"},
    @{Label="Stripped (KB)"; Expression={$_.Stripped_KB}; Align="Right"},
    @{Label="Savings (%)"; Expression={if ($_.Savings -eq 0) {"-"} else {"+$($_.Savings)%"}}; Align="Right"}

Write-Host ""
Write-Host "Note: Actual savings depend on which features your application uses." -ForegroundColor Gray
Write-Host "For maximum size reduction, use -DCWEBHTTP_BUILD_MINIMAL" -ForegroundColor Gray
Write-Host ""
