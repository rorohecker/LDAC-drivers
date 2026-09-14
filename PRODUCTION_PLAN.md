# Production plan — XM5 LDAC driver

This document is the authoritative phase plan. Complete each phase’s exit criteria before starting the next. `PHASE0.md` tracks the current phase in detail.

## Product goals (choose explicitly)

| Goal | Meaning | Exit when |
|---|---|---|
| Internal alpha | One G16 + XM5 can load a test-signed driver and play short PCM | Phase 0–1 done |
| Usable beta | Dedicated endpoint path, reconnect, soak listening | Through Phase 3 |
| Consumer production | Signed package, compliance, CI, support matrix | Through Phase 5 |

Default working goal until changed: **internal alpha**, then reassess.

**Status snapshot (software):** Phase 0 tooling done; Phases 2–3 portable protocol/audio policy done and Linux-tested; Phases 4–5 docs/CI process done. **Phase 0–1 hardware exit and Phase 2 async-BRB Verifier remain blocked on the G16.**

---

## Phase 0 — Buildable and bindable

**Purpose:** Produce a loadable, test-signed, correctly bound driver package on the target PC without claiming audio works.

### Work items

| ID | Item | Owner | Notes |
|---|---|---|---|
| P0-1 | VS + SDK + WDK install on G16 | Hardware | Required for compile |
| P0-2 | User-mode CMake build + CTest | Hardware | `tools/build.ps1` |
| P0-3 | Kernel WDK build (`-IncludeDriver`) | Hardware | Fix toolset diagnostics |
| P0-4 | Hardware report with XM5 paired | Hardware | `tools/hardware-report.ps1` |
| P0-5 | Resolve Audio Sink service HWID | Hardware | `tools/resolve-binding.ps1` |
| P0-6 | Generate INF from template | Either | `tools/generate-inf.ps1` |
| P0-7 | Assemble test package | Either | `tools/package-test.ps1` |
| P0-8 | Create/import test certificate + sign | Hardware | `tools/sign-test.ps1` |
| P0-9 | Enable testsigning boot + install | Hardware | `tools/install-test.ps1` |
| P0-10 | Smoke: load, interface present, unload | Hardware | No BSOD; address readable |
| P0-11 | Document rollback / uninstall | Repo | `tools/uninstall-test.ps1` |
| P0-12 | Portable core CI (Linux sanitizers) | Repo | GitHub Actions |

### Exit criteria

- [ ] `xm5_stream.exe` and `xm5ldac.sys` build on the G16
- [ ] INF uses the real headphone Audio Sink HWID (not Intel radio)
- [ ] Test-signed package installs and uninstalls cleanly
- [ ] Device interface `GUID_DEVINTERFACE_XM5_LDAC` enumerates when bound
- [ ] Client can open the driver for the XM5 Bluetooth address (IOCTL_INFO)
- [ ] No claim of AVDTP/audio success yet

### Repo prep (this phase’s software deliverable)

Scripts, INF tooling, CI, and checklists so the G16 run is mechanical. Items P0-1…P0-5 and P0-8…P0-10 remain **blocked on Windows hardware**.

---

## Phase 1 — Prove real LDAC to the headset

**Purpose:** Audible short PCM over live Bluetooth with packet evidence.

### Work items

| ID | Item |
|---|---|
| P1-1 | Driver Verifier + kernel debugger on a throwaway / known-good image |
| P1-2 | Stress cancel/remove/power/close before long audio |
| P1-3 | Open L2CAP signaling + media channels against live stack |
| P1-4 | Live AVDTP Discover → Start; capture HCI or BT snoop |
| P1-5 | Confirm LDAC codec config and media framing vs fake-peer assumptions |
| P1-6 | Play short raw 48 kHz s16le PCM via `xm5_stream --file` |
| P1-7 | Clean teardown: Ctrl+C, peer disconnect, radio off, unload |

### Exit criteria

- [ ] Headphones play the short PCM file
- [ ] Trace shows successful LDAC SetConfiguration / Start
- [ ] No BSOD under Verifier for the smoke scenarios
- [ ] Failures are diagnosable from logs/traces

---

## Phase 2 — Protocol and transport hardening

**Purpose:** Survive peer commands, disconnects, and cancel/remove races.

### Work items

| ID | Item |
|---|---|
| P2-1 | Peer-initiated AVDTP command responder |
| P2-2 | Continuous signaling pump while media streams |
| P2-3 | Late response / collision / retry policy |
| P2-4 | Replace sync BRBs with async request model |
| P2-5 | Prompt cancellation and safe rundown |
| P2-6 | Automatic reconnect state machine |
| P2-7 | Expanded malformed-peer and RF glitch tests |
| P2-8 | Explicit product scope: XM5-only vs broader A2DP |

### Exit criteria

- [ ] Peer Suspend/Abort/Close handled without stuck channels
- [ ] Cancel/remove/power paths Verifier-clean under load
- [ ] Reconnect recovers a new session without reboot
- [ ] Scope statement published in README

---

## Phase 3 — Windows audio experience

**Purpose:** Apps can play to the headset without loopback hacks.

### Work items

| ID | Item |
|---|---|
| P3-1 | Dedicated WASAPI / virtual audio endpoint |
| P3-2 | Bounded buffering with measured underrun policy |
| P3-3 | Default-device / mute-local routing behavior |
| P3-4 | Adaptive HQ/SQ/MQ under congestion |
| P3-5 | Optional: WAV input, tray status, AVRCP volume |
| P3-6 | Listening QA: soak, sleep/wake, RF, app switch |

### Exit criteria

- [ ] System / app audio routes to XM5 LDAC endpoint
- [ ] Sustained session meets latency/underrun targets (define numbers in Phase 3 kickoff)
- [ ] Adaptive quality or documented fixed-quality policy

---

## Phase 4 — Packaging, security, compliance

**Purpose:** Ship a signed, supportable package with legal clarity.

### Work items

| ID | Item |
|---|---|
| P4-1 | Versioned installer + uninstall + conflict policy with inbox A2DP |
| P4-2 | Production signing (EV / attestation / WHQL path) |
| P4-3 | Security review of IOCTL + privilege model |
| P4-4 | LICENSE/NOTICE distribution for libldac |
| P4-5 | Bluetooth qualification decision if marketed as A2DP |
| P4-6 | ETW/WPP logging + crash-dump support docs |

### Exit criteria

- [ ] Normal Windows users can install without testsigning
- [ ] Legal/compliance checklist signed off
- [ ] Support docs cover install, conflict, and dump collection

---

## Phase 5 — Production engineering system

**Purpose:** Repeatable releases and a declared support matrix.

### Work items

| ID | Item |
|---|---|
| P5-1 | CI: Windows user-mode + optional WDK; Linux sanitizers |
| P5-2 | Warnings-as-errors on driver Release; SDV where viable |
| P5-3 | SemVer, changelog, signed artifacts, rollback |
| P5-4 | Hardware compatibility list |
| P5-5 | Release train: canary → broad |

### Exit criteria

- [ ] Green CI required to merge
- [ ] Published support matrix and release process
- [ ] Goal “consumer production” (or chosen goal) met

---

## Dependency graph

```text
Phase 0 (build/bind/sign smoke)
    → Phase 1 (live LDAC PCM)
        → Phase 2 (protocol/transport)
            → Phase 3 (audio endpoint UX)
                → Phase 4 (packaging/compliance)
                    → Phase 5 (CI/release system)
```

Phase 5 CI for portable `core/` may start in Phase 0; Windows driver CI waits on Phase 0 hardware success.
