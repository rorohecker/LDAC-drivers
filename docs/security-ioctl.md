# IOCTL security review (Phase 4.3)

## Threat model

Local attackers with code execution as a standard user should not be able to open the device or issue Bluetooth BRBs through this driver.

## Controls present

| Control | Detail |
|---|---|
| SDDL | `D:P(A;;GA;;;SY)(A;;GA;;;BA)` — SYSTEM and Built-in Administrators only |
| Exclusive | `WdfDeviceInitSetExclusive(TRUE)` |
| Secure open | `FILE_DEVICE_SECURE_OPEN` |
| ABI version | All IOCTLs require `XM5_ABI` |
| Bounds | Channel ∈ {0,1}, packet ≤ 4096, timeout 1–3000 ms |
| METHOD_BUFFERED | System copies buffers; no direct user pointer dereference in Transfer path |

## Residual risks

- Administrators are trusted; malware with admin can open L2CAP channels to the bound device.
- Sync BRB cancel gaps (see `async-brb.md`) can extend DoS windows for admin callers.
- Consumer distribution may need a less privileged, more tightly scoped interface than GA for BA.

## Recommendation

Retain admin-only access for experimental builds. Revisit capability-based access before any non-admin consumer SKU.
