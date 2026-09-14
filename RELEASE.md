# Release train

1. **Dev** — feature branches; portable CI must stay green.
2. **Canary** — test-signed package on the G16 lab machine only; complete Phase 0–1 checklists.
3. **Beta** — limited installs; Phase 2–3 soak; still test-signed or attestation preview.
4. **Production** — production-signed package; SUPPORT_MATRIX row marked Supported; CHANGELOG section released; tag `vMAJOR.MINOR.PATCH`.

Rollback: `tools/uninstall-test.ps1` (or production uninstaller) then reboot; restore inbox A2DP binding via Device Manager scan.
