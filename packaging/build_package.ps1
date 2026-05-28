# T-Code IME MSIX Packager Script
# This script compiles, stages, manifests, and packages the T-Code IME using winapp CLI.

$ErrorActionPreference = "Stop"

# 1. Paths configuration
$root = Resolve-Path ".."
$packagingDir = Join-Path $root "packaging"
$layoutDir = Join-Path $packagingDir "layout"
$outputDir = Join-Path $packagingDir "output"

Write-Host "=== T-Code IME MSIX Packager ===" -ForegroundColor Cyan

# Ensure clean layout and output folders
if (Test-Path $layoutDir) {
    Remove-Item $layoutDir -Recurse -Force
}
New-Item -ItemType Directory -Path $layoutDir -Force | Out-Null
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

# 2. Stage Binaries
Write-Host "Staging compiled binaries..." -ForegroundColor Green

# Stage Native DLL at layout root
$dllSource = Join-Path $root "build\Release\TCodeIME.dll"
if (-not (Test-Path $dllSource)) {
    throw "TCodeIME.dll not found. Please build the Release configuration first."
}
Copy-Item $dllSource -Destination $layoutDir -Force

# Stage C# Proxy under layout/proxy
$proxyLayout = New-Item -ItemType Directory -Path (Join-Path $layoutDir "proxy") -Force
$proxySource = Join-Path $root "proxy\TCodeProxy\bin\Release\net10.0-windows"
if (-not (Test-Path (Join-Path $proxySource "TCodeProxy.exe"))) {
    throw "TCodeProxy.exe not found. Please build the Release configuration first."
}
Copy-Item (Join-Path $proxySource "\*") -Destination $proxyLayout -Recurse -Force

# Stage Java T-Code Engine under layout/engine
$engineLayout = New-Item -ItemType Directory -Path (Join-Path $layoutDir "engine") -Force
$engineSource = Join-Path $root "engine"
Copy-Item (Join-Path $engineSource "\*") -Destination $engineLayout -Recurse -Force

# Stage Icons under layout/icons
$iconsLayout = New-Item -ItemType Directory -Path (Join-Path $layoutDir "icons") -Force
$iconSource = Join-Path $root "icons\icon.png"
Copy-Item $iconSource -Destination $iconsLayout -Force

# 3. Generate AppX Manifest & Visual Assets using winapp CLI
Write-Host "Generating Package.appxmanifest and visual assets using winapp CLI..." -ForegroundColor Green

Push-Location $layoutDir
# Generate base manifest inside layout folder
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
winapp manifest generate . `
    --package-name "TCodeIME" `
    --publisher-name "CN=TCode" `
    --description "T-Code IME for Windows" `
    --executable "proxy\TCodeProxy.exe" `
    --logo-path "icons\icon.png" `
    --if-exists overwrite
Pop-Location

# 4. Inject TSF Input Method and Packaged Startup Task into AppxManifest.xml
Write-Host "Customizing Package.appxmanifest for startup task..." -ForegroundColor Green
$manifestPath = Join-Path $layoutDir "Package.appxmanifest"
[xml]$manifest = Get-Content $manifestPath

# Update namespaces
$manifest.Package.SetAttribute("xmlns:desktop", "http://schemas.microsoft.com/appx/manifest/desktop/windows10")
$ignorable = $manifest.Package.GetAttribute("IgnorableNamespaces")
if ($ignorable -notlike "*desktop*") {
    $manifest.Package.SetAttribute("IgnorableNamespaces", "$ignorable desktop")
}

# Find Application node
$appNode = $manifest.Package.Applications.Application
$appNode.SetAttribute("Executable", "proxy\TCodeProxy.exe")

# Create Extensions element if not exists
$extsNode = $appNode.Extensions
if ($null -eq $extsNode) {
    $extsNode = $manifest.CreateElement("Extensions", $manifest.Package.NamespaceURI)
    $appNode.AppendChild($extsNode) | Out-Null
}

# Create StartupTask extension element
$startupNode = $manifest.CreateElement("desktop", "Extension", "http://schemas.microsoft.com/appx/manifest/desktop/windows10")
$startupNode.SetAttribute("Category", "windows.startupTask")
$startupNode.SetAttribute("Executable", "proxy\TCodeProxy.exe")
$startupNode.SetAttribute("EntryPoint", "Windows.FullTrustApplication")

# Create desktop:StartupTask element
$taskNode = $manifest.CreateElement("desktop", "StartupTask", "http://schemas.microsoft.com/appx/manifest/desktop/windows10")
$taskNode.SetAttribute("TaskId", "TCodeProxyStartup")
$taskNode.SetAttribute("Enabled", "true")
$taskNode.SetAttribute("DisplayName", "T-Code IME Proxy")
$startupNode.AppendChild($taskNode) | Out-Null

$extsNode.AppendChild($startupNode) | Out-Null

# Save modified manifest
$manifest.Save($manifestPath)

# 5. Pack and Sign the MSIX package
Write-Host "Creating and signing MSIX package..." -ForegroundColor Green
$msixFile = Join-Path $outputDir "TCodeIME.msix"

winapp package $layoutDir `
    --output $msixFile `
    --generate-cert `
    --publisher "CN=TCode" `
    --manifest $manifestPath

Write-Host "Successfully packaged and signed T-Code IME MSIX package!" -ForegroundColor Green
Write-Host "MSIX Location: $msixFile" -ForegroundColor Cyan
