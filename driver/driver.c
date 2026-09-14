/* Experimental KMDF Bluetooth profile transport. Requires Windows WDK validation. */
#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>
#include <bthdef.h>
#include <bthguid.h>
#include <bthioctl.h>
#include <bthddi.h>
#include "public.h"

typedef struct { L2CAP_CHANNEL_HANDLE handle; USHORT out_mtu,in_mtu; volatile LONG disconnected; } CHANNEL;
typedef struct {
    WDFDEVICE device; BTH_PROFILE_DRIVER_INTERFACE profile; BTH_ADDR address;
    BOOLEAN ready; CHANNEL channels[2];
} DEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT,Context)
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD AddDevice;
EVT_WDF_DEVICE_PREPARE_HARDWARE Prepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE Release;
EVT_WDF_DEVICE_D0_EXIT PowerExit;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL Control;
EVT_WDF_FILE_CLEANUP FileCleanup;

static NTSTATUS Submit(DEVICE_CONTEXT *d,PBRB brb,ULONG size,ULONG timeout){
    WDF_MEMORY_DESCRIPTOR memory;
    WDF_REQUEST_SEND_OPTIONS options;
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memory,brb,size);
    WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,WDF_REL_TIMEOUT_IN_MS(timeout));
    return WdfIoTargetSendInternalIoctlOthersSynchronously(WdfDeviceGetIoTarget(d->device),
        NULL,IOCTL_INTERNAL_BTH_SUBMIT_BRB,&memory,NULL,NULL,&options,NULL);
}
static void Indication(PVOID context,INDICATION_CODE code,PINDICATION_PARAMETERS parameters){
    CHANNEL *channel=(CHANNEL*)context;
    UNREFERENCED_PARAMETER(parameters);
    if(code==IndicationRemoteDisconnect)InterlockedExchange(&channel->disconnected,1);
}
static void CloseChannel(DEVICE_CONTEXT *d,ULONG index){
    CHANNEL *channel=&d->channels[index];
    struct _BRB_L2CA_CLOSE_CHANNEL brb;
    NTSTATUS status;
    if(!channel->handle || !d->ready)return;
    RtlZeroMemory(&brb,sizeof(brb));
    d->profile.BthInitializeBrb((PBRB)&brb,BRB_L2CA_CLOSE_CHANNEL);
    brb.BtAddress=d->address; brb.ChannelHandle=channel->handle;
    status=Submit(d,(PBRB)&brb,sizeof(brb),3000);
    if(!NT_SUCCESS(status)){
        /* Retain the handle so a later cleanup retries; do not reuse this slot. */
        InterlockedExchange(&channel->disconnected,1);
        return;
    }
    channel->handle=NULL; channel->out_mtu=channel->in_mtu=0;
    InterlockedExchange(&channel->disconnected,1);
}
static NTSTATUS OpenChannel(DEVICE_CONTEXT *d,ULONG index){
    CHANNEL *channel=&d->channels[index];
    struct _BRB_L2CA_OPEN_CHANNEL *brb;
    NTSTATUS status;
    if(channel->handle)return STATUS_INVALID_DEVICE_STATE;
    if(index==1 && (!d->channels[0].handle || d->channels[0].disconnected))return STATUS_INVALID_DEVICE_STATE;
    brb=(struct _BRB_L2CA_OPEN_CHANNEL*)d->profile.BthAllocateBrb(BRB_L2CA_OPEN_CHANNEL,'5mxL');
    if(!brb)return STATUS_INSUFFICIENT_RESOURCES;
    brb->BtAddress=d->address; brb->Psm=0x19; brb->ChannelFlags=CF_ROLE_EITHER;
    brb->ConfigOut.Flags=CFG_MTU;
    brb->ConfigOut.Mtu.Min=L2CAP_MIN_MTU;
    brb->ConfigOut.Mtu.Max=XM5_MAX_PACKET;
    brb->ConfigOut.Mtu.Preferred=1024;
    brb->ConfigIn=brb->ConfigOut;
    brb->CallbackFlags=CALLBACK_DISCONNECT;
    brb->Callback=Indication; brb->CallbackContext=channel;
    brb->ReferenceObject=WdfDeviceWdmGetDeviceObject(d->device);
    brb->IncomingQueueDepth=10;
    InterlockedExchange(&channel->disconnected,0);
    status=Submit(d,(PBRB)brb,sizeof(*brb),5000);
    if(NT_SUCCESS(status)){
        channel->handle=brb->ChannelHandle;
        channel->out_mtu=brb->OutResults.Params.Mtu;
        channel->in_mtu=brb->InResults.Params.Mtu;
        if(channel->out_mtu>XM5_MAX_PACKET || channel->in_mtu>XM5_MAX_PACKET){
            CloseChannel(d,index);status=STATUS_INVALID_BUFFER_SIZE;
        }
    }
    d->profile.BthFreeBrb((PBRB)brb);return status;
}
static NTSTATUS Transfer(DEVICE_CONTEXT *d,XM5_PACKET *p,BOOLEAN receive){
    CHANNEL *channel=&d->channels[p->channel];
    struct _BRB_L2CA_ACL_TRANSFER *brb;
    NTSTATUS status;
    if(!channel->handle || InterlockedCompareExchange(&channel->disconnected,0,0))return STATUS_DEVICE_NOT_CONNECTED;
    if(!receive && (!p->size || p->size>channel->out_mtu))return STATUS_INVALID_BUFFER_SIZE;
    brb=(struct _BRB_L2CA_ACL_TRANSFER*)d->profile.BthAllocateBrb(BRB_L2CA_ACL_TRANSFER,'5mxL');
    if(!brb)return STATUS_INSUFFICIENT_RESOURCES;
    brb->BtAddress=d->address; brb->ChannelHandle=channel->handle;
    brb->Buffer=p->data; brb->BufferMDL=NULL;
    brb->BufferSize=receive?channel->in_mtu:p->size;
    brb->TransferFlags=receive?(ACL_TRANSFER_DIRECTION_IN|ACL_SHORT_TRANSFER_OK|ACL_TRANSFER_TIMEOUT):ACL_TRANSFER_DIRECTION_OUT;
    brb->Timeout=p->timeout_ms;
    status=Submit(d,(PBRB)brb,sizeof(*brb),p->timeout_ms+1000);
    if(NT_SUCCESS(status))p->size=brb->BufferSize;
    d->profile.BthFreeBrb((PBRB)brb);return status;
}
VOID Control(WDFQUEUE queue,WDFREQUEST request,size_t out_length,size_t in_length,ULONG code){
    DEVICE_CONTEXT *d=Context(WdfIoQueueGetDevice(queue));
    NTSTATUS status=STATUS_INVALID_DEVICE_REQUEST; ULONG_PTR returned=0;
    XM5_CHANNEL *input,*output; XM5_PACKET *packet; XM5_INFO *info;
    if(!d->ready){WdfRequestComplete(request,STATUS_DEVICE_NOT_READY);return;}
    if(code==XM5_IOCTL_INFO){
        status=WdfRequestRetrieveOutputBuffer(request,sizeof(*info),(PVOID*)&info,NULL);
        if(NT_SUCCESS(status)){RtlZeroMemory(info,sizeof(*info));info->version=XM5_ABI;info->address=d->address;returned=sizeof(*info);}
    }else if(code==XM5_IOCTL_OPEN || code==XM5_IOCTL_CLOSE){
        status=WdfRequestRetrieveInputBuffer(request,sizeof(*input),(PVOID*)&input,NULL);
        if(NT_SUCCESS(status)){
            ULONG channel=input->channel;
            if(input->version!=XM5_ABI || channel>1)status=STATUS_INVALID_PARAMETER;
            else if(code==XM5_IOCTL_CLOSE){
                if(channel==0)CloseChannel(d,1);
                CloseChannel(d,channel);status=STATUS_SUCCESS;
            }else{
                status=WdfRequestRetrieveOutputBuffer(request,sizeof(*output),(PVOID*)&output,NULL);
                if(NT_SUCCESS(status)){
                    status=OpenChannel(d,channel);
                    if(NT_SUCCESS(status)){
                        RtlZeroMemory(output,sizeof(*output));output->version=XM5_ABI;output->channel=channel;
                        output->out_mtu=d->channels[channel].out_mtu;output->in_mtu=d->channels[channel].in_mtu;
                        returned=sizeof(*output);
                    }
                }
            }
        }
    }else if(code==XM5_IOCTL_SEND || code==XM5_IOCTL_RECV){
        if(in_length<sizeof(*packet) || out_length<sizeof(*packet))status=STATUS_BUFFER_TOO_SMALL;
        else{
            status=WdfRequestRetrieveInputBuffer(request,sizeof(*packet),(PVOID*)&packet,NULL);
            if(NT_SUCCESS(status)){
                if(packet->version!=XM5_ABI || packet->channel>1 || packet->size>XM5_MAX_PACKET ||
                   packet->timeout_ms<1 || packet->timeout_ms>3000)status=STATUS_INVALID_PARAMETER;
                else{
                    // METHOD_BUFFERED uses the same system buffer for input and output.
                    if(code==XM5_IOCTL_RECV)RtlZeroMemory(packet->data,sizeof(packet->data));
                    status=Transfer(d,packet,code==XM5_IOCTL_RECV);
                    if(NT_SUCCESS(status))returned=sizeof(*packet);
                }
            }
        }
    }
    WdfRequestCompleteWithInformation(request,status,returned);
}
VOID FileCleanup(WDFFILEOBJECT file){
    DEVICE_CONTEXT *d=Context(WdfFileObjectGetDevice(file));CloseChannel(d,1);CloseChannel(d,0);
}
NTSTATUS Prepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated){
    DEVICE_CONTEXT *d=Context(device); NTSTATUS status; BTH_DEVICE_INFO info;
    WDF_MEMORY_DESCRIPTOR output;
    UNREFERENCED_PARAMETER(raw);UNREFERENCED_PARAMETER(translated);
    RtlZeroMemory(&d->profile,sizeof(d->profile));
    status=WdfFdoQueryForInterface(device,&GUID_BTHDDI_PROFILE_DRIVER_INTERFACE,(PINTERFACE)&d->profile,
        sizeof(d->profile),BTHDDI_PROFILE_DRIVER_INTERFACE_VERSION_FOR_QI,NULL);
    if(!NT_SUCCESS(status))return status;
    RtlZeroMemory(&info,sizeof(info));WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&output,&info,sizeof(info));
    status=WdfIoTargetSendInternalIoctlSynchronously(WdfDeviceGetIoTarget(device),NULL,
        IOCTL_INTERNAL_BTHENUM_GET_DEVINFO,NULL,&output,NULL,NULL);
    if(!NT_SUCCESS(status)){
        d->profile.Interface.InterfaceDereference(d->profile.Interface.Context);RtlZeroMemory(&d->profile,sizeof(d->profile));return status;
    }
    d->address=info.address;d->ready=TRUE;return STATUS_SUCCESS;
}
NTSTATUS Release(WDFDEVICE device,WDFCMRESLIST resources){
    DEVICE_CONTEXT *d=Context(device);UNREFERENCED_PARAMETER(resources);
    CloseChannel(d,1);CloseChannel(d,0);
    if(d->ready){d->ready=FALSE;d->profile.Interface.InterfaceDereference(d->profile.Interface.Context);}
    return STATUS_SUCCESS;
}
NTSTATUS PowerExit(WDFDEVICE device,WDF_POWER_DEVICE_STATE target){
    DEVICE_CONTEXT *d=Context(device);UNREFERENCED_PARAMETER(target);
    CloseChannel(d,1);CloseChannel(d,0);return STATUS_SUCCESS;
}
NTSTATUS AddDevice(WDFDRIVER driver,PWDFDEVICE_INIT init){
    WDFDEVICE device;WDF_OBJECT_ATTRIBUTES attr,file_attr;WDF_PNPPOWER_EVENT_CALLBACKS pnp;
    WDF_FILEOBJECT_CONFIG file;WDF_IO_QUEUE_CONFIG queue;NTSTATUS status;
    DECLARE_CONST_UNICODE_STRING(sddl,L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");
    UNREFERENCED_PARAMETER(driver);
    WdfDeviceInitSetCharacteristics(init,FILE_DEVICE_SECURE_OPEN,TRUE);
    WdfDeviceInitSetExclusive(init,TRUE);WdfDeviceInitSetDeviceType(init,FILE_DEVICE_UNKNOWN);
    status=WdfDeviceInitAssignSDDLString(init,&sddl);if(!NT_SUCCESS(status))return status;
    WDF_FILEOBJECT_CONFIG_INIT(&file,NULL,NULL,FileCleanup);
    WDF_OBJECT_ATTRIBUTES_INIT(&file_attr);file_attr.SynchronizationScope=WdfSynchronizationScopeInherit;
    file_attr.ExecutionLevel=WdfExecutionLevelPassive;
    WdfDeviceInitSetFileObjectConfig(init,&file,&file_attr);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);pnp.EvtDevicePrepareHardware=Prepare;
    pnp.EvtDeviceReleaseHardware=Release;pnp.EvtDeviceD0Exit=PowerExit;
    WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,DEVICE_CONTEXT);
    attr.ExecutionLevel=WdfExecutionLevelPassive;attr.SynchronizationScope=WdfSynchronizationScopeDevice;
    status=WdfDeviceCreate(&init,&attr,&device);if(!NT_SUCCESS(status))return status;
    Context(device)->device=device;
    status=WdfDeviceCreateDeviceInterface(device,&GUID_DEVINTERFACE_XM5_LDAC,NULL);
    if(!NT_SUCCESS(status))return status;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue,WdfIoQueueDispatchSequential);
    queue.EvtIoDeviceControl=Control;
    return WdfIoQueueCreate(device,&queue,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING path){
    WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,AddDevice);
    return WdfDriverCreate(object,path,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}
