param(
    [string]$BridgeCorePath = '',
    [string]$AdapterInstanceId = ''
)
$ErrorActionPreference = 'Stop'

# Read-only inventory for the dedicated-adapter path. This script never binds
# WinUSB, starts a Bluetooth session, installs a package, or changes boot policy.
$core = $null
if ($BridgeCorePath) {
    if (!(Test-Path -LiteralPath $BridgeCorePath -PathType Leaf)) {
        throw "A2DP Windows Bridge core not found: $BridgeCorePath"
    }
    $core = (Resolve-Path -LiteralPath $BridgeCorePath).ProviderPath
} else {
    $command = Get-Command 'a2dpwb-core.exe' -ErrorAction SilentlyContinue
    if ($command) { $core = $command.Source }
}

$testSigning = $null
$codeIntegrityBypass = $null
try {
    $boot = & bcdedit.exe /enum '{current}' 2>&1
    if ($LASTEXITCODE -eq 0) {
        $testSigning = $false
        $codeIntegrityBypass = $false
        $testLines = @($boot | Where-Object { $_ -match '(?i)^\s*testsigning\s+' })
        if ($testLines.Count -gt 1 -or @($testLines | Where-Object { $_ -notmatch '(?i)\s+(Yes|No)\s*$' }).Count) {
            $testSigning = $null
        } elseif (@($testLines | Where-Object { $_ -match '(?i)\s+Yes\s*$' }).Count) {
            $testSigning = $true
        }
        $integrityLines = @($boot | Where-Object { $_ -match '(?i)^\s*(testsigning|nointegritychecks)\s+' })
        if (@($integrityLines | Where-Object { $_ -match '(?i)\s+Yes\s*$' }).Count -or
            $boot -match '(?im)^\s*loadoptions\s+.*DISABLE_INTEGRITY_CHECKS') {
            $codeIntegrityBypass = $true
        } elseif (@($integrityLines | Where-Object { $_ -notmatch '(?i)\s+(Yes|No)\s*$' }).Count) {
            $codeIntegrityBypass = $null
        }
    }
} catch { }
try { $secureBoot = [bool](Confirm-SecureBootUEFI -ErrorAction Stop) }
catch { $secureBoot = $null }

$radios = @(Get-PnpDevice -PresentOnly -ErrorAction Stop | Where-Object {
    $_.InstanceId -like 'USB\*' -and $_.Class -in @('Bluetooth', 'USBDevice')
} | ForEach-Object {
    $device = $_
    $properties = Get-PnpDeviceProperty -InstanceId $device.InstanceId -ErrorAction SilentlyContinue
    $compatibleIds = @((($properties | Where-Object KeyName -eq 'DEVPKEY_Device_CompatibleIds').Data))
    $bluetoothClass = [bool](@($compatibleIds | Where-Object { $_ -match '^USB\\Class_E0&SubClass_01&Prot_01$' }).Count)
    [pscustomobject]@{
        Name = $device.FriendlyName
        InstanceId = $device.InstanceId
        Class = $device.Class
        Status = $device.Status
        Service = ($properties | Where-Object KeyName -eq 'DEVPKEY_Device_Service').Data
        HardwareIds = @((($properties | Where-Object KeyName -eq 'DEVPKEY_Device_HardwareIds').Data))
        CompatibleIds = $compatibleIds
        BluetoothLike = ($device.Class -eq 'Bluetooth' -or $bluetoothClass -or
            $device.FriendlyName -match '(?i)Bluetooth.*(adapter|radio|dongle)')
    }
})

$selected = $null
if ($AdapterInstanceId) {
    $selected = @($radios | Where-Object InstanceId -eq $AdapterInstanceId)
    if ($selected.Count -ne 1) { throw 'Selected adapter was not found in the present USB device inventory.' }
    $selected = $selected[0]
    if ($selected.Name -match '(?i)Intel.*Bluetooth' -or $selected.InstanceId -match '(?i)^USB\\VID_8087') {
        throw 'The selected adapter appears to be the built-in Intel radio. Choose a physically separate USB adapter.'
    }
    if (-not $selected.BluetoothLike) {
        throw 'Selected USB device is not identifiable as a Bluetooth adapter. Inspect its hardware and compatible IDs before using any driver assignment tool.'
    }
}

[pscustomobject]@{
    BridgeCore = $core
    SecureBoot = $secureBoot
    TestSigning = $testSigning
    CodeIntegrityBypass = $codeIntegrityBypass
    SelectedAdapter = $selected
    UsbDeviceInventory = $radios
    Note = 'Read-only inventory. Unknown security values require an elevated check. Physically confirm an external adapter before assigning WinUSB in A2DP Windows Bridge. Do not select the built-in radio.'
} | ConvertTo-Json -Depth 6
