param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir,
    [switch]$SkipTestSigningEnable
)
$ErrorActionPreference = 'Stop'

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}
if (-not (Test-IsAdmin)) { throw 'install-test.ps1 must run elevated (Administrator).' }

$inf = Join-Path $PackageDir 'xm5ldac.inf'
$sys = Join-Path $PackageDir 'xm5ldac.sys'
if (!(Test-Path $inf)) { throw "Missing $inf" }
if (!(Test-Path $sys)) { throw "Missing $sys" }
if ((Get-Content -Raw $inf) -match '__HEADPHONE_SERVICE_HARDWARE_ID__') {
    throw 'Refusing to install INF that still has the placeholder HardwareId.'
}

Write-Warning @'
This installs an experimental Bluetooth PROFILE driver for the headphone Audio Sink service.
It must not replace the Intel radio driver. Inbox A2DP on the XM5 sink will be displaced.
Use only on a development machine. Uninstall with tools/uninstall-test.ps1.
'@

if (-not $SkipTestSigningEnable) {
    $testsigning = (bcdedit /enum '{current}' | Select-String -Pattern 'testsigning\s+Yes')
    if (-not $testsigning) {
        Write-Output 'Enabling testsigning (reboot required before the driver will load)...'
        bcdedit /set testsigning on
        if ($LASTEXITCODE) { throw 'bcdedit failed to enable testsigning.' }
        Write-Warning 'Reboot now, then re-run this script with -SkipTestSigningEnable after reboot.'
        return
    }
}

$cert = Join-Path (Split-Path (Split-Path $PackageDir -Parent) -Parent) 'package\xm5ldac-test.cer'
# Prefer cert next to package parent package\xm5ldac-test.cer from repo layout.
$repoCert = Join-Path (Split-Path $PSScriptRoot -Parent) 'package\xm5ldac-test.cer'
if (Test-Path $repoCert) {
    Write-Output "Importing test cert into Local Machine Root + TrustedPublisher: $repoCert"
    Import-Certificate -FilePath $repoCert -CertStoreLocation 'Cert:\LocalMachine\Root' | Out-Null
    Import-Certificate -FilePath $repoCert -CertStoreLocation 'Cert:\LocalMachine\TrustedPublisher' | Out-Null
} elseif (Test-Path $cert) {
    Import-Certificate -FilePath $cert -CertStoreLocation 'Cert:\LocalMachine\Root' | Out-Null
    Import-Certificate -FilePath $cert -CertStoreLocation 'Cert:\LocalMachine\TrustedPublisher' | Out-Null
} else {
    Write-Warning 'Test certificate .cer not found. If pnputil rejects the package, re-run sign-test.ps1 and import the cert manually.'
}

Write-Output "Adding driver package via pnputil: $inf"
pnputil /add-driver $inf /install
if ($LASTEXITCODE) { throw "pnputil /add-driver failed with exit $LASTEXITCODE" }

Write-Output 'Install requested. Check Device Manager → Bluetooth for the XM5 sink service.'
Write-Output 'Then: .\tools\smoke-info.ps1 -Address <XM5 Bluetooth address>'
