# Phase 2 — Protocol / transport status

## Implemented in software (Linux-tested)

| Item | Status |
|---|---|
| P2-1 Peer command responder | Done — Discover/GetCaps/GetConfig/Start/Suspend/Close/Abort |
| P2-2 Signaling pump | Done — `Session::pump` |
| P2-3 Collision policy | Done — peer cmds handled during `transact`; late mismatched responses still error |
| P2-6 Reconnect supervisor | Done — `ReconnectSupervisor` + `--reconnect` |
| P2-7 Extra malformed/peer SEID tests | Done in `tests/test_core.cpp` |
| P2-8 Scope statement | Done — XM5-only 48 kHz stereo LDAC (see README) |

## Remaining (hardware / kernel)

| Item | Status |
|---|---|
| P2-4 Async BRB model | Design in `docs/async-brb.md`; sync Submit retained |
| P2-5 Cancel/remove/power Verifier | Blocked on Windows |
| P2-6 RF drop end-to-end | Blocked on hardware |
