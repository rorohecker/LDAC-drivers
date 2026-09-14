#pragma once
/* Include ntddk.h/wdf.h in kernel mode, windows.h/winioctl.h in user mode first. */
#if !defined(_KERNEL_MODE)
#include <winioctl.h>
#endif
#define XM5_ABI 1u
#define XM5_MAX_PACKET 4096u
#define XM5_IOCTL_INFO CTL_CODE(FILE_DEVICE_UNKNOWN,0x800,METHOD_BUFFERED,FILE_READ_DATA)
#define XM5_IOCTL_OPEN CTL_CODE(FILE_DEVICE_UNKNOWN,0x801,METHOD_BUFFERED,FILE_READ_DATA|FILE_WRITE_DATA)
#define XM5_IOCTL_SEND CTL_CODE(FILE_DEVICE_UNKNOWN,0x802,METHOD_BUFFERED,FILE_READ_DATA|FILE_WRITE_DATA)
#define XM5_IOCTL_RECV CTL_CODE(FILE_DEVICE_UNKNOWN,0x803,METHOD_BUFFERED,FILE_READ_DATA|FILE_WRITE_DATA)
#define XM5_IOCTL_CLOSE CTL_CODE(FILE_DEVICE_UNKNOWN,0x804,METHOD_BUFFERED,FILE_READ_DATA|FILE_WRITE_DATA)
/* Fixed-width Windows types; no pointers or native handles cross the ABI. */
#pragma pack(push,8)
typedef struct { ULONG version; ULONG reserved; ULONGLONG address; } XM5_INFO;
typedef struct { ULONG version; ULONG channel; ULONG timeout_ms; ULONG size; UCHAR data[XM5_MAX_PACKET]; } XM5_PACKET;
typedef struct { ULONG version; ULONG channel; ULONG out_mtu; ULONG in_mtu; } XM5_CHANNEL;
#pragma pack(pop)
/* Project-specific interface, unrelated to the Bluetooth radio's interface. */
DEFINE_GUID(GUID_DEVINTERFACE_XM5_LDAC,0xa54b7971,0x8fe1,0x4dda,0xa8,0x91,0x62,0xbd,0xf4,0x33,0x68,0x27);
