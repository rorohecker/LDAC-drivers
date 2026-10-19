# Verification performed in this environment

## 2026-10-18 review of the Windows LDAC path changes

- The portable core compiled with Zig C++ using `-Wall -Wextra -Wpedantic -Werror`
  and passed 3,748 checks, including LDAC capability flags.
- The offline encoder compiled with Zig C using the same warning flags and passed
  HQ/SQ/MQ, four sample rates by three packed PCM depths, input and output
  protection, and help/invalid-option checks.
- All 12 PowerShell scripts parsed without syntax errors. The signed-installer
  INF patterns accepted the local template and rejected an added model section;
  boot-option patterns rejected enabled or unrecognized integrity settings.
  A read-only query found one
  present WH-1000XM5 Audio Sink service with a qualified VID/PID hardware ID
  currently bound to `BthA2dp`. No driver was installed or rebound.
- PowerShell execution policy is `Restricted`, so script runtime tests could not
  run. CMake and MSBuild are not currently available on this environment; the
  Windows client and KMDF source changes were not rebuilt in this review.

The result does not establish audible XM5 playback, driver installation, or
Microsoft package signing.

Date: 2026-09-10. Platform: Linux, GCC/G++. Windows build and hardware tests: not performed.

## Portable protocol and real encoder

Compiled the portable C++ core with C++17, `-Wall -Wextra -Wpedantic -Werror`, AddressSanitizer, and UndefinedBehaviorSanitizer. Both upstream encoder compilation units were also instrumented.

Result:

```text
PASS: 3516 checks; negotiation, fragmentation, rejection cleanup, RTP, real encoder MTU/quality matrix, malformed corpus.
```

The quality/MTU matrix uses HQ, SQ, and MQ at negotiated-MTU test values of 679, 1024, and 4096. Checks cover outgoing packet length, RTP fields, sequence/timestamp wraparound, irregular sample-aligned input chunks, and encoder drain. Negotiation uses a fake peer, not the headphones. A deterministic 10,000-input malformed corpus exercises capability, endpoint, and signaling parsers.

An upstream signed-left-shift issue in `pack_ldac.c` was reported by UBSan, corrected with an unsigned cast, and the instrumented tests rerun successfully with `UBSAN_OPTIONS=halt_on_error=1`. LeakSanitizer cannot run under this environment's tracing setup; `ASAN_OPTIONS=detect_leaks=0` was used. These are not leak-test results.

## Offline encoder

```text
PASS: HQ/SQ/MQ encoding; records; partial/empty input; malformed input; output protection.
One-second diagnostic sizes (HQ, SQ, MQ): [124832, 83224, 41612]
```

The fixed encoder was compared with the original encoder on full-range deterministic random PCM at all three quality settings:

```text
PASS: unsigned-shift fix produces byte-identical HQ/SQ/MQ output for full-range random PCM versus original encoder.
```

## Not established

No Windows compilation, real Bluetooth signaling, headphone decoding/listening, WDK static analysis, Driver Verifier, signing, installation, power/removal testing, or sustained streaming has been performed. Portable test success cannot establish these properties.
