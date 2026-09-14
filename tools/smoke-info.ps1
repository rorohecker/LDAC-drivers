param(
    [Parameter(Mandatory = $true)]
    [string]$Address,
    [string]$StreamExe = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent

# Phase 0 smoke: confirm the bound driver answers XM5_IOCTL_INFO for this address.
# Prefers a tiny probe via xm5_stream if present; otherwise uses a .NET P/Invoke opener.

function ConvertTo-BthUlongLong([string]$addr) {
    $parts = $addr.Trim() -split '[:\-]'
    if ($parts.Count -ne 6) { throw "Address must look like AA:BB:CC:DD:EE:FF (got $addr)" }
    [uint64]$value = 0
    foreach ($p in $parts) {
        $value = ($value -shl 8) -bor [Convert]::ToByte($p, 16)
    }
    return $value
}

$addrValue = ConvertTo-BthUlongLong $Address

if ([string]::IsNullOrWhiteSpace($StreamExe)) {
    $candidates = @(
        (Join-Path $projectRoot 'build\Debug\xm5_stream.exe'),
        (Join-Path $projectRoot 'build\Release\xm5_stream.exe'),
        (Join-Path $projectRoot 'package\test\xm5_stream.exe')
    )
    $StreamExe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class Xm5Smoke {
  public const uint DIGCF_PRESENT = 2;
  public const uint DIGCF_DEVICEINTERFACE = 0x10;
  public static readonly Guid Iface = new Guid("a54b7971-8fe1-4dda-a891-62bdf4336827");
  [StructLayout(LayoutKind.Sequential)] public struct SP_DEVICE_INTERFACE_DATA {
    public int cbSize; public Guid InterfaceClassGuid; public int Flags; public IntPtr Reserved;
  }
  [DllImport("setupapi.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern IntPtr SetupDiGetClassDevs(ref Guid classGuid, string enumerator, IntPtr hwnd, uint flags);
  [DllImport("setupapi.dll", SetLastError=true)]
  public static extern bool SetupDiDestroyDeviceInfoList(IntPtr deviceInfoSet);
  [DllImport("setupapi.dll", SetLastError=true)]
  public static extern bool SetupDiEnumDeviceInterfaces(IntPtr deviceInfoSet, IntPtr deviceInfoData, ref Guid interfaceClassGuid, uint memberIndex, ref SP_DEVICE_INTERFACE_DATA deviceInterfaceData);
  [DllImport("setupapi.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern bool SetupDiGetDeviceInterfaceDetail(IntPtr deviceInfoSet, ref SP_DEVICE_INTERFACE_DATA deviceInterfaceData, IntPtr deviceInterfaceDetailData, uint deviceInterfaceDetailDataSize, out uint requiredSize, IntPtr deviceInfoData);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern SafeFileHandle CreateFile(string lpFileName, uint dwDesiredAccess, uint dwShareMode, IntPtr lpSecurityAttributes, uint dwCreationDisposition, uint dwFlagsAndAttributes, IntPtr hTemplateFile);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool DeviceIoControl(SafeFileHandle hDevice, uint dwIoControlCode, IntPtr lpInBuffer, uint nInBufferSize, byte[] lpOutBuffer, uint nOutBufferSize, out uint lpBytesReturned, IntPtr lpOverlapped);
  [StructLayout(LayoutKind.Sequential, Pack=8)] public struct XM5_INFO {
    public uint version; public uint reserved; public ulong address;
  }
  // CTL_CODE(FILE_DEVICE_UNKNOWN=0x22, 0x800, METHOD_BUFFERED=0, FILE_READ_DATA=1)
  public const uint XM5_IOCTL_INFO = 0x00226000;
}
"@

# CTL_CODE = (DeviceType<<16) | (Access<<14) | (Function<<2) | Method
$expectedIoctl = [uint32]((0x22 -shl 16) -bor (1 -shl 14) -bor (0x800 -shl 2) -bor 0)
if ([Xm5Smoke]::XM5_IOCTL_INFO -ne $expectedIoctl) {
    throw "Internal IOCTL constant mismatch: $([Xm5Smoke]::XM5_IOCTL_INFO) vs $expectedIoctl"
}

$guid = [Xm5Smoke]::Iface
$devInfo = [Xm5Smoke]::SetupDiGetClassDevs([ref]$guid, $null, [IntPtr]::Zero, [Xm5Smoke]::DIGCF_PRESENT -bor [Xm5Smoke]::DIGCF_DEVICEINTERFACE)
if ($devInfo -eq [IntPtr]::Zero -or $devInfo -eq [IntPtr]::new(-1)) {
    throw 'SetupDiGetClassDevs failed. Is the driver installed and the sink device present?'
}

try {
    $found = 0
    $matched = $false
    for ($i = 0u; $i -lt 64u; $i++) {
        $iface = New-Object Xm5Smoke+SP_DEVICE_INTERFACE_DATA
        $iface.cbSize = [Runtime.InteropServices.Marshal]::SizeOf($iface)
        if (-not [Xm5Smoke]::SetupDiEnumDeviceInterfaces($devInfo, [IntPtr]::Zero, [ref]$guid, $i, [ref]$iface)) { break }
        $found++
        $required = 0u
        [Xm5Smoke]::SetupDiGetDeviceInterfaceDetail($devInfo, [ref]$iface, [IntPtr]::Zero, 0, [ref]$required, [IntPtr]::Zero) | Out-Null
        if ($required -eq 0) { continue }
        $buffer = [Runtime.InteropServices.Marshal]::AllocHGlobal([int]$required)
        try {
            # SP_DEVICE_INTERFACE_DETAIL_DATA.cbSize is 8 on x64.
            [Runtime.InteropServices.Marshal]::WriteInt32($buffer, 8)
            if (-not [Xm5Smoke]::SetupDiGetDeviceInterfaceDetail($devInfo, [ref]$iface, $buffer, $required, [ref]$required, [IntPtr]::Zero)) {
                continue
            }
            $path = [Runtime.InteropServices.Marshal]::PtrToStringUni([IntPtr]::Add($buffer, 4))
            $handle = [Xm5Smoke]::CreateFile($path, 0xC0000000, 0, [IntPtr]::Zero, 3, 0, [IntPtr]::Zero)
            if ($handle.IsInvalid) {
                Write-Output "Interface present but CreateFile failed (need Administrators): $path"
                continue
            }
            try {
                $out = New-Object byte[] 16
                $returned = 0u
                $ok = [Xm5Smoke]::DeviceIoControl($handle, [Xm5Smoke]::XM5_IOCTL_INFO, [IntPtr]::Zero, 0, $out, [uint32]$out.Length, [ref]$returned, [IntPtr]::Zero)
                if (-not $ok -or $returned -lt 16) {
                    Write-Output "IOCTL_INFO failed on $path (Win32=$( [Runtime.InteropServices.Marshal]::GetLastWin32Error() ))"
                    continue
                }
                $version = [BitConverter]::ToUInt32($out, 0)
                $address = [BitConverter]::ToUInt64($out, 8)
                $hex = ('{0:X12}' -f $address) -replace '..(?!$)', '$0:'
                Write-Output "OK interface=$path"
                Write-Output "OK abi_version=$version address=$hex"
                if ($address -eq $addrValue) {
                    Write-Output "PASS: address matches $Address"
                    $matched = $true
                } else {
                    Write-Output "INFO: address did not match requested $Address"
                }
            } finally {
                $handle.Dispose()
            }
        } finally {
            [Runtime.InteropServices.Marshal]::FreeHGlobal($buffer)
        }
    }
    if ($found -eq 0) { throw 'No GUID_DEVINTERFACE_XM5_LDAC interfaces present. Driver not bound or device not started.' }
    if (-not $matched) { throw "No bound interface reported Bluetooth address $Address" }
    Write-Output 'Phase 0 smoke probe succeeded.'
} finally {
    [Xm5Smoke]::SetupDiDestroyDeviceInfoList($devInfo) | Out-Null
}

if ($StreamExe) {
    Write-Output "Client binary available for Phase 1: $StreamExe"
}
