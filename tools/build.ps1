param(
    [switch]$IncludeDriver,
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$HardwareId = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent

function Find-VsInstallationPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (!(Test-Path $vswhere)) { throw 'Install Visual Studio C++ build tools first.' }
    $path = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($path)) {
        $path = & $vswhere -latest -products '*' -property installationPath 2>$null | Select-Object -First 1
    }
    $fallbacks = @(
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools',
        'C:\Program Files\Microsoft Visual Studio\2022\Community',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional',
        'C:\Program Files\Microsoft Visual Studio\18\BuildTools',
        'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools'
    )
    foreach ($candidate in @($path) + $fallbacks) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path $candidate)) { return $candidate }
    }
    throw 'Visual Studio C++ tools were not found.'
}

function Find-CMake([string]$vsRoot) {
    $candidates = @(
        (Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source),
        (Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'),
        'C:\Program Files\CMake\bin\cmake.exe',
        'C:\Program Files (x86)\CMake\bin\cmake.exe'
    )
    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path $candidate)) { return $candidate }
    }
    throw 'CMake was not found. Add the C++ CMake tools component in Visual Studio Installer.'
}

function Resolve-CMakeGenerator([string]$vsRoot) {
    # Prefer generators that match the installed VS major version; fall back through known names.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $major = $null
    if (Test-Path $vswhere) {
        $major = & $vswhere -latest -products '*' -property installationVersion 2>$null | Select-Object -First 1
        if ($major -match '^(\d+)\.') { $major = [int]$Matches[1] } else { $major = $null }
    }
    $ordered = @()
    if ($major -eq 18) { $ordered += 'Visual Studio 18 2026' }
    if ($major -eq 17) { $ordered += 'Visual Studio 17 2022' }
    $ordered += @('Visual Studio 17 2022', 'Visual Studio 18 2026', 'Visual Studio 16 2019')
    $unique = $ordered | Select-Object -Unique
    return $unique[0]
}

$vsRoot = Find-VsInstallationPath
$cmakePath = Find-CMake $vsRoot
$generator = Resolve-CMakeGenerator $vsRoot
$buildRoot = Join-Path $projectRoot 'build'

Write-Output "Using VS: $vsRoot"
Write-Output "Using CMake: $cmakePath"
Write-Output "Generator: $generator"
Write-Output "Configuration: $Configuration"

if (!(Test-Path $buildRoot)) { New-Item -ItemType Directory -Path $buildRoot | Out-Null }

& $cmakePath -S $projectRoot -B $buildRoot -G $generator -A x64
if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
& $cmakePath --build $buildRoot --config $Configuration
if ($LASTEXITCODE) { throw 'User-mode build failed.' }

$ctest = Join-Path (Split-Path $cmakePath -Parent) 'ctest.exe'
& $ctest --test-dir $buildRoot -C $Configuration --output-on-failure
if ($LASTEXITCODE) { throw 'Tests failed.' }

if ($IncludeDriver) {
    $msbuild = Join-Path $vsRoot 'MSBuild\Current\Bin\MSBuild.exe'
    if (!(Test-Path $msbuild)) { throw "MSBuild not found at $msbuild" }
    $proj = Join-Path $projectRoot 'driver\xm5ldac.vcxproj'
    & $msbuild $proj "/p:Configuration=$Configuration" '/p:Platform=x64' '/m'
    if ($LASTEXITCODE) { throw 'WDK driver build failed. Save this build output for diagnosis.' }
    $sys = Join-Path $projectRoot "driver\out\x64\$Configuration\xm5ldac.sys"
    if (!(Test-Path $sys)) { throw "Driver build reported success but $sys is missing." }
    Write-Output "Driver binary: $sys"
}

if (-not [string]::IsNullOrWhiteSpace($HardwareId)) {
    & (Join-Path $PSScriptRoot 'generate-inf.ps1') -HardwareId $HardwareId
}

Write-Output 'Build checks completed. This script does not install or bind a driver.'
Write-Output 'Next: hardware-report.ps1 → resolve-binding.ps1 → generate-inf.ps1 → package-test.ps1 → sign-test.ps1 → install-test.ps1'
