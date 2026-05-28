$pfxFile = "C:\Users\imuno\VSCodeProjects\T-Code-IME-for-Win\packaging\output\TCodeIME_cert.pfx"
$cerFile = "C:\Users\imuno\VSCodeProjects\T-Code-IME-for-Win\packaging\output\TCodeIME_cert.cer"

$cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2
$cert.Import($pfxFile, "password", [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::Exportable)
$bytes = $cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert)
[System.IO.File]::WriteAllBytes($cerFile, $bytes)

Write-Host "Public CER file successfully generated at: $cerFile" -ForegroundColor Green
