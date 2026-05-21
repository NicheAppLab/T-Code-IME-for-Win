$scriptPath = Join-Path $PSScriptRoot 'TCodeIMEInstaller.iss'
$possiblePaths = @()
$programFilesX86 = ${env:ProgramFiles(x86)}
if ($programFilesX86) {
    $possiblePaths += Join-Path $programFilesX86 'Inno Setup 6\ISCC.exe'
    $possiblePaths += Join-Path $programFilesX86 'Inno Setup 7\ISCC.exe'
}
if ($env:ProgramFiles) {
    $possiblePaths += Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe'
    $possiblePaths += Join-Path $env:ProgramFiles 'Inno Setup 7\ISCC.exe'
}
if ($env:LocalAppData) {
    $possiblePaths += Join-Path $env:LocalAppData 'Programs\Inno Setup 6\ISCC.exe'
    $possiblePaths += Join-Path $env:LocalAppData 'Programs\Inno Setup 7\ISCC.exe'
}
$commandInfo = Get-Command ISCC.exe -ErrorAction SilentlyContinue
if ($commandInfo) {
    $possiblePaths += $commandInfo.Source
}
$innoPath = $possiblePaths | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $innoPath) {
    Write-Error "Inno Setup compiler not found. Install Inno Setup 6 or 7 and try again."
    Write-Error "Searched paths:"
    $possiblePaths | ForEach-Object { Write-Error "  $_" }
    exit 1
}

$logPath = Join-Path $PSScriptRoot 'build_installer.log'
Write-Host "Building installer from $scriptPath using $innoPath"
Write-Host "Writing installer build log to $logPath"
& $innoPath $scriptPath 2>&1 | Tee-Object -FilePath $logPath
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    Write-Error "Installer build failed with exit code $exitCode"
    exit $exitCode
}
Write-Host "Installer built successfully. Output is in installer\output. Log: $logPath"