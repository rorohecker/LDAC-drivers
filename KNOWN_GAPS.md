# Completion boundary

## Windows build and kernel lifecycle

The WDK project, driver source, and Windows client are uncompiled in this environment. No `.sys`, `.cat`, signed package, or verified installable INF is supplied. Hardware binding is unresolved. WDK compile, static analysis, Driver Verifier, and a kernel-debuggable test setup must precede claims of driver stability.

The transport currently uses serialized synchronous BRB requests with timeouts. Upper-level cancellation is not forwarded immediately; it waits for the bounded lower request. Close failures retain the channel handle for a subsequent cleanup attempt. Release/removal while a close fails, callback lifetime/rundown, cancellation races, and power-transition behavior need Windows validation and likely a fully asynchronous request design. These are outstanding engineering issues, not tested guarantees. No automatic reconnect is implemented.

## Protocol completeness

The initial AVDTP state machine initiates the session from the PC. It validates incoming response fragments and transaction labels. It does not provide a complete local source endpoint responder: peer-initiated commands during setup terminate the attempt. Signaling is not continuously monitored while media streams. A complete responder and independent signaling pump are needed for peer Suspend/Abort/Close and recovery. Late responses, collision handling, retry policy, signaling interleaving, and malformed-peer behavior need broader conformance tests.

The selected configuration is only 48 kHz stereo LDAC with the basic media transport and codec categories. Content protection, delay reporting, reporting/recovery services, codec renegotiation, and other sample rates are not implemented. No claim of Bluetooth qualification or full A2DP/AVDTP conformance is made. The LDAC codec identifier and media framing still need confirmation by a real headset session or packet trace.

## Audio experience

There is no dedicated Windows playback device. File streaming and WASAPI loopback are the initial integration paths. Loopback uses the startup default endpoint and may also play locally. No endpoint hot-swap, automatic muting, mixer UI, microphone routing, AVRCP controls, multipoint coordination, or volume synchronization is implemented.

Quality is fixed to the requested HQ/SQ/MQ mode. There is no adaptive bitrate controller. A bounded 200 ms capture queue prevents unlimited latency accumulation; queue overflow, later capture discontinuity, and sends taking over 100 ms terminate the attempt. The pacer is based on `steady_clock` and has not been measured under Windows scheduling load. Listening quality, RF congestion, sustained latency, underruns, and long-session behavior remain untested.

## Next implementation sequence

1. Compile user mode and kernel mode on Windows; fix SDK/WDK diagnostics.
2. Resolve the exact XM5 service binding from the supplied read-only report.
3. Validate driver request lifecycle and cancellation with the WDK tools before loading a test build for audio trials.
4. Trace signaling and play a short PCM file, then add the peer-command responder and continuous signaling handling.
5. Validate WASAPI capture and implement a dedicated audio endpoint with bounded buffering.
6. Add reconnection, asynchronous transport, adaptive quality, packaging, signing, and sustained hardware tests.
