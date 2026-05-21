$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$nuspec = Join-Path $repoRoot 'proxy\TCodeProxy\TCodeProxy.nuspec'
$nuget = 'nuget.exe'

if (-not (Get-Command $nuget -ErrorAction SilentlyContinue)) {
    Write-Host 'nuget.exe not found on PATH. Downloading a temporary copy...'
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $tempNuget = Join-Path $env:TEMP 'nuget.exe'
    $nugetUrl = 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe'
    if (-not (Test-Path $tempNuget)) {
        Invoke-WebRequest -Uri $nugetUrl -OutFile $tempNuget
    }
    if (-not (Test-Path $tempNuget)) {
        Write-Error "Failed to download nuget.exe. Install it manually or ensure it is on PATH."
        exit 1
    }
    $nuget = $tempNuget
}

$proxyDir = Join-Path $repoRoot 'proxy\TCodeProxy'
$buildDir = Join-Path $proxyDir 'bin\Release\net10.0-windows'
if (-not (Test-Path $buildDir)) {
    Write-Error "Build output not found: $buildDir. Run `dotnet build -c Release` in proxy\TCodeProxy first."
    exit 1
}

$packageOutput = Join-Path $repoRoot 'proxy\nupkg'
if (-not (Test-Path $packageOutput)) {
    New-Item -ItemType Directory -Path $packageOutput | Out-Null
}

Write-Host "Packing NuGet package from $nuspec..."
& $nuget pack $nuspec -BasePath $repoRoot -OutputDirectory $packageOutput
if ($LASTEXITCODE -ne 0) {
    Write-Error "NuGet pack failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}
Write-Host "Package created in $packageOutput"
