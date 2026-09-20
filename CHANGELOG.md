# Changelog

All notable changes to this project are documented here. Version numbers follow SemVer for packages (`MAJOR.MINOR.PATCH`). Driver `DriverVer` in the INF tracks the same triple.

## Unreleased

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
- Document all four adaptations to the pinned Sony encoder sources.

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
