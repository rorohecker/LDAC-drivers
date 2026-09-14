# Dedicated WASAPI endpoint (Phase 3.1) — stub

## Goal

Expose a Windows audio render device so applications play to “XM5 LDAC” without loopback capture.

## Proposed shape

1. User-mode virtual audio driver / APO / or Virtual Audio Cable–style endpoint feeding PCM into the existing encode+AVDTP path.
2. Shared ring buffer with the same `AudioQueue` limit (≤200 ms).
3. Endpoint format locked to 48 kHz stereo s16le initially (matches current encoder).

## Non-goals for this stub

- Implementing a signed virtual audio driver in this repository yet
- Multipoint or capture (microphone) routing

## Current workaround

`xm5_stream --loopback` captures the default render device. Audio may also play locally.
