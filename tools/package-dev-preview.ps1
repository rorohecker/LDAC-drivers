param(
    [ValidatePattern('^v[0-9]+\.[0-9]+\.[0-9]+-dev\.[0-9]+$')]
    [string]$Version = 'v0.1.0-dev.2',
    [string]$OutputDir = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root 'build\release'
}
$OutputDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$archiveName = "XM5-LDAC-$Version-win64-dev-tools.zip"
$archive = Join-Path $OutputDir $archiveName
$checksum = "$archive.sha256"
if ((Test-Path -LiteralPath $archive) -or (Test-Path -LiteralPath $checksum)) {
    throw 'Release archive or checksum already exists; choose an empty output directory.'
}

$stage = Join-Path $OutputDir ("stage-$Version-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
$files = @(
    @{ Source = 'build\Release\ldac_encode.exe'; Name = 'ldac_encode.exe' },
    @{ Source = 'build\Release\xm5_stream.exe'; Name = 'xm5_stream.exe' },
    @{ Source = 'tools\gen_pcm.py'; Name = 'gen_pcm.py' },
    @{ Source = 'tools\setup-usb.ps1'; Name = 'setup-usb.ps1' },
    @{ Source = 'tools\bridge-readiness.ps1'; Name = 'bridge-readiness.ps1' },
    @{ Source = 'vendor\libldac\LICENSE'; Name = 'LICENSE.libldac' },
    @{ Source = 'vendor\libldac\NOTICE'; Name = 'NOTICE.libldac' }
)
foreach ($item in $files) {
    $source = Join-Path $root $item.Source
    if (!(Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing release input: $source" }
    Copy-Item -LiteralPath $source -Destination (Join-Path $stage $item.Name)
}
$readme = Join-Path $root 'docs\dev-preview-readme.md'
if (!(Test-Path -LiteralPath $readme -PathType Leaf)) { throw "Missing release README: $readme" }
$content = (Get-Content -Raw -LiteralPath $readme).Replace('__VERSION__', $Version)
Set-Content -LiteralPath (Join-Path $stage 'README.md') -Value $content -Encoding utf8

Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -CompressionLevel Optimal
$digest = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath $checksum -Value "$digest  $archiveName" -Encoding ascii
Write-Output "Archive: $archive"
Write-Output "SHA-256: $digest"
Write-Output "Checksum file: $checksum"
