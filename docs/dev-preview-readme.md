# XM5 LDAC __VERSION__ Windows x64 development tools

This is a development preview. It does not install a Bluetooth driver or enable
LDAC on Windows. No signed KMDF package or USB bridge program is included.

## Files

- `ldac_encode.exe`: offline LDAC encoder for diagnostic PCM files.
- `xm5_stream.exe`: experimental streaming client. It requires an installed,
  compatible XM5 profile driver, which this archive does not provide. Live XM5
  playback has not been verified by this project.
- `gen_pcm.py`: creates short raw PCM input for local diagnostics (Python needed).
- `setup-usb.ps1` and `bridge-readiness.ps1`: optional guided setup for a
  physically separate USB Bluetooth adapter. The setup helper can download
  and hash-check the official A2DP Windows Bridge installer. The bridge app
  handles WinUSB assignment and pairing. This path has not been tested with
  the XM5 here and cannot use the G16's built-in Intel BE201 radio.
- `LICENSE.libldac` and `NOTICE.libldac`: Sony encoder license and notice.

The executables require the Microsoft Visual C++ x64 runtime. Run
`ldac_encode.exe --help` for PCM format and encoding options. Run
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\setup-usb.ps1` to
inventory adapters; this process-only policy option changes no system setting. No
driver changes occur without the later explicit `-InstallBridge` option and
an exact, physically confirmed external adapter instance ID.

For source, release status, and known gaps, see
https://github.com/rorohecker/LDAC-drivers/releases/tag/__VERSION__
