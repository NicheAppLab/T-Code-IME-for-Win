Param(
    [Parameter(Mandatory=$true)]
    [string]$Version
)

# Ensure version has four components as required by the appx manifest (e.g., 0.1.0.0)
function Convert-ToFourPartVersion([string]$v) {
    $parts = $v.Split('.')
    while ($parts.Count -lt 4) {
        $parts += '0'
    }
    if ($parts.Count -gt 4) { $parts = $parts[0..3] }
    return $parts -join '.'
}
$manifestVersion = Convert-ToFourPartVersion $Version

$manifestPath = Join-Path $PSScriptRoot "layout\Package.appxmanifest"
[xml]$xml = Get-Content $manifestPath

# Locate the Identity element inside the Package element
$identity = $xml.Package.Identity
if (-not $identity) {
    Write-Error "Identity element not found in manifest."
    exit 1
}
# Set the Version attribute on Identity
$identity.SetAttribute('Version', $manifestVersion)

# Save the modified manifest back
$xml.Save($manifestPath)

Write-Host "Manifest version set to $manifestVersion"
