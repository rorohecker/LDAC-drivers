# Phase 1 — Live LDAC validation (hardware lab)

Software prep for Phase 1 is in-tree. **Exit criteria still require the G16 + XM5.**

## Lab procedure

1. Complete `PHASE0.md` (`smoke-info.ps1` PASS).
2. Enable Driver Verifier for `xm5ldac.sys` on a recoverable image; attach KD if available.
3. Capture Bluetooth HCI / BT snoop while running:
   ```text
   build\Debug\xm5_stream.exe --address AA:BB:CC:DD:EE:FF --file short.s16le sq
   ```
4. Confirm AVDTP Discover → GetCapabilities → SetConfiguration → Open → Start and LDAC media frames (RTP PT 96, Sony vendor codec).
5. Teardown matrix: Ctrl+C, unplug XM5, radio off, unload driver — no bugcheck.

## Expected signaling (source-initiated)

| Signal | Body notes |
|---|---|
| Discover | Peer sink list |
| GetCapabilities | Must include media transport + Sony LDAC 48 kHz stereo |
| SetConfiguration | ACP SEID + INT SEID 1 + `ldac_configuration()` |
| Open / Start | Remote sink SEID |

Golden capability bytes: `core` `ldac_configuration()` → `{1,0,7,10,0,0xff,0x2d,1,0,0,0xaa,0,0x10,1}`.

## Generate short PCM

```bash
python3 tools/gen_pcm.py --seconds 2 --out short.s16le
```

## Hardware-blocked exit

- [ ] Audible short PCM on XM5
- [ ] Packet trace matches LDAC framing assumptions
- [ ] Verifier-clean for smoke cancel/remove/power
