# Phase 4 — Packaging, security, compliance

## Done in-repo

- Test package / sign / install / uninstall scripts (Phase 0)
- libldac LICENSE/NOTICE copied by `package-test.ps1`
- IOCTL security review: `docs/security-ioctl.md`
- ETW/WPP macro stubs: `driver/xm5_trace.h`
- Explicit non-qualification statement in README

## Blocked on release engineering

- [ ] EV / attestation / WHQL signing (P4-2)
- [ ] Production installer UX beyond pnputil (P4-1 MSI optional)
- [ ] Live dump collection on G16 (P4-6 runtime)
- [ ] Bluetooth qualification if ever marketed as A2DP product (P4-5) — **decision: do not claim qualification**
