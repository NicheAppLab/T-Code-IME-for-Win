function Invoke-Process($file, $arguments) {
    Write-Host "file:$file"
    Write-Host "arguments:$arguments"
}
$repoRoot = 'C:\Users\imuno\VSCodeProjects\T-Code-IME-for-Win'
$buildDir = 'C:\Users\imuno\VSCodeProjects\T-Code-IME-for-Win\build'
Invoke-Process 'cmake' "-S `"$repoRoot`" -B `"$buildDir`" -DCMAKE_BUILD_TYPE=Release"
