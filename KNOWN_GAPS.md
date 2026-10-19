# Completion boundary

## Windows build and kernel lifecycle

The WDK project, driver source, and Windows client are uncompiled in this environment. No `.sys`, `.cat`, signed package, or verified installable INF is supplied. Hardware binding is unresolved. WDK compile, static analysis, Driver Verifier, and a kernel-debuggable test setup must precede claims of driver stability.

The transport currently uses serialized synchronous BRB requests with timeouts. Upper-level cancellation is not forwarded immediately; it waits for the bounded lower request. Close failures retain the channel handle for a subsequent cleanup attempt. Release/removal while a close fails, callback lifetime/rundown, cancellation races, and power-transition behavior need Windows validation and likely a fully asynchronous request design. These are outstanding engineering issues, not tested guarantees. No automatic reconnect is implemented.

## Protocol completeness

Peer Suspend/Abort/Close/Discover/GetCapabilities (and related) are handled by `Session::handle_peer` and `Session::pump`. Signaling is polled during media emit on the Windows client. A full async BRB transport, RF reconnect soak, and Bluetooth qualification are still outstanding — see `PHASE2.md` and `docs/async-brb.md`.

The selected configuration remains 48 kHz stereo LDAC with the basic media transport and codec categories. Content protection, delay reporting, reporting/recovery services, codec renegotiation, and other sample rates are not implemented. No claim of Bluetooth qualification or full A2DP/AVDTP conformance is made. Live headset confirmation of LDAC framing remains a Phase 1 hardware gate.

The offline `ldac_encode` diagnostic now exercises libldac at 44.1/48/88.2/96 kHz with signed 16/24/32-bit stereo PCM. These formats are not yet exposed by the live client or AVDTP configuration, and offline success does not establish headset compatibility.

## Audio experience

File, WAV, and WASAPI loopback paths exist. Adaptive EQMID and a bounded capture queue are implemented. There is still no dedicated Windows playback device (`docs/wasapi-endpoint.md`). Loopback uses the startup default endpoint and may also play locally. No endpoint hot-swap, automatic muting, mixer UI, microphone routing, AVRCP controls, multipoint coordination, or volume synchronization is implemented.

## Next implementation sequence

Tracked in `PRODUCTION_PLAN.md` and `PHASE0.md`–`PHASE5.md`.

1. **Hardware:** Complete Phase 0 bind/sign smoke, then Phase 1 audible PCM + Verifier on the G16.
2. **Kernel:** Implement async BRBs per `docs/async-brb.md`; Verifier cancel/remove/power.
3. **Audio:** Dedicated WASAPI/virtual endpoint; mute-local routing; listening soak.
4. **Release:** Production signing, measured SUPPORT_MATRIX, tagged RELEASE train.
