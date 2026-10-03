# PC time server - no Python needed
# Run: powershell -ExecutionPolicy Bypass -File D:\time\pc_time_server.ps1

$Port = 8765
$ip = [System.Net.IPAddress]::Any
$listener = New-Object System.Net.Sockets.TcpListener $ip, $Port

try {
    $listener.Start()
} catch {
    Write-Host ("Cannot bind port {0}: {1}" -f $Port, $_.Exception.Message)
    exit 1
}

Write-Host ("PC time server OK on 0.0.0.0:{0}" -f $Port)
Write-Host "Fill PC IP in board MQTT settings, then tap sync button."
Write-Host "Keep this window open. Ctrl+C to stop."
Write-Host ""

function Get-HttpPath([System.Net.Sockets.NetworkStream]$stream) {
    $buf = New-Object byte[] 4096
    $sb = New-Object System.Text.StringBuilder
    $deadline = [datetime]::UtcNow.AddSeconds(3)
    while ([datetime]::UtcNow -lt $deadline) {
        if ($stream.DataAvailable -or $sb.Length -eq 0) {
            $n = $stream.Read($buf, 0, $buf.Length)
            if ($n -le 0) { break }
            [void]$sb.Append([System.Text.Encoding]::ASCII.GetString($buf, 0, $n))
            if ($sb.ToString().Contains("`r`n`r`n")) { break }
            if ($sb.Length -gt 8192) { break }
        } else {
            Start-Sleep -Milliseconds 20
            if (-not $stream.DataAvailable) { break }
        }
    }
    $text = $sb.ToString()
    if ($text -match "GET\s+(\S+)") {
        return ($Matches[1].Split("?")[0])
    }
    return "/"
}

while ($true) {
    $client = $listener.AcceptTcpClient()
    try {
        $stream = $client.GetStream()
        $stream.ReadTimeout = 3000
        $stream.WriteTimeout = 3000
        $path = Get-HttpPath $stream

        if (($path -eq "/time") -or ($path -eq "/")) {
            $now = Get-Date
            $epoch = [DateTimeOffset]::Now.ToUnixTimeSeconds()
            $timeStr = $now.ToString("yyyy-MM-dd HH:mm:ss")
            $json = "{{""time"":""{0}"",""epoch"":{1}}}" -f $timeStr, $epoch
            $body = [System.Text.Encoding]::UTF8.GetBytes($json)
            $header = "HTTP/1.1 200 OK`r`nContent-Type: application/json; charset=utf-8`r`nContent-Length: {0}`r`nConnection: close`r`n`r`n" -f $body.Length
            $hdrBytes = [System.Text.Encoding]::ASCII.GetBytes($header)
            $stream.Write($hdrBytes, 0, $hdrBytes.Length)
            $stream.Write($body, 0, $body.Length)
            $remote = $client.Client.RemoteEndPoint.Address
            Write-Host ("served time {0} to {1}" -f $timeStr, $remote)
        } else {
            $hdr = [System.Text.Encoding]::ASCII.GetBytes("HTTP/1.1 404 Not Found`r`nContent-Length: 0`r`nConnection: close`r`n`r`n")
            $stream.Write($hdr, 0, $hdr.Length)
        }
        $stream.Flush()
    } catch {
        Write-Host ("request error: {0}" -f $_.Exception.Message)
    } finally {
        $client.Close()
    }
}
