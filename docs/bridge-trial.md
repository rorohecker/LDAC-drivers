# XM5 LDAC trial with a dedicated USB adapter

This is the first hardware gate for the normal-security Windows path. [A2DP Windows Bridge](https://github.com/SeiyaFunaokaJP/A2DP-Windows-Bridge) has reported LDAC playback with a Sony WH-1000XM4 and a TP-Link UB500. The WH-1000XM5 is supported by Sony for LDAC, but has not been validated by this project or by the bridge's published hardware table.

## Prepare

1. Keep the built-in Intel Bluetooth radio assigned to Windows. Physically identify a separate USB Bluetooth adapter before selecting it in the bridge.
2. Run `./tools/setup-usb.ps1` to inventory USB adapter candidates. The helper lists no candidate on the current G16 because only its built-in Intel radio is present. After physically identifying a separate dongle, run `./tools/setup-usb.ps1 -AdapterInstanceId '<external USB instance ID>' -ConfirmExternalAdapter -InstallBridge`. This downloads and SHA-256 checks the pinned official A2DP Windows Bridge v2.0.0-alpha.1 installer, then launches it interactively. The script never assigns a device driver itself. The upstream version is a prerelease and its XM5 behavior still needs testing.
3. In the bridge app, follow its [official setup instructions](https://seiyafunaokajp.github.io/A2DP-Windows-Bridge/setup). Its elevated helper assigns Microsoft's signed WinUSB driver to the **external** adapter; the app offers a restore action. Do not assign WinUSB to the built-in radio.
4. Pair the XM5 with the bridge on that adapter. Use a 48 kHz Windows playback format for the initial test. Begin with LDAC SQ (660 kbps) and system loopback capture.

## Record the result

| Check | Evidence to collect |
|---|---|
| Negotiation | Headset advertises LDAC; AVDTP SetConfiguration and Start complete with 48 kHz stereo; record the selected mode. |
| Audible stream | A short known PCM source is audible, then at least 30 minutes of continuous listening with no unexplained disconnect. |
| Bitrate policy | Repeat at HQ (990 kbps) and adaptive mode; note stutters and quality changes. Do not assume 990 kbps remains stable. |
| Recovery | Disconnect/reconnect, headset power cycle, and Windows sleep/wake; note whether audio resumes and whether the correct codec returns. |
| Routing | System loopback may also play at the local output. For a selectable output, use an existing normally signed virtual audio device with the bridge's Virtual Device capture mode. |
| Restoration | Use the bridge's Restore driver action; verify the external adapter returns to Windows Bluetooth. |

The bridge's diagnostics include HCI capture and logs. Protect reports before sharing because they may contain Bluetooth addresses, device identifiers, and pairing information. A UI codec label or a Windows playback-format setting alone is not proof of over-the-air LDAC; use the trace when diagnosing negotiation or rate claims.

**Pass for the first milestone:** audible XM5 LDAC at 48 kHz with a captured negotiation, a stable 30-minute SQ session, and successful restore of the external adapter. This does not validate the local KMDF driver.

## Built-in Intel path

The separate [KMDF phase plan](../PRODUCTION_PLAN.md) remains experimental. The current driver still needs hardware binding, live AVDTP/audio tests, cancellation and power validation, a Windows render endpoint, and a Microsoft-signed package. `tools/install-signed.ps1` is a read-only package preflight by default; it verifies an exact confirmed XM5 Audio Sink instance, normal boot security, and kernel-policy signatures before an explicit `-Install`. There is no eligible Microsoft-signed package in this repository today. The legacy `install-test.ps1` requires explicit lab opt-in and is outside this normal-security path.

Run the signed-package preflight from an elevated shell because Windows restricts boot-security checks. The default invocation still makes no driver or boot changes.
