$log = 'c:\Users\imuno\VSCodeProjects\T-Code-IME-for-Win\installer\build_installer_log.txt'
$script = 'c:\Users\imuno\VSCodeProjects\T-Code-IME-for-Win\installer\build_installer.ps1'
Write-Host "Running: $script"
& $script *>&1 | Tee-Object -FilePath $log
Write-Host "Log saved to: $log"
