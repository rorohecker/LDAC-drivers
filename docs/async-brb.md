# Async BRB design (Phase 2.4)

## Problem

`Submit()` today uses `WdfIoTargetSendInternalIoctlOthersSynchronously` with a timeout. Upper-level cancellation waits for the lower BRB. Close failures retain the channel handle. Release/removal while a BRB is in flight is unvalidated under Driver Verifier.

## Target model

1. Allocate a WDFREQUEST per BRB; send asynchronously to the Bluetooth PDO IoTarget.
2. Track outstanding requests in the device context (list + spinlock at DISPATCH where required).
3. `EvtRequestCancel` marks the BRB cancelled and completes the user IOCTL with `STATUS_CANCELLED` when the lower request finishes.
4. `EvtDeviceReleaseHardware` / file cleanup flushes the list (cancel + wait) before dereferencing the profile interface.
5. Never reuse an L2CAP handle after a failed close until a successful close or device removal rundown clears it.

## Interim policy

Keep sync Submit for internal alpha. Do not claim cancel/remove/power safety until this design is implemented and Verifier-clean on Windows.
