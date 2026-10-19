# Changelog

All notable changes to this project are documented here. Version numbers follow SemVer for packages (`MAJOR.MINOR.PATCH`). Driver `DriverVer` in the INF tracks the same triple.

## Unreleased

## 0.1.0-dev.3 - 2026-10-19

### Free Linux trial

- Add an XM5 LDAC profile selector for a connected Linux Bluetooth headset using
  BlueZ, PipeWire and WirePlumber. It checks the exact headset address, available
  profile, output sink and exposed codec, and restores the previous profile if
  activation fails. Live playback on a Linux laptop remains unverified.

## 0.1.0-dev.2 - 2026-10-19

### Setup and release checks

- Add a guided dedicated-USB setup helper that requires a physically confirmed
  external adapter and verifies the pinned upstream installer hash before launch.
- Make the Microsoft-signed package installer reject multiple matching present
  devices and report a staged-but-unbound package as a failed installation.
- Recheck the built-in Intel BE201 route and record the remaining live driver,
  endpoint, signing, and hardware playback gates before an installable release.

### Development downloads

- Package the verified Windows x64 user-mode tools and optional USB setup helper
  as a prerelease. The archive contains no kernel driver or driver installer.

## 0.1.0-dev.1 - 2026-10-18

### Windows LDAC path development

- Add a read-only dedicated USB adapter and bridge readiness inventory plus an XM5
  LDAC playback, reconnect, and adapter-restoration trial plan.
- Add a normal-security signed-driver preflight that checks the confirmed Audio Sink
  binding, current boot security, catalog membership, kernel policy, and Microsoft
  catalog publisher before any explicitly requested installation.
- Require explicit lab opt-in for the legacy test-signing installer; remove the
  PowerShell execution-policy override from CTest.
- Extend the offline diagnostic encoder and tests to 44.1/48/88.2/96 kHz and
  signed 16/24/32-bit stereo PCM. Live streaming remains 48 kHz/16-bit.
- Decode LDAC sample-rate and channel-mode capability flags for accurate future
  headset negotiation; continue selecting only the proven 48 kHz stereo mode.
- Audit the new paths: require an exact device instance and one INF manufacturer/model
  binding, reject additional code-integrity bypass boot options, improve encoder
  help and sample-rate parsing, and check depth/rate behavior in the offline suite.

### Audit fixes and usability

- Correct receive-timeout typing, local SEID validation, GetConfiguration payloads,
  command-specific rejects, and unknown-command General Reject responses.
- Synchronize capture queue access; use one duration-weighted congestion sample;
  discard queued capture during peer Suspend and check producer failures while paused.
- Reconnect the complete transport/capture session with bounded, cancellable backoff.
  Files restart from the beginning. Add `--help` and packet/pacing/send statistics.
- Stream PCM/WAV with bounded reads and validate RIFF/chunk bounds and format fields.
- Use a high-resolution waitable timer where available, MMCSS scheduling, and avoid
  large catch-up bursts. End-to-end latency still requires hardware measurement.
- Fix KMDF device naming before SDDL, serialize lifecycle changes, account for
  callback references, propagate close failures, and block stale channel reuse.
- Restrict INF support to Windows 11 x64, discover installed SDK/WDK versions,
  harden Audio Sink binding and package paths, and fix localized uninstall selection.
- Add Windows user-mode, protocol/WAV/PowerShell regressions, timer diagnostics,
  and an optional user-mode AddressSanitizer build (`-DXM5_ASAN=ON`).
- Document all five adaptations to the pinned Sony encoder sources, including
  the packed 24-bit PCM signed-shift fix found by Linux sanitizer CI.

Hardware smoke/audio, asynchronous BRB cancellation and Verifier, a virtual
playback endpoint, ETW/WPP, and Microsoft signing/certification remain incomplete.

### Added

- Phase plans `PRODUCTION_PLAN.md`, `PHASE0.md`–`PHASE5.md`
- Peer-initiated AVDTP responder + `Session::pump`
- `ReconnectSupervisor`, `--reconnect`
- Adaptive EQMID controller, `--adaptive`, `Encoder::set_quality`
- Bounded `AudioQueue`, WAV reader (`--wav`)
- Phase 0 bind/sign/install tooling and portable CI
- Trace macro stubs, async-BRB / security / endpoint docs

### Changed

- Local source SEID explicitly set to 1 in SetConfiguration
- `stop()` closes from `suspended` as well as `open`/`streaming`
