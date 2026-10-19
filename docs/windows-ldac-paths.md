# Windows LDAC development paths — 2026-10-18

This review compares the [LDAC optimization guide](https://github.com/legendcodermartino1245/ldac-optimization-guide), [A2DP Windows Bridge](https://github.com/SeiyaFunaokaJP/A2DP-Windows-Bridge), the current XM5 project, and Microsoft's published driver requirements. It is an architecture review, not evidence that the local driver plays audio on an XM5. No driver, certificate, boot option, or Bluetooth adapter was changed during this review.

## Built-in G16 radio check (2026-10-19)

This laptop exposes Intel Wi-Fi 7 BE201 Bluetooth (`USB\VID_8087&PID_0037`),
currently using the Windows `BTHUSB` radio service. Its WH-1000XM5 Audio Sink
still uses `BthA2dp`. No separate USB Bluetooth dongle is present. The bridge's
current [adapter table](https://seiyafunaokajp.github.io/A2DP-Windows-Bridge/setup#recommended-adapters)
lists older Intel 8265/9260/AX200/AX201 as experimental and says AX210 and
newer are unsupported. Thus its WinUSB route has no supported built-in-radio
path for this BE201, and reassigning the only Bluetooth radio would take it
away from the Windows Bluetooth stack.

The repository's KMDF profile driver is the experimental built-in-radio path:
it targets the XM5 Audio Sink service rather than the Intel radio. It compiles,
but live load, Bluetooth negotiation, and audible output have not been shown.
The current client has file/WAV/loopback input but no selectable Windows render
endpoint. The profile conflict, cancellation and power safety, Microsoft signing,
and hardware tests in `RELEASE.md` still gate an installable release.

## What the two repositories contribute

| Source | Useful for this project | Boundary |
|---|---|---|
| [LDAC optimization guide](https://github.com/legendcodermartino1245/ldac-optimization-guide) | Its [configuration matrix](https://github.com/legendcodermartino1245/ldac-optimization-guide/blob/main/ldac_configuration_matrixes.md) suggests test cases for 44.1/48/88.2/96 kHz, 16/24/32-bit input, and fixed/adaptive rates. Reconnect and codec reset scenarios can inform a headset test matrix. | Primarily Android/Samsung settings and observations, not Windows driver source. Its claims about an Intel `Enable LDAC` registry key and persistent headset codec profiles are unverified here. Do not implement a driver around them or treat Android developer-option values as Windows AVDTP negotiation evidence. |
| [A2DP Windows Bridge](https://github.com/SeiyaFunaokaJP/A2DP-Windows-Bridge) | A complete alternative architecture: user-mode [BTstack and libldac over a dedicated WinUSB USB Bluetooth adapter](https://seiyafunaokajp.github.io/A2DP-Windows-Bridge/architecture). It has codec negotiation, WASAPI capture, reconnect, diagnostics, and reversible adapter assignment with an elevated helper. Its [virtual-device capture](https://seiyafunaokajp.github.io/A2DP-Windows-Bridge/#audio-capture-modes) gives apps a selectable audio destination when paired with an already signed virtual audio device. | The published [hardware table](https://seiyafunaokajp.github.io/A2DP-Windows-Bridge/#verified-hardware) verifies a WH-1000XM4 with a TP-Link UB500, not this project's WH-1000XM5 and Intel radio. It takes exclusive use of a *separate* USB Bluetooth adapter while WinUSB is assigned. Bundled [BTstack permits personal, non-commercial use](https://github.com/SeiyaFunaokaJP/A2DP-Windows-Bridge/blob/main/THIRD_PARTY_LICENSES.md); a commercial product needs an appropriate license or another stack. |

Microsoft's [Windows Classic audio documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/bluetooth-classic-audio) lists SBC, AAC, classic aptX, and aptX Adaptive on select compatible Qualcomm Windows 11 24H2 devices; it does **not** list LDAC. This also qualifies A2DP Windows Bridge's broad statement that built-in Windows lacks aptX Adaptive. Sony [lists LDAC for the WH-1000XM5](https://www.sony.com/electronics/support/wireless-headphones-bluetooth-headphones/wh-1000xm5/specifications), so a working source must advertise, negotiate, encode, and transmit it; a registry setting alone is not a supported implementation path.

## Recommended route to something usable

Use the dedicated-adapter, user-mode path first. It can keep Windows kernel code integrity and Secure Boot in their normal state because it uses the [Microsoft-provided WinUSB driver](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/automatic-installation-of-winusb) and obtains administrator approval only when assigning or restoring that adapter's driver. This is a separate Bluetooth stack for the dedicated adapter; the built-in Intel adapter remains with Windows. It does **not** make a normal Windows Bluetooth adapter run LDAC, and its default loopback mode also plays sound at the local output. A normally signed virtual audio device can provide separate routing.

The fastest evidence is a controlled XM5 trial with A2DP Windows Bridge and a supported dedicated adapter, beginning at 48 kHz LDAC SQ/660 kbps, then HQ/990 and adaptive mode. Record actual negotiated codec, sample rate, bitrate mode, packet loss/underruns, reconnect behavior, and whether the adapter restores cleanly. Verify with an over-the-air or HCI trace where possible; UI labels and Windows mix format alone do not prove the transmitted codec or rate. Keep the XM5 result as **unvalidated** until this trial succeeds.

For independent development, keep the local `core/` LDAC encoder and AVDTP tests. `core/avdtp.hpp` already defines a `Transport` interface; a second backend can implement its connection, send, receive, disconnect, and cancellation contract. A WinUSB backend would also need a Bluetooth host stack (HCI, pairing/key storage, L2CAP, SDP, AVDTP/A2DP, and likely AVRCP); replacing the present L2CAP IOCTLs with raw USB writes is insufficient. Study or contribute to [A2DP Windows Bridge's existing stack and pipeline](https://seiyafunaokajp.github.io/A2DP-Windows-Bridge/architecture) before duplicating them. Respect each copied component's license and keep Sony's [libldac Apache-2.0 LICENSE/NOTICE](https://android.googlesource.com/platform/external/libldac/+/refs/heads/main/Android.bp).

## Existing KMDF route and security gate

The current project is a 48 kHz, 16-bit, stereo file/loopback prototype. Its [known gaps](../KNOWN_GAPS.md) include no normal playback endpoint, unverified live XM5 playback, and unfinished kernel request/cancellation lifetime. Its INF binds the headphone Audio Sink service, displacing the Windows A2DP profile for that service. Microsoft [advises third-party Bluetooth software not to replace inbox profiles](https://learn.microsoft.com/en-us/windows-hardware/design/component-guidelines/bluetooth). The kernel route therefore needs an explicit compatibility and servicing decision before installation, in addition to protocol, power, and security validation. Microsoft's [profile-driver interfaces](https://learn.microsoft.com/en-us/windows-hardware/drivers/bluetooth/bluetooth-profile-drivers-overview) document L2CAP access; they do not make this implementation production-ready.

The legacy `tools/install-test.ps1` can enable `TESTSIGNING` through `bcdedit` and import a self-signed root certificate only with explicit `-AllowTestSigningLab` opt-in. **Do not run that helper for the stated no-security-bypass goal.** The research and build/test steps can proceed without it. If continuing the kernel product route on a normally configured Windows PC, obtain a Microsoft dashboard-signed package through the [Hardware Developer Program](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/). Microsoft describes [attestation as a testing route](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/driver-signing-offerings), with no retail Windows Update publication; use the HLK/Windows Hardware Compatibility Program route for a retail release. The `tools/install-signed.ps1` preflight checks exact hardware binding, catalog file membership, kernel signature policy, and the trusted Microsoft catalog publisher. Keep Secure Boot and code integrity enabled, validate the real service binding, and run Driver Verifier/power/removal testing before live playback claims. A custom [SysVAD-style virtual audio endpoint](https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/sysvad-virtual-audio-device-driver-sample/) would itself be a driver package that needs normal signing and validation.

## Next development gates

The signed-package preflight needs an elevated shell to verify boot settings and Secure Boot. Elevation alone does not install the package; `-Install` must also be supplied.

1. **Hardware evidence:** Establish whether a dedicated supported USB adapter is available. Test A2DP Windows Bridge with the XM5 without touching the built-in Intel radio; capture codec negotiation and an audible 30-minute session plus disconnect/reconnect and sleep/wake results.
2. **Audio experience:** Choose system loopback, process capture, or an existing normally signed virtual audio device. Prefer the last for a selectable “XM5 LDAC” output and to avoid local speaker duplication.
3. **Independent implementation:** Decide whether to contribute XM5 support/tests to A2DP Windows Bridge or implement a separate user-mode backend. If implementing, prioritize transport/pairing/AVDTP interoperability, measured packet pacing, bitrate fallback, reconnect, and secure local key storage before UI or higher sample rates.
4. **Kernel product only if needed:** Resolve the inbox-profile conflict, complete async BRB lifetime and IOCTL review, provide an audio endpoint, obtain Microsoft signing, and pass HLK/Verifier and headset tests. Do not use the existing test-signing install sequence as the product path.

The [optimization guide's bitrate matrix](https://github.com/legendcodermartino1245/ldac-optimization-guide/blob/main/ldac_configuration_matrixes.md) is a useful *test matrix*. The offline diagnostic encoder now exercises 44.1/48/88.2/96 kHz with signed 16/24/32-bit PCM. The AVDTP core also decodes the LDAC rate and channel flags from a headset response, based on [AOSP's LDAC constants](https://android.googlesource.com/platform/system/bt/+/ea7ab70a711e642653dd5922b83aa04a53af9e0e/stack/include/a2dp_vendor_ldac_constants.h). The live Windows client remains 48 kHz/16-bit; additional live formats require selecting a common advertised mode, resampling/capture changes, pacing validation, and a verified 48 kHz baseline.
