# Read-only report. No adapter, driver, security, or audio settings are changed.
$ErrorActionPreference = 'Stop'
$devices = Get-PnpDevice -PresentOnly | Where-Object {
    $_.FriendlyName -like '*WH-1000XM5*' -or
    $_.FriendlyName -like '*XM5*' -or
    $_.FriendlyName -like '*Sony*' -or
    $_.FriendlyName -like '*Intel*Bluetooth*' -or
    $_.InstanceId -like 'BTHENUM\*'
}

$devices | Sort-Object InstanceId -Unique | ForEach-Object {
    $device = $_
    $properties = Get-PnpDeviceProperty -InstanceId $device.InstanceId -ErrorAction SilentlyContinue
    [pscustomobject]@{
        Name = $device.FriendlyName
        InstanceId = $device.InstanceId
        Status = $device.Status
        HardwareIds = ($properties | Where-Object KeyName -eq 'DEVPKEY_Device_HardwareIds').Data
        Service = ($properties | Where-Object KeyName -eq 'DEVPKEY_Device_Service').Data
        DriverVersion = ($properties | Where-Object KeyName -eq 'DEVPKEY_Device_DriverVersion').Data
        DriverInf = ($properties | Where-Object KeyName -eq 'DEVPKEY_Device_DriverInfPath').Data
    }
} | ConvertTo-Json -Depth 5
