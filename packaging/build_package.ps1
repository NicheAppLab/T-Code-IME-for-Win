Param(
    [Parameter(Mandatory=$true)]
    [string]$Version,
    [string]$PackageName = "NicheAppLab.T-CodeIME",
    [string]$PublisherGuid = "2BF2953C-66FF-4938-ACE4-42FB3950D28C",
    [string]$DisplayName = "T-Code IME",
    [string]$PublisherDisplayName = "Niche App Lab",
    [Parameter(Mandatory=$false)]
    [string]$Publisher = "Niche App Lab"
)

[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
# Ensure version has four components as required by appx manifest (e.g., 0.1.0.0)
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
    # Set Identity attributes
    $identity.SetAttribute('Name', $PackageName)
    $identity.SetAttribute('Publisher', "CN=$PublisherGuid")
    $identity.SetAttribute('Version', $manifestVersion)
    # Update display names
    if ($xml.Package.Properties) {
        $xml.Package.Properties.DisplayName = $DisplayName
        $xml.Package.Properties.PublisherDisplayName = $PublisherDisplayName
    } else {
        # Fallback: add Properties element if missing
        $props = $xml.CreateElement('Properties')
        $display = $xml.CreateElement('DisplayName')
        $display.InnerText = $DisplayName
        $props.AppendChild($display) | Out-Null
        $pubDisp = $xml.CreateElement('PublisherDisplayName')
        $pubDisp.InnerText = $DisplayName
        $props.AppendChild($pubDisp) | Out-Null
        $xml.Package.AppendChild($props) | Out-Null
    }

# Save the modified manifest back
$xml.Save($manifestPath)

Write-Host "Manifest version set to $manifestVersion"
# Package the app into an MSIX using winapp CLI
# The layout folder contains the compiled DLL and assets.
# --generate-cert creates a temporary dev certificate for signing.
# Output will be placed in the 'output' subfolder.
$layoutPath = Join-Path $PSScriptRoot "layout"
$outputPath = Join-Path $PSScriptRoot "output"
# Ensure output directory exists
if (-not (Test-Path $outputPath)) { New-Item -ItemType Directory -Path $outputPath | Out-Null }
# Run winapp packaging
winapp package $layoutPath --manifest $manifestPath --output (Join-Path $outputPath "TCodeIME.msix") --generate-cert -v