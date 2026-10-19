param(
    [Parameter(Mandatory = $true)][string]$PackageDir,
    [Parameter(Mandatory = $true)][string]$ConfirmedInstanceId,
    [switch]$Install
)
$ErrorActionPreference = 'Stop'

function Assert-NormalBootSecurity {
    $boot = & bcdedit.exe /enum '{current}' 2>&1
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read boot configuration; refusing driver package.' }
    $integrityLines = @($boot | Where-Object { $_ -match '(?i)^\s*(testsigning|nointegritychecks)\s+' })
    if (@($integrityLines | Where-Object { $_ -notmatch '(?i)\s+No\s*$' }).Count -gt 0 -or
        $boot -match '(?im)^\s*loadoptions\s+.*DISABLE_INTEGRITY_CHECKS') {
        throw 'A code-integrity bypass boot option is present. This path requires normal Windows code integrity.'
    }
    try { $secureBoot = Confirm-SecureBootUEFI -ErrorAction Stop }
    catch { throw 'Cannot verify Secure Boot state; refusing driver package.' }
    if (-not $secureBoot) { throw 'Secure Boot is disabled; refusing driver package.' }
}
function Assert-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'An elevated Administrator shell is required to verify boot security. No package was installed.'
    }
}

if (!(Test-Path -LiteralPath $PackageDir -PathType Container)) { throw "Missing package directory: $PackageDir" }
$PackageDir = (Resolve-Path -LiteralPath $PackageDir).ProviderPath
$inf = Join-Path $PackageDir 'xm5ldac.inf'
$sys = Join-Path $PackageDir 'xm5ldac.sys'
$cat = Join-Path $PackageDir 'xm5ldac.cat'
foreach ($file in @($inf, $sys, $cat)) {
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing package file: $file" }
}
if ($ConfirmedInstanceId -notmatch '^BTHENUM\\' -or $ConfirmedInstanceId -match '[*?\[\]]') {
    throw 'Expected one exact BTHENUM headset service instance ID, without wildcards.'
}

$infText = Get-Content -Raw -LiteralPath $inf
$manufacturerSections = [regex]::Matches($infText, '(?im)^\[Manufacturer\][ \t]*\r?$')
$modelSections = [regex]::Matches($infText, '(?im)^\[Models(?:\.[^\]]*)?\][ \t]*\r?$')
if ($manufacturerSections.Count -ne 1 -or $modelSections.Count -ne 1) {
    throw 'Expected exactly one manufacturer section and one model section in the INF.'
}
$manufacturer = [regex]::Match($infText, '(?ims)^\[Manufacturer\][ \t]*\r?\n(?<lines>.*?)(?=^\[|\z)')
$manufacturerLines = @($manufacturer.Groups['lines'].Value -split '\r?\n' |
    ForEach-Object { ($_ -split ';', 2)[0].Trim() } | Where-Object { $_ })
if ($manufacturerLines.Count -ne 1 -or
    $manufacturerLines[0] -notmatch '(?i)^%Provider%\s*=\s*Models\s*,\s*NTamd64\.10\.0\.\.\.22000\s*$') {
    throw 'The INF manufacturer must reference only the Windows 11 x64 model section.'
}
$models = [regex]::Match($infText, '(?ims)^\[Models\.NTamd64\.10\.0\.\.\.22000\][ \t]*\r?\n(?<lines>.*?)(?=^\[|\z)')
if (-not $models.Success) { throw 'Missing supported Windows 11 x64 model section.' }
$modelLines = @($models.Groups['lines'].Value -split '\r?\n' |
    ForEach-Object { ($_ -split ';', 2)[0].Trim() } | Where-Object { $_ })
if ($modelLines.Count -ne 1) { throw 'Expected exactly one Audio Sink model in the INF.' }
$binding = [regex]::Match($modelLines[0], '(?i)^%Description%\s*=\s*Install\s*,\s*(?<id>[^,\s]+)\s*$')
$hardwareId = $binding.Groups['id'].Value
if (-not $binding.Success -or
    $hardwareId -notmatch '^BTHENUM\\\{0000110b-0000-1000-8000-00805f9b34fb\}_VID&[A-Fa-f0-9]+_PID&[A-Fa-f0-9]+(?:_[A-Za-z0-9]+&[A-Fa-f0-9]+)*\z') {
    throw 'The INF must target one specific Bluetooth Audio Sink VID/PID; a generic service ID is unsafe.'
}

$device = Get-PnpDevice -PresentOnly -InstanceId $ConfirmedInstanceId -ErrorAction Stop
if (-not $device -or @($device).Count -ne 1 -or $device.InstanceId -ne $ConfirmedInstanceId) {
    throw 'Confirmed device instance is not present or does not match exactly.'
}
$properties = Get-PnpDeviceProperty -InstanceId $device.InstanceId -ErrorAction Stop
$deviceIds = @(($properties | Where-Object KeyName -eq 'DEVPKEY_Device_HardwareIds').Data)
if ($hardwareId -notin $deviceIds) { throw 'INF Audio Sink hardware ID does not match the confirmed device instance.' }
$currentService = ($properties | Where-Object KeyName -eq 'DEVPKEY_Device_Service').Data
if ($currentService -notin @('BthA2dp', 'xm5ldac')) {
    throw "Unexpected current service '$currentService' on the confirmed Audio Sink. Review its binding before installation."
}
# PnPUtil /install targets every matching present device, not just the instance
# supplied above. Refuse the package if it could change another Audio Sink.
$matchingDevices = @(Get-PnpDevice -PresentOnly -ErrorAction Stop | Where-Object {
    $_.InstanceId -like 'BTHENUM\*'
} | Where-Object {
    $ids = @((Get-PnpDeviceProperty -InstanceId $_.InstanceId -KeyName 'DEVPKEY_Device_HardwareIds' -ErrorAction Stop).Data)
    $hardwareId -in $ids
})
if ($matchingDevices.Count -ne 1 -or $matchingDevices[0].InstanceId -ne $ConfirmedInstanceId) {
    throw 'The INF hardware ID matches another present device, or the confirmed instance was not unique. PnPUtil /install would affect all matches.'
}

Assert-IsAdministrator
Assert-NormalBootSecurity

$kitBin = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
$signTool = Get-ChildItem -LiteralPath $kitBin -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
    Sort-Object FullName -Descending | Select-Object -First 1
if (-not $signTool) { throw 'SignTool from the Windows SDK is required to verify the package.' }
foreach ($file in @($inf, $sys)) {
    & $signTool.FullName verify /kp /c $cat $file
    if ($LASTEXITCODE -ne 0) { throw "Kernel-policy signature verification failed for $file" }
}
$catalogSignature = Get-AuthenticodeSignature -LiteralPath $cat -ErrorAction Stop
if ($catalogSignature.Status -ne 'Valid' -or -not $catalogSignature.SignerCertificate) {
    throw 'The driver catalog has no valid trusted signature.'
}
$publisher = $catalogSignature.SignerCertificate.GetNameInfo(
    [Security.Cryptography.X509Certificates.X509NameType]::SimpleName, $false)
if ($publisher -ne 'Microsoft Windows Hardware Compatibility Publisher') {
    throw "The catalog is not signed by Microsoft Windows Hardware Compatibility Publisher: $publisher"
}

Write-Output "Verified signed XM5 Audio Sink package: $PackageDir"
Write-Output "Catalog publisher: $publisher"
Write-Output "Confirmed instance: $ConfirmedInstanceId"
Write-Output "Device name: $($device.FriendlyName)"
Write-Output "Current service: $currentService"
Write-Output "Exact hardware ID: $hardwareId"
if (-not $Install) {
    Write-Output 'Preflight only. No driver, boot option, certificate, or adapter was changed.'
    Write-Output 'After hardware binding and rollback are reviewed, rerun with -Install from an elevated shell.'
    return
}

& pnputil.exe /add-driver $inf /install
if ($LASTEXITCODE -ne 0) { throw "PnPUtil installation failed with exit code $LASTEXITCODE" }
$installedService = (Get-PnpDeviceProperty -InstanceId $ConfirmedInstanceId -KeyName 'DEVPKEY_Device_Service' -ErrorAction Stop).Data
if ($installedService -ne 'xm5ldac') {
    throw "Package was staged, but the confirmed Audio Sink is still bound to '$installedService'. PnPUtil does not force a lower-ranked driver. No successful installation is claimed. Review Device Manager and the package rank before any manual binding change."
}
Write-Output 'The confirmed XM5 Audio Sink is bound to xm5ldac. Run the IOCTL smoke check before attempting playback.'
