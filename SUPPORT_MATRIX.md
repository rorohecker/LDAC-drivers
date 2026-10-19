# Support matrix

Only configurations listed as **Supported** are in scope. Everything else is unsupported.

| Host | OS | Radio | Headset | Status | Notes |
|---|---|---|---|---|---|
| ASUS ROG Zephyrus G16 (2025) | Windows 11 25H2 26200.x | Intel Wireless Bluetooth | Sony WH-1000XM5 | **Target / unvalidated** | Present XM5 Audio Sink service and current `BthA2dp` binding identified; driver installation and playback pending |
| Burner laptop | Linux with BlueZ, PipeWire, WirePlumber and LDAC codec | Built-in Bluetooth radio | Sony WH-1000XM5 | **Trial / unvalidated** | `linux/xm5_ldac.py` selects an available LDAC profile; hardware, codec negotiation and audible playback pending |
| Windows 11 PC | Windows 11 | Dedicated USB adapter with Microsoft WinUSB + A2DP Windows Bridge | Sony WH-1000XM5 | **Trial / unvalidated** | Follow `docs/bridge-trial.md`; bridge publishes XM4/UB500 evidence, not an XM5 result |
| Other PCs | — | — | — | Unsupported | |
| Other headsets | — | — | — | Unsupported | XM5-only experimental scope |

Update rows only after measured lab results (build, bind, audible stream, soak).
