param(
    [string]$AdapterInstanceId = '',
    [switch]$ConfirmExternalAdapter,
    [switch]$InstallBridge
)
$ErrorActionPreference = 'Stop'

# A2DP Windows Bridge v2.0.0-alpha.1, x64 installer. The hash is the SHA-256
# digest published with this exact asset in its GitHub release metadata.
$bridgeVersion = '2.0.0-alpha.1'
$bridgeUrl = "https://github.com/SeiyaFunaokaJP/A2DP-Windows-Bridge/releases/download/v$bridgeVersion/A2DPWB-$bridgeVersion-win64-setup.exe"
$bridgeSha256 = '277856df3c42feb2bf92ebdeb19b3915a988dacac168dc7c9602043d8cbb69bf'
$setupGuide = 'https://seiyafunaokajp.github.io/A2DP-Windows-Bridge/setup'

if (-not [Environment]::Is64BitOperatingSystem -or -not [Environment]::Is64BitProcess) {
    throw 'This setup requires 64-bit Windows and a 64-bit PowerShell process.'
}
$readiness = & (Join-Path $PSScriptRoot 'bridge-readiness.ps1') -AdapterInstanceId $AdapterInstanceId |
    ConvertFrom-Json
$candidates = @($readiness.UsbDeviceInventory | Where-Object {
    $_.BluetoothLike -and $_.Name -notmatch '(?i)Intel.*Bluetooth' -and
    $_.InstanceId -notmatch '(?i)^USB\\VID_8087'
})

if (-not $AdapterInstanceId) {
    Write-Output 'Present USB Bluetooth adapter candidates:'
    if ($candidates.Count -eq 0) {
        Write-Output '  None. Connect a physically separate USB Bluetooth adapter, then rerun this script.'
    } else {
        $candidates | Format-Table Name, InstanceId, Service, Status -AutoSize | Out-String | Write-Output
    }
    Write-Output 'Keep the laptop built-in Bluetooth radio assigned to Windows.'
    Write-Output "Official bridge setup: $setupGuide"
    if ($InstallBridge) { throw 'Select one external adapter with -AdapterInstanceId before installing the bridge.' }
    return
}

$adapter = $readiness.SelectedAdapter
if (-not $adapter) { throw 'The selected external adapter was not found.' }
Write-Output "Selected adapter: $($adapter.Name)"
Write-Output "Instance ID: $($adapter.InstanceId)"
Write-Output "Current service: $($adapter.Service)"
if (-not $ConfirmExternalAdapter) {
    Write-Output 'Physically identify the separate USB dongle, then rerun with -ConfirmExternalAdapter.'
    if ($InstallBridge) { throw 'External adapter confirmation is required before installing the bridge.' }
    return
}
if (-not $InstallBridge) {
    Write-Output "Ready to install A2DP Windows Bridge v$bridgeVersion. Rerun with -InstallBridge to download and launch its official installer."
    Write-Output "Official setup guide: $setupGuide"
    return
}

$installer = Join-Path ([IO.Path]::GetTempPath()) ("xm5-a2dpwb-$bridgeVersion-" + [guid]::NewGuid().ToString('N') + '.exe')
try {
    Write-Output "Downloading the official A2DP Windows Bridge v$bridgeVersion installer..."
    Invoke-WebRequest -Uri $bridgeUrl -OutFile $installer -UseBasicParsing
    $actualHash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -ne $bridgeSha256) {
        throw 'Downloaded bridge installer SHA-256 does not match the pinned official release asset.'
    }
    Write-Output 'Installer SHA-256 verified. Launching the interactive bridge installer.'
    $process = Start-Process -FilePath $installer -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Bridge installer exited with code $($process.ExitCode)." }
} finally {
    if (Test-Path -LiteralPath $installer -PathType Leaf) { Remove-Item -LiteralPath $installer -Force }
}

Write-Output @'
Open A2DP Windows Bridge and use its first-run setup:
  1. Select the same physically separate USB adapter shown above.
  2. In the bridge app, assign the Microsoft WinUSB driver to that adapter only.
  3. For a Realtek adapter, follow its firmware instructions in the official guide.
  4. Pair the XM5, select LDAC SQ at 48 kHz, and play a short test audio file.
  5. Verify audible playback and reconnection. Use the app's Restore driver action
     when returning the external adapter to normal Windows Bluetooth use.
This script does not assign WinUSB, install xm5ldac.sys, or change boot security.
'@
Write-Output "Official setup guide: $setupGuide"
