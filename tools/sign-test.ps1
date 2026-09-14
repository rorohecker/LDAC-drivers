param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir,
    [string]$CertSubject = 'CN=XM5 LDAC Test Signing',
    [string]$CertPath = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}
if (-not (Test-IsAdmin)) { throw 'sign-test.ps1 must run elevated (Administrator).' }

$sys = Join-Path $PackageDir 'xm5ldac.sys'
$inf = Join-Path $PackageDir 'xm5ldac.inf'
if (!(Test-Path $sys)) { throw "Missing $sys" }
if (!(Test-Path $inf)) { throw "Missing $inf" }

if ([string]::IsNullOrWhiteSpace($CertPath)) {
    $CertPath = Join-Path $projectRoot 'package\xm5ldac-test.cer'
}
$certDir = Split-Path $CertPath -Parent
if (!(Test-Path $certDir)) { New-Item -ItemType Directory -Path $certDir | Out-Null }

$cert = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert -ErrorAction SilentlyContinue |
    Where-Object { $_.Subject -eq $CertSubject } |
    Select-Object -First 1
if (-not $cert) {
    Write-Output "Creating self-signed test certificate: $CertSubject"
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $CertSubject -CertStoreLocation 'Cert:\CurrentUser\My'
}
Export-Certificate -Cert $cert -FilePath $CertPath -Force | Out-Null
Write-Output "Exported public cert: $CertPath"
Write-Output 'Import this cert into Local Machine Trusted Root and Trusted Publisher if signtool/Inf2Cat require it.'

$signtool = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if (-not $signtool) { throw 'signtool.exe not found under Windows Kits. Install the Windows SDK.' }

$inf2cat = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Filter Inf2Cat.exe -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x86\\Inf2Cat\.exe$' -or $_.FullName -match '\\x64\\Inf2Cat\.exe$' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1

Push-Location $PackageDir
try {
    if ($inf2cat) {
        & $inf2cat.FullName /driver:. /os:10_X64
        if ($LASTEXITCODE) { throw 'Inf2Cat failed.' }
    } else {
        Write-Warning 'Inf2Cat.exe not found; continuing with sys signature only. Catalog may be required on some systems.'
    }

    & $signtool.FullName sign /fd SHA256 /td SHA256 /tr http://timestamp.digicert.com /a $sys
    if ($LASTEXITCODE) {
        Write-Warning 'Timestamped sign failed; retrying without timestamp.'
        & $signtool.FullName sign /fd SHA256 /a $sys
        if ($LASTEXITCODE) { throw 'signtool failed for xm5ldac.sys' }
    }

    $cat = Join-Path $PackageDir 'xm5ldac.cat'
    if (Test-Path $cat) {
        & $signtool.FullName sign /fd SHA256 /a $cat
        if ($LASTEXITCODE) { throw 'signtool failed for xm5ldac.cat' }
    }
} finally {
    Pop-Location
}

Write-Output 'Test signing complete.'
Write-Output "Next: .\tools\install-test.ps1 -PackageDir '$PackageDir'"
