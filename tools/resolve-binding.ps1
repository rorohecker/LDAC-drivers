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

$audioSinkHints = @(
    '*Audio Sink*',
    '*A2DP*',
    '*0000110B*',
    '*0000110b*',
    '*Headset*',
    '*Headphones*',
    '*WH-1000XM5*',
    '*XM5*'
)
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
    $looksLikeSink = (Test-AnyMatch $blob $audioSinkHints) -or ($d.InstanceId -like 'BTHENUM\*')
    if ($isRadio) { continue }
    if (-not $looksLikeSink) { continue }
    $score = 0
    if ($d.Name -match 'WH-1000XM5|XM5') { $score += 5 }
    if ($hw -match '0000110[Bb]') { $score += 4 }
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
        RecommendedHardwareId = $(if (@($d.HardwareIds).Count -gt 0) { @($d.HardwareIds)[0] } else { $null })
        Warning = $(if ($d.Service -match 'BthA2dp') { 'Inbox A2DP currently owns this service; installing xm5ldac will replace that binding.' } else { $null })
    }
}

$ranked = $candidates | Sort-Object Score -Descending
if (-not $ranked -or $ranked.Count -eq 0) {
    Write-Error @'
No headphone Audio Sink candidates found.
Pair the XM5, confirm it appears in Settings → Bluetooth, then re-run tools/hardware-report.ps1.
Do not invent an Intel radio hardware ID.
'@
    exit 1
}

if ($Json) {
    $ranked | ConvertTo-Json -Depth 6
    exit 0
}

Write-Output '=== Candidate Audio Sink bindings (highest score first) ==='
Write-Output 'Pick RecommendedHardwareId from a headphone service row. Never use the Intel radio.'
Write-Output ''
$i = 0
foreach ($c in $ranked) {
    $i++
    Write-Output ("[{0}] score={1}  name={2}" -f $i, $c.Score, $c.Name)
    Write-Output ("    InstanceId: {0}" -f $c.InstanceId)
    Write-Output ("    Service:    {0}" -f $c.Service)
    Write-Output ("    DriverInf:  {0}" -f $c.DriverInf)
    Write-Output ("    RecommendedHardwareId: {0}" -f $c.RecommendedHardwareId)
    if ($c.Warning) { Write-Output ("    WARNING:    {0}" -f $c.Warning) }
    Write-Output ("    All HardwareIds:")
    foreach ($id in $c.HardwareIds) { Write-Output ("      - {0}" -f $id) }
    Write-Output ''
}

$best = $ranked | Select-Object -First 1
Write-Output 'Suggested next command:'
Write-Output ("  .\tools\generate-inf.ps1 -HardwareId '{0}'" -f $best.RecommendedHardwareId)
