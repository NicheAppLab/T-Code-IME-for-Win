param(
    [string]$Configuration = 'Release',
    [switch]$SkipInstaller,
    [switch]$PackNuGet
)

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptRoot '..')
$buildDir = Join-Path $repoRoot 'build'
$proxyDir = Join-Path $repoRoot 'proxy\TCodeProxy'
$proxyNuGetDir = Join-Path $repoRoot 'proxy'
$nupkgDir = Join-Path $proxyNuGetDir 'nupkg'

function Fail($message) {
    Write-Error $message
    exit 1
}

function RequireCommand($command, $hint) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) {
        Fail "$command is required but was not found. $hint"
    }
}

function Invoke-Process($file, $arguments) {
    Write-Host "Running: $file $arguments"
    $argList = @()
    if ($arguments -ne $null -and $arguments -ne '') {
        $argList = @($arguments)
    }
    $process = Start-Process -FilePath $file -ArgumentList $argList -NoNewWindow -Wait -PassThru
    if ($process.ExitCode -ne 0) {
        Fail "$file failed with exit code $($process.ExitCode)."
    }
}

Write-Host '=== T-Code IME Super Build Script ==='
Write-Host "Repo root: $repoRoot"
Write-Host "Configuration: $Configuration"

# Native IME build
RequireCommand 'cmake' 'Install CMake and make sure it is on PATH.'
Write-Host 'Building native TCodeIME...'
Invoke-Process 'cmake' "-S `"$repoRoot`" -B `"$buildDir`" -DCMAKE_BUILD_TYPE=$Configuration"
Invoke-Process 'cmake' "--build `"$buildDir`" --config $Configuration"

# Proxy build
Write-Host 'Building proxy...'
Set-Location $proxyDir
if (Get-Command nuget -ErrorAction SilentlyContinue) {
    Invoke-Process 'nuget' 'restore TCodeProxy.csproj'
} else {
    Write-Host 'nuget not found; using dotnet restore instead.'
    RequireCommand 'dotnet' 'Install the .NET SDK and add it to PATH.'
    Invoke-Process 'dotnet' 'restore TCodeProxy.csproj'
}
RequireCommand 'dotnet' 'Install the .NET SDK and add it to PATH.'
Invoke-Process 'dotnet' "build TCodeProxy.csproj --configuration $Configuration"

# Installer build
if (-not $SkipInstaller) {
    Write-Host 'Building installer...'
    Set-Location $scriptRoot
    Invoke-Process 'powershell' "-NoProfile -ExecutionPolicy Bypass -File `"$scriptRoot\build_installer.ps1`""
}

if ($PackNuGet) {
    Write-Host 'Packing NuGet package...'
    Set-Location $proxyNuGetDir
    Invoke-Process 'powershell' "-NoProfile -ExecutionPolicy Bypass -File `"$proxyNuGetDir\build_nuget.ps1`""
}

Write-Host '=== Build completed successfully ==='
if ($PackNuGet) {
    Write-Host "NuGet package output: $nupkgDir"
}
Write-Host "Installer output: $scriptRoot\output"
