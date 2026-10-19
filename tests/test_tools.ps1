$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$count = 0
function Assert($value, [string]$message) {
    if (-not $value) { throw $message }
    $script:count++
}
# Parse every production PowerShell helper without executing privileged actions.
foreach ($file in Get-ChildItem (Join-Path $root 'tools') -Filter '*.ps1') {
    $tokens = $null; $errors = $null
    $null = [System.Management.Automation.Language.Parser]::ParseFile($file.FullName, [ref]$tokens, [ref]$errors)
    Assert ($errors.Count -eq 0) "PowerShell parse error in $($file.Name): $errors"
}
$temp = Join-Path $root ('build\script-tests-' + [guid]::NewGuid().ToString('N'))
$signedPackage = ''
New-Item -ItemType Directory -Path $temp -Force | Out-Null
try {
    $labBlocked = $false
    try { & (Join-Path $root 'tools\install-test.ps1') -PackageDir $temp | Out-Null }
    catch { $labBlocked = $_.Exception.Message -like '*explicitly opted-in lab*' }
    Assert $labBlocked 'Test installer did not reject a call without lab opt-in'
    $inf = Join-Path $temp 'test.inf'
    $valid = 'BTHENUM\{0000110b-0000-1000-8000-00805f9b34fb}_VID&0002054c_PID&0df0'
    & (Join-Path $root 'tools\generate-inf.ps1') -HardwareId $valid -OutputPath $inf | Out-Null
    Assert ((Get-Content -Raw $inf).Contains($valid)) 'Audio Sink ID not preserved'
    foreach ($bad in @('USB\VID_8087&PID_0033', 'BTH\MS_BTHPORT\0', 'Intel Wireless Bluetooth',
        'BTHENUM\{0000110e-0000-1000-8000-00805f9b34fb}', ($valid + "`n[malicious]"))) {
        $rejected = $false
        try { & (Join-Path $root 'tools\generate-inf.ps1') -HardwareId $bad -OutputPath $inf | Out-Null }
        catch { $rejected = $true }
        Assert $rejected "Unsafe binding accepted: $bad"
    }
    # Exercise the actual uninstall selection logic with mock DISM and pnputil.
    # Only this in-memory test copy changes the administrator guard.
    $source = Get-Content -Raw (Join-Path $root 'tools\uninstall-test.ps1')
    $source = $source.Replace('if (-not (Test-IsAdmin))', 'if ($false)')
    function Get-WindowsDriver {
        param([switch]$Online, $ErrorAction)
        @(
            [pscustomobject]@{OriginalFileName='C:\Store\xm5ldac.inf'; ProviderName='XM5 LDAC development project'; Driver='oem42.inf'},
            [pscustomobject]@{OriginalFileName='C:\Store\xm5ldac.inf'; ProviderName='XM5 LDAC development project'; Driver='oem42.inf'},
            [pscustomobject]@{OriginalFileName='C:\Store\xm5ldac.inf'; ProviderName='Another provider'; Driver='oem43.inf'},
            [pscustomobject]@{OriginalFileName='C:\Store\radio.inf'; ProviderName='Intel'; Driver='oem44.inf'}
        )
    }
    $script:deletions = @()
    function pnputil.exe { $script:deletions += ($args -join ' '); $global:LASTEXITCODE = 0 }
    & ([scriptblock]::Create($source)) | Out-Null
    Assert ($script:deletions.Count -eq 1) 'Uninstall did not select exactly one XM5 package'
    Assert ($script:deletions[0] -eq '/delete-driver oem42.inf /uninstall') 'Unexpected uninstall target or flags'

    # The signed installer must stop before PnPUtil if its INF could bind more
    # than the one confirmed, present Audio Sink service.
    $signedPackage = Join-Path $temp 'signed'
    New-Item -ItemType Directory -Path $signedPackage | Out-Null
    Copy-Item -LiteralPath $inf -Destination (Join-Path $signedPackage 'xm5ldac.inf')
    New-Item -ItemType File -Path (Join-Path $signedPackage 'xm5ldac.sys') | Out-Null
    New-Item -ItemType File -Path (Join-Path $signedPackage 'xm5ldac.cat') | Out-Null
    $instance = "$valid\7&11111111&0&TEST"
    $otherInstance = "$valid\7&22222222&0&TEST"
    $global:mockDeviceInstances = @($instance, $otherInstance)
    $global:mockService = 'BthA2dp'
    function Get-PnpDevice {
        param([switch]$PresentOnly, [string]$InstanceId, $ErrorAction)
        $devices = @($global:mockDeviceInstances | ForEach-Object {
            [pscustomobject]@{InstanceId=$_; FriendlyName='WH-1000XM5'}
        })
        if ($InstanceId) { return $devices | Where-Object InstanceId -eq $InstanceId }
        return $devices
    }
    function Get-PnpDeviceProperty {
        param([string]$InstanceId, [string]$KeyName, $ErrorAction)
        $properties = @(
            [pscustomobject]@{KeyName='DEVPKEY_Device_HardwareIds'; Data=@($valid)},
            [pscustomobject]@{KeyName='DEVPKEY_Device_Service'; Data=$global:mockService}
        )
        if ($KeyName) { return $properties | Where-Object KeyName -eq $KeyName }
        return $properties
    }
    $rejected = $false
    try { & (Join-Path $root 'tools\install-signed.ps1') -PackageDir $signedPackage -ConfirmedInstanceId $instance -Install | Out-Null }
    catch { $rejected = $_.Exception.Message -like '*another present device*' }
    Assert $rejected 'Signed installer did not reject a second matching Audio Sink'
    Assert ($script:deletions.Count -eq 1) 'Signed installer reached PnPUtil with two matching devices'
    $global:mockDeviceInstances = @($instance)
    $global:mockService = 'UnexpectedService'
    $rejected = $false
    try { & (Join-Path $root 'tools\install-signed.ps1') -PackageDir $signedPackage -ConfirmedInstanceId $instance -Install | Out-Null }
    catch { $rejected = $_.Exception.Message -like '*Unexpected current service*' }
    Assert $rejected 'Signed installer did not reject an unexpected existing service'
} finally {
    # All owned test files live in this unique directory; no recursive deletion.
    if ($signedPackage -and (Test-Path -LiteralPath $signedPackage -PathType Container)) {
        foreach ($name in @('xm5ldac.inf', 'xm5ldac.sys', 'xm5ldac.cat')) {
            $ownedFile = Join-Path $signedPackage $name
            if (Test-Path -LiteralPath $ownedFile -PathType Leaf) { Remove-Item -LiteralPath $ownedFile }
        }
        Remove-Item -LiteralPath $signedPackage
    }
    if (Test-Path -LiteralPath $inf) { Remove-Item -LiteralPath $inf }
    Remove-Item -LiteralPath $temp
}
Write-Output "PASS: $count PowerShell checks; no driver or machine settings changed."
