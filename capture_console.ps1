# Capture console output from a board's ST-LINK virtual COM port (VCP):
# opens the serial port, resets the target via probe-rs, prints N seconds of
# serial output (and saves it to a file next to the caller's output path).
#
# Usage:
#   powershell -File capture_console.ps1 [-COM COMn] [-Seconds 15] [-Out <file>]
#   -COM omitted -> auto-detect the STMicroelectronics STLink virtual COM port.

param(
    [string]$COM = '',
    [int]$Seconds = 15,
    [string]$Out = ''
)
$ErrorActionPreference = 'Continue'
$probe = '0483:3754:0023003B3137511233333639'   # nucleo-g431 ST-LINK

if (-not $COM) {
    $port = Get-CimInstance Win32_SerialPort |
        Where-Object { $_.PNPDeviceID -like '*STLINK*' -or $_.Name -like '*STLink*' } |
        Select-Object -First 1
    if ($port) {
        $COM = ($port.DeviceID)
    }
    if (-not $COM) {
        Write-Error "No ST-LINK VCP found; pass -COM COMn"
        exit 1
    }
}

$tmpfile = [System.IO.Path]::GetTempFileName()
Remove-Item $tmpfile -ErrorAction SilentlyContinue

$job = Start-Job -ArgumentList $tmpfile, $COM {
    param($tf, $com)
    $p = [System.IO.Ports.SerialPort]::new($com, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $p.ReadTimeout = 6000
    $p.Open()
    $p.DiscardInBuffer()
    $sw = [System.IO.StreamWriter]::new($tf, $false)
    try {
        while ($true) {
            $line = $p.ReadLine()
            $sw.WriteLine($line)
            $sw.Flush()
        }
    } catch { }
    $sw.Close()
    $p.Close()
}

Start-Sleep -Milliseconds 1000
# Reset the target so the capture starts from the boot banner.
probe-rs reset --probe $probe 2>$null | Out-Null
Start-Sleep -Seconds $Seconds
Stop-Job $job -ErrorAction SilentlyContinue
Remove-Job $job -Force -ErrorAction SilentlyContinue
if (Test-Path $tmpfile) {
    $captured = Get-Content $tmpfile -Raw
    Remove-Item $tmpfile -ErrorAction SilentlyContinue
} else {
    $captured = ''
}
if ($Out) {
    [System.IO.File]::WriteAllText($Out, $captured)
}
Write-Output "----- CONSOLE ($COM, $($captured.Length) chars) -----"
Write-Output $captured
Write-Output "----- END -----"
