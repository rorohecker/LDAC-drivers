# XM5 LDAC driver project — first implementation

This is experimental development source, not a working driver release. The Windows client and KMDF driver build on the G16 with MSVC and the WDK. Offline codec/protocol tests pass; driver loading and audible XM5 playback remain unverified. Installing the source archive does not enable LDAC. See [the audit and repair status](DRIVER_AUDIT.md).

Target: Sony WH-1000XM5, ASUS ROG Zephyrus G16 (2025), Intel Wireless Bluetooth, Windows 11 Home 25H2 build 26200.9168. The screenshots establish these names but do not identify the exact Bluetooth service hardware ID or establish compatibility with this driver.

## Production roadmap

Full multi-phase plan: [`PRODUCTION_PLAN.md`](PRODUCTION_PLAN.md).  
Phase checklists: [`PHASE0.md`](PHASE0.md) … [`PHASE5.md`](PHASE5.md).  
**Scope:** experimental **XM5-only**, **48 kHz stereo LDAC**. No Bluetooth qualification or general A2DP conformance is claimed. See [`SUPPORT_MATRIX.md`](SUPPORT_MATRIX.md).

**Hardware gate:** Phase 0–1 exit still requires the G16 lab. Software for Phases 2–5 (protocol responder, pump, reconnect, adaptive bitrate, packaging docs/CI) is implemented and Linux-tested.

Phase 0 on the G16 (elevated where noted):

```powershell
.\tools\build.ps1 -IncludeDriver
.\tools\hardware-report.ps1 | Out-File hw.json -Encoding utf8
.\tools\resolve-binding.ps1 -ReportPath hw.json
.\tools\generate-inf.ps1 -HardwareId '<RecommendedHardwareId>'
.\tools\package-test.ps1
# elevated:
.\tools\sign-test.ps1 -PackageDir package\test
.\tools\install-test.ps1 -PackageDir package\test
.\tools\smoke-info.ps1 -Address AA:BB:CC:DD:EE:FF
```

Phase 1 file smoke (after Phase 0):

```powershell
python tools\gen_pcm.py --seconds 2 --out short.s16le
build\Debug\xm5_stream.exe --address AA:BB:CC:DD:EE:FF --file short.s16le sq --adaptive --reconnect
```

## Implemented components

| Component | Implementation | Verification |
|---|---|---|
| LDAC encoder | Sony encoder, 48 kHz / 16-bit / stereo, HQ/SQ/MQ, partial blocks and drain | Compiled and integration-tested on Linux |
| AVDTP source | Discover→Start plus peer Suspend/Close/Abort/Discover/GetCaps responder and `pump` | Fake-peer negotiation, peer-command, and failure tests |
| Adaptive / reconnect | Duration-weighted EQMID congestion backoff; complete-session retry | Unit tests; Windows flags `--adaptive` `--reconnect`; live recovery unverified |
| WAV input | 48 kHz stereo PCM WAV demux | Unit test + `--wav` |
| KMDF transport | Two L2CAP channels, packet send/receive, IOCTL validation, exclusive administrator access, callback references and failed-close quarantine | WDK compile/static analysis pass; runtime and Verifier pending |
| Windows streaming client | Bounded PCM/WAV reads, timer pacing, signaling pump, synchronized WASAPI capture queue, Ctrl+C shutdown | Windows compile and user-mode regressions pass; live audio pending |
| Windows playback endpoint | Not implemented | Loopback/file paths only — see `docs/wasapi-endpoint.md` |

## Build on the G16

Use Visual Studio C++ build tools with CMake. Building the kernel component also requires the Windows SDK, WDK, and WDK Visual Studio integration. The build scripts install nothing and change no driver or security settings.

From a PowerShell terminal in this extracted project folder:

```powershell
.\tools\build.ps1
```

This builds the user-mode client and portable tests, then runs CTest. If Python is available, CTest also runs the offline encoder tests.

To also attempt the kernel driver compile with the WDK installed:

```powershell
.\tools\build.ps1 -IncludeDriver
```

The WDK project targets Windows 11 x64 (build 22000 or later) and KMDF 1.33. The build helper selects the newest installed matching SDK/WDK headers; use `-WindowsTargetPlatformVersion 10.0.26100.0` to select another installed kit. WDK Visual Studio integration is also required. No signed driver, catalog, test certificate, hardware report, or personal build log is published.

`xm5_stream --help` describes the client options. `--reconnect` retries transport creation, negotiation, and streaming failures with bounded backoff; it restarts files from the beginning and reopens the default capture endpoint. Capture samples queued during peer Suspend are discarded. Discontinuities reset the session when reconnect is enabled. This is recovery behavior, not seamless audio or clock-drift compensation.

For user-mode memory checks, configure a separate build with `cmake -S . -B build/asan -A x64 -DXM5_ASAN=ON`, build `RelWithDebInfo`, then run CTest in that directory. MSVC AddressSanitizer and its runtime must be installed. These tests do not load the kernel driver.

If PowerShell policy prevents the script from running, the equivalent user-mode commands in a Visual Studio developer terminal are:

```text
cmake -S . -B build -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
msbuild driver\xm5ldac.vcxproj /p:Configuration=Debug /p:Platform=x64
```

The last command requires the WDK. Send the first failing build output, or the successful build/test log, to continue Windows-specific development.

## Establish the device binding

Run this read-only report while the headphones are paired:

```powershell
.\tools\hardware-report.ps1
```

It prints the XM5 and Intel Bluetooth device instances, hardware IDs, current service, and driver version. Bluetooth addresses may appear in the device IDs; this report stays in the terminal unless you choose to share or save it.

The kernel code is intended to attach as the profile function driver on the headphones' Bluetooth Audio Sink service device. It must not replace the Intel radio driver. The `.inf.in` file is deliberately an unfinished template: the actual service hardware ID and its existing binding have not yet been checked. There is no installer in this package, and ordinary Windows A2DP and this driver must not compete for the same profile connection. Resolve binding and the Windows build before preparing a signed test package.

## Intended live tests after driver validation and binding

These commands describe the implemented client interface; they have not produced verified headphone playback yet.

```text
build\Debug\xm5_stream.exe --address AA:BB:CC:DD:EE:FF --file input.s16le sq
build\Debug\xm5_stream.exe --address AA:BB:CC:DD:EE:FF --loopback sq
```

Use the headphones' actual Bluetooth address. The client matches it against the address reported by the bound driver, so it does not silently choose a different Bluetooth device.

PCM input must be raw 48 kHz, stereo, signed 16-bit little-endian audio. WAV, MP3, and FLAC containers cannot be passed directly. Start with a short PCM file to isolate transport from audio capture. Loopback captures the default playback endpoint selected at startup; audio may also play locally, and the client does not automatically change or mute that device. Capture requests shared-mode conversion to 48 kHz stereo; support must be checked on this Windows installation.

## Remaining work

Read `KNOWN_GAPS.md` before treating this as a complete driver. The next concrete gate is a Windows build. After that: validate binding, L2CAP opening, real XM5 negotiation, and PCM-file playback. A dedicated output endpoint, complete peer-initiated signaling, reconnection, and production driver lifecycle/signing work remain.

## Source and references

- `core/`: portable protocol and encoder wrapper
- `windows/`: driver client, WASAPI capture, streaming executable
- `driver/`: experimental KMDF source, WDK project, INF template and shared ABI
- `tests/`: integration tests and malformed-input corpus
- `tools/`: build and read-only hardware report scripts, offline encoder
- `vendor/`: Sony LDAC source, original LICENSE/NOTICE, documented local portability fix

LDAC source revision: `eeee1a3f5f8df1282e3a6d297085885fd886737b`, https://android.googlesource.com/platform/external/libldac/

Windows interfaces reviewed:

- https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/bluetooth-profile-drivers-overview
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/bthddi/ns-bthddi-_brb_l2ca_open_channel
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/bthddi/ns-bthddi-_brb_l2ca_acl_transfer
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/bthddi/ns-bthddi-_bth_profile_driver_interface
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/bthddi/nc-bthddi-pfnbth_initialize_brb
- https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording
- https://github.com/microsoft/Windows-driver-samples/tree/main/bluetooth/bthecho

This is independent source; no Alternative A2DP Driver binary or license data is used.
