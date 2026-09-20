param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$PackageDir = '',
    [string]$InfPath = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if ([string]::IsNullOrWhiteSpace($PackageDir)) {
    $PackageDir = Join-Path $projectRoot 'package\test'
}
if ([string]::IsNullOrWhiteSpace($InfPath)) {
    $InfPath = Join-Path $projectRoot 'driver\xm5ldac.inf'
}

$sys = Join-Path $projectRoot "driver\out\x64\$Configuration\xm5ldac.sys"
$client = Join-Path $projectRoot "build\$Configuration\xm5_stream.exe"
$notice = Join-Path $projectRoot 'vendor\libldac\NOTICE'
$license = Join-Path $projectRoot 'vendor\libldac\LICENSE'

if (!(Test-Path $sys)) { throw "Missing driver binary: $sys (run build.ps1 -IncludeDriver first)" }
if (!(Test-Path $InfPath)) { throw "Missing INF: $InfPath (run generate-inf.ps1 first)" }
if ((Get-Content -Raw $InfPath) -match '__HEADPHONE_SERVICE_HARDWARE_ID__') {
    throw 'INF still contains the placeholder. Run generate-inf.ps1 with a real HardwareId.'
}
if (!(Test-Path $client)) {
    Write-Warning "xm5_stream.exe not found at $client - package will omit the client."
}

$PackageDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($PackageDir)
$packageRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'package')).TrimEnd('\') + '\'
if (-not $PackageDir.StartsWith($packageRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "PackageDir must be a subdirectory of $packageRoot"
}
# Do not recursively remove an arbitrary caller-provided directory. Replace only
# the package files this helper owns, leaving any unrelated files untouched.
New-Item -ItemType Directory -Path $PackageDir -Force | Out-Null
foreach ($name in @('xm5ldac.sys', 'xm5ldac.inf', 'xm5ldac.cat', 'xm5_stream.exe', 'LICENSE.libldac', 'NOTICE.libldac', 'README.txt')) {
    $packageFile = Join-Path $PackageDir $name
    if (Test-Path -LiteralPath $packageFile -PathType Leaf) {
        Remove-Item -LiteralPath $packageFile -Force
    }
}

Copy-Item $sys (Join-Path $PackageDir 'xm5ldac.sys')
Copy-Item $InfPath (Join-Path $PackageDir 'xm5ldac.inf')
if (Test-Path $client) { Copy-Item $client (Join-Path $PackageDir 'xm5_stream.exe') }
if (Test-Path $license) { Copy-Item $license (Join-Path $PackageDir 'LICENSE.libldac') }
if (Test-Path $notice) { Copy-Item $notice (Join-Path $PackageDir 'NOTICE.libldac') }

$readme = @"
XM5 LDAC experimental test package ($Configuration)
Generated: $(Get-Date -Format o)

Install (elevated PowerShell, from repo root):
  .\tools\sign-test.ps1 -PackageDir '$PackageDir'
  .\tools\install-test.ps1 -PackageDir '$PackageDir'

Smoke:
  .\tools\smoke-info.ps1 -Address AA:BB:CC:DD:EE:FF

Uninstall:
  .\tools\uninstall-test.ps1

This package is not production-signed. Testsigning must be enabled.
"@
Set-Content -Path (Join-Path $PackageDir 'README.txt') -Value $readme -Encoding utf8

Write-Output "Package assembled at $PackageDir"
Get-ChildItem $PackageDir | ForEach-Object { Write-Output ("  {0}  {1}" -f $_.Length, $_.Name) }
