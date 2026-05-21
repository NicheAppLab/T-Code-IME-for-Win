# Finds installed vcpkg packages and reports ones not referenced in source files
# Usage: .\scripts\find_unused_vcpkg.ps1

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot = Resolve-Path "$scriptRoot\.."
$vcpkgExe = Join-Path $repoRoot 'vcpkg\vcpkg.exe'
$installedShare = Join-Path $repoRoot 'vcpkg_installed\x64-windows\share'

if (-not (Test-Path $vcpkgExe)) {
    Write-Error "vcpkg.exe not found at $vcpkgExe"
    exit 2
}
if (-not (Test-Path $installedShare)) {
    Write-Error "vcpkg installed share folder not found at $installedShare"
    exit 2
}

# Get installed packages from vcpkg list parsing package names before ':'
$installed = & $vcpkgExe list | ForEach-Object {
    if ($_ -match '^([a-z0-9A-Z\-_.+]+):') { $matches[1] }
} | Sort-Object -Unique

if (-not $installed) {
    Write-Host "No installed packages found via vcpkg list"
    exit 0
}

Write-Host "Installed packages:`n" -ForegroundColor Cyan
$installed | ForEach-Object { Write-Host " - $_" }

# For each package, search for references in source files
$unused = @()
foreach ($pkg in $installed) {
    # Build a set of search patterns: folder name, common include names
    $patterns = @(
        "$pkg",
        "$pkg/",
        "$pkg.h",
        "$pkg.hpp",
        ($pkg + '\\')
    )
    # Also check package share folder for canonical names
    $sharePath = Join-Path $installedShare $pkg
    if (Test-Path $sharePath) {
        # search for headers in share folder
        $hdrs = Get-ChildItem -Path $sharePath -Recurse -Include *.h,*.hpp -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name -Unique
        foreach ($h in $hdrs) { $patterns += $h }
    }

    $found = $false
    foreach ($pat in $patterns) {
        if (Select-String -Path "$repoRoot\**\*.*" -Pattern $pat -SimpleMatch -Quiet -ErrorAction SilentlyContinue) { $found = $true; break }
    }

    if (-not $found) { $unused += $pkg }
}

if (-not $unused) {
    Write-Host "No unused packages detected." -ForegroundColor Green
    exit 0
}

Write-Host "Unused packages (candidates for removal):" -ForegroundColor Yellow
$unused | ForEach-Object { Write-Host " - $_" }

Write-Host "\nTo remove them, run the following commands (verify before running):`n" -ForegroundColor Cyan
foreach ($p in $unused) { Write-Host ".\vcpkg\vcpkg.exe remove ${p}:x64-windows --recurse" }

Write-Host "\nOptionally run one-by-one to confirm nothing breaks." -ForegroundColor Gray
