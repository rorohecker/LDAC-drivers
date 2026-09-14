# Phase 3 — Audio experience status

## Implemented

| Item | Status |
|---|---|
| P3-2 Bounded queue | `core/audio_queue.hpp` (default 38400 bytes) |
| P3-4 Adaptive EQMID | `core/adaptive.hpp` + `Encoder::set_quality` + `--adaptive` |
| P3-5 WAV input | `core/wav.hpp` + `--wav` (48 kHz stereo s16le PCM only) |
| Stream pump integration | Client calls `Session::pump` while emitting |

## Remaining (Windows runtime)

| Item | Status |
|---|---|
| P3-1 Dedicated render endpoint | Stub notes in `docs/wasapi-endpoint.md` |
| P3-3 Mute-local / default-device routing | Not implemented |
| P3-5 Tray / AVRCP | Not implemented |
| P3-6 Listening / RF soak QA | Hardware lab |
