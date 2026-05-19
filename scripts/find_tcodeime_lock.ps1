Get-Process | ForEach-Object {
    try {
        if ($_.Modules | Where-Object { $_.ModuleName -eq 'TCodeIME.dll' }) {
            $_ | Select-Object Id, ProcessName
        }
    } catch {
        # ignore inaccessible processes
    }
}
