<#
  sign.ps1 - Authenticode-sign the built SimpleCamera .asi files.

  Why: Windows (SmartScreen / Smart App Control / Defender) blocks unsigned DLLs
  that another process - e.g. the FiveM game subprocess - tries to load, with
  "we can't confirm who published ...". FiveM then reports "Couldn't load X.asi".
  Signing with a certificate the target PC trusts removes that block.

  This script:
    1. Reuses (or creates) a self-signed code-signing cert in CurrentUser\My.
    2. Exports its PUBLIC half to dist\SimpleCamera_CodeSign.cer - install THAT
       on the machine that runs FiveM (see dist\INSTALL_FIVEM.md).
    3. Signs the .asi files with SHA256 + an RFC3161 timestamp.

  Run after building:  powershell -ExecutionPolicy Bypass -File sign.ps1
#>

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

# --- 1. cert (reuse by subject, else create, valid 5 years) ---
$subject = "CN=SimpleCamera Mod, O=SimpleCamera, C=US"
$cert = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert -ErrorAction SilentlyContinue |
        Where-Object { $_.Subject -eq $subject } | Select-Object -First 1
if (-not $cert) {
  Write-Host "Creating self-signed code-signing certificate..."
  $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $subject `
            -KeyUsage DigitalSignature -KeyExportPolicy Exportable `
            -CertStoreLocation Cert:\CurrentUser\My `
            -NotAfter (Get-Date).AddYears(5) -FriendlyName "SimpleCamera Mod Signing"
} else {
  Write-Host "Reusing certificate $($cert.Thumbprint)"
}

# --- 2. export public cert for the gaming PC ---
$dist = Join-Path $root "dist"
New-Item -ItemType Directory -Force -Path $dist | Out-Null
Export-Certificate -Cert $cert -FilePath (Join-Path $dist "SimpleCamera_CodeSign.cer") -Force | Out-Null

# --- locate signtool (newest x64) ---
$signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '\\x64\\' } |
            Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if (-not $signtool) { throw "signtool.exe not found (install the Windows 10/11 SDK)." }

# --- 3. sign whichever builds exist ---
$targets = @(
  (Join-Path $root "bin\Release_FiveM\SimpleCamera_FiveM.asi"),
  (Join-Path $root "bin\Release\SimpleCamera.asi")
) | Where-Object { Test-Path $_ }
if (-not $targets) { throw "No built .asi found. Build the solution first." }

foreach ($t in $targets) {
  & $signtool sign /sha1 $cert.Thumbprint /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $t
  if ($LASTEXITCODE -ne 0) { throw "signing failed for $t" }
}
Write-Host "Done. Signed: $($targets -join ', ')"
Write-Host "Install dist\SimpleCamera_CodeSign.cer on the FiveM PC (see dist\INSTALL_FIVEM.md)."
