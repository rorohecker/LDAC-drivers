# Phase 0 status — Buildable and bindable

See `PRODUCTION_PLAN.md` for the full multi-phase plan. This file is the working checklist for **Phase 0 only**.

## Goal

On the ASUS ROG Zephyrus G16 (Windows 11), produce a **test-signed** package that binds as the Bluetooth Audio Sink **profile** driver for the Sony WH-1000XM5, exposes `GUID_DEVINTERFACE_XM5_LDAC`, and answers `XM5_IOCTL_INFO` for the headset address. **No AVDTP or audio success is required to exit Phase 0.**

## Repo deliverables (completed in cloud)

| Item | Status | Artifact |
|---|---|---|
| Phase plans | Done | `PRODUCTION_PLAN.md` |
| Portable Linux CI | Done | `.github/workflows/portable-ci.yml` |
| Hardened user/kernel build script | Done | `tools/build.ps1` |
| Binding resolver | Done | `tools/resolve-binding.ps1` |
| INF generator | Done | `tools/generate-inf.ps1` |
| INF template + policy comments | Done | `driver/xm5ldac.inf.in` |
| Test package assembly | Done | `tools/package-test.ps1` |
| Test-sign helper | Done | `tools/sign-test.ps1` |
| Install / uninstall helpers | Done | `tools/install-test.ps1`, `tools/uninstall-test.ps1` |
| Smoke probe (user-mode) | Done | `tools/smoke-info.ps1` |
| Portable core still green | Done | Linux CMake + ASan/UBSan (see walkthrough) |

## Hardware-blocked items (run on the G16)

Copy this checklist and tick locally:

- [ ] **P0-1** Install Visual Studio C++ tools, Windows SDK, WDK + VS integration
- [ ] **P0-2** `.\tools\build.ps1` → user-mode + CTest pass
- [ ] **P0-3** `.\tools\build.ps1 -IncludeDriver` → `driver\out\x64\Debug\xm5ldac.sys` exists
- [ ] **P0-4** Pair XM5, then `.\tools\hardware-report.ps1 | Out-File hw.json -Encoding utf8`
- [ ] **P0-5** `.\tools\resolve-binding.ps1 -ReportPath hw.json` → pick Audio Sink HWID (never Intel radio)
- [ ] **P0-6** `.\tools\generate-inf.ps1 -HardwareId '<id from resolve>'`
- [ ] **P0-7** `.\tools\package-test.ps1 -Configuration Debug`
- [ ] **P0-8** Elevated: `.\tools\sign-test.ps1 -PackageDir package\test`
- [ ] **P0-9** Elevated: `.\tools\install-test.ps1 -PackageDir package\test` (enables testsigning; reboot if prompted)
- [ ] **P0-10** Confirm device interface present; `.\tools\smoke-info.ps1 -Address AA:BB:CC:DD:EE:FF`
- [ ] **P0-11** Elevated: `.\tools\uninstall-test.ps1` restores prior binding; reboot if needed

## Binding policy (non-negotiable)

1. Bind only to the **headphones’ Bluetooth Audio Sink service** instance.
2. **Never** replace the Intel Wireless Bluetooth radio driver.
3. Expect conflict with stock Windows A2DP on the same service — uninstall this package before relying on inbox audio again.
4. Keep testsigning limited to a development machine.

## Exit criteria

Phase 0 is **ready for Phase 1** when all hardware-blocked boxes above are checked and:

1. Driver loads without bugcheck.
2. `smoke-info.ps1` prints ABI version + matching Bluetooth address.
3. Uninstall works.

Paste the build log, resolve-binding output (redact if desired), and smoke-info output into the PR or a follow-up message to unlock Phase 1.
