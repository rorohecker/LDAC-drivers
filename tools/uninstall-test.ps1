param(
    [ValidateSet('xm5ldac.inf')]
    [string]$InfName = 'xm5ldac.inf'
)
$ErrorActionPreference = 'Stop'

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}
if (-not (Test-IsAdmin)) { throw 'uninstall-test.ps1 must run elevated (Administrator).' }

# DISM objects avoid localized pnputil text and the automatic $Matches variable.
$packages = @(Get-WindowsDriver -Online -ErrorAction Stop | Where-Object {
    [IO.Path]::GetFileName($_.OriginalFileName) -ieq $InfName -and
    $_.ProviderName -eq 'XM5 LDAC development project' -and $_.Driver -match '^oem[0-9]+\.inf$'
} | Select-Object -ExpandProperty Driver -Unique)
if ($packages.Count -eq 0) {
    Write-Warning "No published driver package matched $InfName. Nothing to delete from the driver store."
} else {
    foreach ($oem in $packages) {
        Write-Output "Deleting $oem ..."
        pnputil.exe /delete-driver $oem /uninstall
        if ($LASTEXITCODE) { throw "pnputil /delete-driver $oem failed with $LASTEXITCODE" }
    }
}

Write-Output @'
If the XM5 Audio Sink still shows this driver, open Device Manager, locate the sink service
device, choose Uninstall device, then scan for hardware changes so inbox A2DP can rebind.
A reboot may be required. This script does not change boot security settings.
'@
