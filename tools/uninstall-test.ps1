param(
    [string]$InfName = 'xm5ldac.inf'
)
$ErrorActionPreference = 'Stop'

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}
if (-not (Test-IsAdmin)) { throw 'uninstall-test.ps1 must run elevated (Administrator).' }

Write-Output "Searching pnputil for published packages matching $InfName ..."
$published = pnputil /enum-drivers | Out-String
$blocks = $published -split '(?=Published Name:)'
$matches = @()
foreach ($block in $blocks) {
    if ($block -match [regex]::Escape($InfName) -or $block -match 'xm5ldac') {
        if ($block -match 'Published Name:\s+(\S+)') {
            $matches += $Matches[1]
        }
    }
}
$matches = $matches | Select-Object -Unique
if (-not $matches -or $matches.Count -eq 0) {
    Write-Warning "No published driver package matched $InfName. Nothing to delete from the driver store."
} else {
    foreach ($oem in $matches) {
        Write-Output "Deleting $oem ..."
        pnputil /delete-driver $oem /uninstall /force
        if ($LASTEXITCODE) { Write-Warning "pnputil /delete-driver $oem failed with $LASTEXITCODE" }
    }
}

Write-Output @'
If the XM5 Audio Sink still shows this driver, open Device Manager, locate the sink service
device, choose Uninstall device, then scan for hardware changes so inbox A2DP can rebind.
A reboot may be required. This script does not disable testsigning (bcdedit /set testsigning off).
'@
