param([switch]$IncludeDriver)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Install Visual Studio C++ build tools first.' }
# Prefer the real VS installation path when available, but be resilient across VS 2022/2026 layouts and older vswhere versions.
$vsRoots = @()
try {
    $vsRoots += & $vswhere -latest -products '*' -property installationPath | Select-Object -First 1
} catch {}
$vsRoots += 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools'
$vsRoots += 'C:\Program Files\Microsoft Visual Studio\18\BuildTools'
$vsRoot = $vsRoots | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path $_) } | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($vsRoot)) { throw 'Visual Studio C++ tools were not found.' }
$cmakeCandidates = @(
    (Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source),
    (Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'),
    'C:\Program Files\CMake\bin\cmake.exe',
    'C:\Program Files (x86)\CMake\bin\cmake.exe'
)
$cmakePath = $cmakeCandidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path $_) } | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($cmakePath)) { throw 'CMake was not found. Add the C++ CMake tools component in Visual Studio Installer.' }
$buildRoot = Join-Path $projectRoot 'build'
if (Test-Path $buildRoot) { Remove-Item $buildRoot -Recurse -Force }
# Force a Visual Studio generator; the default NMake generator rejects the -A x64 switch and the project requires the MSVC toolchain.
$generator = 'Visual Studio 18 2026'
& $cmakePath -S $projectRoot -B $buildRoot -G $generator -A x64
if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
& $cmakePath --build $buildRoot --config Debug
if ($LASTEXITCODE) { throw 'User-mode build failed.' }
$ctest = Join-Path (Split-Path $cmakePath -Parent) 'ctest.exe'
& $ctest --test-dir $buildRoot -C Debug --output-on-failure
if ($LASTEXITCODE) { throw 'Tests failed.' }
if ($IncludeDriver) {
    $msbuild = Join-Path $vsRoot 'MSBuild\Current\Bin\MSBuild.exe'
    & $msbuild (Join-Path $projectRoot 'driver\xm5ldac.vcxproj') '/p:Configuration=Debug' '/p:Platform=x64' '/m'
    if ($LASTEXITCODE) { throw 'WDK driver build failed. Save this build output for diagnosis.' }
}
Write-Output 'Build checks completed. This script does not install or bind a driver.'
