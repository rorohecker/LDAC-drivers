param(
    [Parameter(Mandatory = $true)]
    [string]$ReportPath,
    [switch]$Json
)
$ErrorActionPreference = 'Stop'

if (!(Test-Path $ReportPath)) { throw "Report not found: $ReportPath" }
$raw = Get-Content -Raw -Path $ReportPath
$devices = $raw | ConvertFrom-Json
if ($null -eq $devices) { throw 'Report JSON is empty.' }
if ($devices -isnot [System.Array]) { $devices = @($devices) }

# Only the Bluetooth Audio Sink service is eligible. Names and other Bluetooth
# profiles are insufficient evidence for a safe binding. Permit standard HWID
# qualifiers (for example _VID&0002054C_PID&0E47 or _LOCALMFG&0000), not an
# instance path, an embedded UUID, or arbitrary trailing text.
$audioSinkHardwareIdPattern = '\ABTHENUM\\\{0000110b-0000-1000-8000-00805f9b34fb\}(?:_[A-Za-z0-9]+&[A-Fa-f0-9]+)*\z'
$radioBlocklist = @(
    '*Intel*Bluetooth*',
    '*Wireless Bluetooth*',
    '*USB\VID_8087*',
    '*BTH\MS_BTHBRB*',
    '*BTH\MS_BTHPORT*'
)

function Test-AnyMatch([string]$text, [string[]]$patterns) {
    if ([string]::IsNullOrWhiteSpace($text)) { return $false }
    foreach ($p in $patterns) { if ($text -like $p) { return $true } }
    return $false
}

$candidates = foreach ($d in $devices) {
    $hw = @($d.HardwareIds) -join ';'
    $blob = (@($d.Name, $d.InstanceId, $d.Service, $hw) -join '|')
    $isRadio = Test-AnyMatch $blob $radioBlocklist
    if ($isRadio) { continue }
    if ($d.InstanceId -notlike 'BTHENUM\*') { continue }
    $sinkHardwareIds = @($d.HardwareIds | Where-Object {
        $_ -is [string] -and $_ -match $audioSinkHardwareIdPattern
    })
    if ($sinkHardwareIds.Count -eq 0) { continue }
    $score = 0
    if ($d.Name -match 'WH-1000XM5|XM5') { $score += 5 }
    $score += 4
    if ($d.Name -match 'Audio Sink|A2DP|Headphone') { $score += 3 }
    if ($d.InstanceId -like 'BTHENUM\*') { $score += 2 }
    if ($d.Service -and $d.Service -notmatch 'BthA2dp|Microsoft') { $score += 1 }
    [pscustomobject]@{
        Score = $score
        Name = $d.Name
        InstanceId = $d.InstanceId
        Status = $d.Status
        Service = $d.Service
        DriverInf = $d.DriverInf
        HardwareIds = @($d.HardwareIds)
        RecommendedHardwareId = $sinkHardwareIds[0]
        AudioSinkHardwareIds = $sinkHardwareIds
        IdentityVerified = $false
        ConfirmationRequired = $true
        IdentityNote = 'The Audio Sink UUID identifies a service, not the XM5. Confirm the device identity and exact HWID before generating an INF.'
        Warning = $(if ($d.Service -match 'BthA2dp') { 'Inbox A2DP currently owns this service; installing xm5ldac will replace that binding.' } else { $null })
    }
}

$ranked = @($candidates | Sort-Object Score -Descending)
if (-not $ranked -or $ranked.Count -eq 0) {
    Write-Error @'
No Bluetooth Audio Sink service hardware IDs found.
Pair the XM5, confirm it appears in Settings > Bluetooth, then re-run tools/hardware-report.ps1.
Names alone do not qualify a device. Never use an Intel radio hardware ID.
'@
    exit 1
}

if ($Json) {
    $ranked | ConvertTo-Json -Depth 6
    exit 0
}

Write-Output '=== Candidate Audio Sink bindings (highest score first) ==='
Write-Output 'These rows have Audio Sink service HWIDs; their XM5 identity still requires confirmation.'
Write-Output 'Wait for the user to confirm the device identity and exact RecommendedHardwareId. Never use the Intel radio.'
Write-Output ''
$i = 0
foreach ($c in $ranked) {
    $i++
    Write-Output ("[{0}] score={1}  name={2}" -f $i, $c.Score, $c.Name)
    Write-Output ("    InstanceId: {0}" -f $c.InstanceId)
    Write-Output ("    Service:    {0}" -f $c.Service)
    Write-Output ("    DriverInf:  {0}" -f $c.DriverInf)
    Write-Output ("    RecommendedHardwareId: {0}" -f $c.RecommendedHardwareId)
    Write-Output ("    Identity:   {0}" -f $c.IdentityNote)
    if ($c.Warning) { Write-Output ("    WARNING:    {0}" -f $c.Warning) }
    Write-Output ("    All HardwareIds:")
    foreach ($id in $c.HardwareIds) { Write-Output ("      - {0}" -f $id) }
    Write-Output ''
}

Write-Output 'Only after the user confirms the exact XM5 Audio Sink HWID, run:'
Write-Output "  .\tools\generate-inf.ps1 -HardwareId '<CONFIRMED_HWID>'"
