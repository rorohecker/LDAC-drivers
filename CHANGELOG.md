# Changelog

All notable changes to this project are documented here. Version numbers follow SemVer for packages (`MAJOR.MINOR.PATCH`). Driver `DriverVer` in the INF tracks the same triple.

## Unreleased

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
