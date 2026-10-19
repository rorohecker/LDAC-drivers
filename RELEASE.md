# Release train

1. **Dev** — feature branches; portable CI must stay green.
2. **Canary** — test-signed package on the G16 lab machine only; complete Phase 0–1 checklists.
3. **Beta** — limited installs; Phase 2–3 soak; still test-signed or attestation preview.
4. **Production** — production-signed package; SUPPORT_MATRIX row marked Supported; CHANGELOG section released; tag `vMAJOR.MINOR.PATCH`.

Development prereleases may contain source and user-mode diagnostic tools when
their notes explicitly state that they do not install or enable LDAC. An
experimental client binary alone does not make a working driver package.

Rollback: `tools/uninstall-test.ps1` (or production uninstaller) then reboot; restore inbox A2DP binding via Device Manager scan.

## Installable built-in-radio release gate (checked 2026-10-19)

The local Release client, portable tests, and KMDF binary compile. The current
WH-1000XM5 Audio Sink remains bound to Windows `BthA2dp`; no installation or
audible LDAC playback was verified. The local package is test-signed, while a
normal Windows install requires a Microsoft-signed catalog. Do not publish the
test-signed files as an installable release.

Before publishing an installer that claims to work on the built-in Intel radio:

1. Resolve the conflict with the inbox A2DP profile and provide a usable Windows
   playback endpoint. The current client accepts files and loopback capture only.
2. Complete asynchronous BRB cancellation/removal/power work, IOCTL review, and
   Driver Verifier tests on the G16 with a kernel-debuggable setup.
3. Obtain a Microsoft-signed package for the exact XM5 Audio Sink hardware ID.
   Keep Secure Boot and normal code integrity enabled.
4. Install on the target hardware; verify the exact service binding and
   `smoke-info.ps1`, then capture LDAC negotiation and confirm audible playback,
   reconnection, a listening soak, and clean uninstall/restore.
5. Update `SUPPORT_MATRIX.md`, `TEST_RESULTS.md`, and `CHANGELOG.md` with measured
   results before publishing a non-preview release.

The optional USB trial uses `tools/setup-usb.ps1` and upstream A2DP Windows
Bridge. It requires a separate adapter and does not install this KMDF driver.
