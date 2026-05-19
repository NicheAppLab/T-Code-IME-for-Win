$errors = @()
try {
    $ports = Get-NetTCPConnection -LocalPort 8080 -ErrorAction SilentlyContinue
    foreach($c in $ports) {
        try {
            Write-Output "Killing PID $($c.OwningProcess) (owner of port 8080)"
            Stop-Process -Id $c.OwningProcess -Force -ErrorAction Stop
        } catch {
            $errors += $_
        }
    }
} catch {
    $errors += $_
}

try {
    $procs = Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -and ($_.CommandLine -match 'tcodeserver|tcodeserver.tcodeserver|tcodeengine') }
    foreach($p in $procs) {
        try {
            Write-Output "Killing PID $($p.ProcessId): $($p.CommandLine)"
            Stop-Process -Id $p.ProcessId -Force -ErrorAction Stop
        } catch {
            $errors += $_
        }
    }
} catch {
    $errors += $_
}

if ($errors.Count -gt 0) {
    Write-Output "Errors encountered while stopping processes:"
    $errors | ForEach-Object { Write-Output $_ }
} else {
    Write-Output "Done."
}
