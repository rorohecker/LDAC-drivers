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

if (!(Test-Path -LiteralPath $PackageDir -PathType Container)) { throw "Missing package directory: $PackageDir" }
$PackageDir = (Resolve-Path -LiteralPath $PackageDir).ProviderPath
$sys = Join-Path $PackageDir 'xm5ldac.sys'
$inf = Join-Path $PackageDir 'xm5ldac.inf'
if (!(Test-Path $sys)) { throw "Missing $sys" }
if (!(Test-Path $inf)) { throw "Missing $inf" }

if ([string]::IsNullOrWhiteSpace($CertPath)) {
    $CertPath = Join-Path $projectRoot 'package\xm5ldac-test.cer'
}
$CertPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($CertPath)
$certDir = Split-Path $CertPath -Parent
if (!(Test-Path $certDir)) { New-Item -ItemType Directory -Path $certDir | Out-Null }

$cert = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert -ErrorAction SilentlyContinue |
    Where-Object { $_.Subject -eq $CertSubject -and $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date) -and $_.NotBefore -le (Get-Date) } |
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
if (-not $inf2cat) { throw 'Inf2Cat.exe not found under Windows Kits. Install the WDK before signing.' }

Push-Location $PackageDir
try {
    & $signtool.FullName sign /fd SHA256 /td SHA256 /tr http://timestamp.digicert.com /sha1 $cert.Thumbprint $sys
    if ($LASTEXITCODE) {
        Write-Warning 'Timestamped sign failed; retrying without timestamp.'
        & $signtool.FullName sign /fd SHA256 /sha1 $cert.Thumbprint $sys
        if ($LASTEXITCODE) { throw 'signtool failed for xm5ldac.sys' }
    }

    & $inf2cat.FullName /driver:. /os:10_CO_X64
    if ($LASTEXITCODE) { throw 'Inf2Cat failed.' }
    $cat = Join-Path $PackageDir 'xm5ldac.cat'
    if (!(Test-Path -LiteralPath $cat)) { throw "Inf2Cat did not create the required catalog: $cat" }
    & $signtool.FullName sign /fd SHA256 /sha1 $cert.Thumbprint $cat
    if ($LASTEXITCODE) { throw 'signtool failed for xm5ldac.cat' }
} finally {
    Pop-Location
}

Write-Output 'Test signing complete.'
Write-Output 'This test-signed package is for an explicitly opted-in lab, not the normal-security Windows path. See docs/windows-ldac-paths.md.'
