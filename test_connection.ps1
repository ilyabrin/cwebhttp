# Test if server is listening and accepting connections
Write-Host "Testing IOCP server connection..." -ForegroundColor Cyan
Write-Host ""

# Test 1: Check if port is listening
Write-Host "[Test 1] Checking if port 8080 is listening..." -ForegroundColor Yellow
$listening = Get-NetTCPConnection -LocalPort 8080 -State Listen -ErrorAction SilentlyContinue
if ($listening) {
    Write-Host "✓ Port 8080 is LISTENING" -ForegroundColor Green
    Write-Host "  Process: $($listening.OwningProcess)" -ForegroundColor Gray
} else {
    Write-Host "✗ Port 8080 is NOT listening" -ForegroundColor Red
    Write-Host "  Server may not have started properly" -ForegroundColor Red
    exit 1
}
Write-Host ""

# Test 2: Try to connect with TCP client
Write-Host "[Test 2] Attempting TCP connection..." -ForegroundColor Yellow
try {
    $client = New-Object System.Net.Sockets.TcpClient
    $client.ReceiveTimeout = 5000
    $client.SendTimeout = 5000
    
    Write-Host "  Connecting to localhost:8080..." -ForegroundColor Gray
    $client.Connect("127.0.0.1", 8080)
    
    if ($client.Connected) {
        Write-Host "✓ TCP connection established!" -ForegroundColor Green
        
        # Send HTTP request
        Write-Host ""
        Write-Host "[Test 3] Sending HTTP GET request..." -ForegroundColor Yellow
        $stream = $client.GetStream()
        $writer = New-Object System.IO.StreamWriter($stream)
        $writer.AutoFlush = $true
        
        $request = "GET / HTTP/1.1`r`nHost: localhost`r`nConnection: close`r`n`r`n"
        Write-Host "  Request:" -ForegroundColor Gray
        Write-Host "  $($request -replace '`r`n', '\r\n')" -ForegroundColor Gray
        
        $writer.Write($request)
        
        # Read response
        Write-Host ""
        Write-Host "[Test 4] Reading response..." -ForegroundColor Yellow
        $reader = New-Object System.IO.StreamReader($stream)
        
        Start-Sleep -Milliseconds 500
        
        if ($stream.DataAvailable) {
            $response = $reader.ReadToEnd()
            Write-Host "✓ Received response:" -ForegroundColor Green
            Write-Host $response -ForegroundColor White
        } else {
            Write-Host "✗ No response received (timeout or no data)" -ForegroundColor Red
            Write-Host "  This means AcceptEx accepted connection but server didn't respond" -ForegroundColor Yellow
        }
        
        $client.Close()
    }
} catch {
    Write-Host "✗ Connection failed: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "  This likely means AcceptEx is not accepting connections" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Check the server console for:" -ForegroundColor White
Write-Host "  [HANDLER] Got request: GET /" -ForegroundColor Green
Write-Host ""
Write-Host "If you see that message, server is working!" -ForegroundColor White
Write-Host "If not, AcceptEx completions aren't being delivered." -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Cyan
