param(
    [string]$src,
    [string]$out
)

Add-Type -AssemblyName System.Drawing
$icon = [System.Drawing.Icon]::ExtractAssociatedIcon($src)
if ($icon -ne $null) {
    $fs = [System.IO.File]::Open($out, [System.IO.FileMode]::Create)
    $icon.Save($fs)
    $fs.Close()
    Write-Host "Saved $out"
} else {
    Write-Host "NoIcon"
}
