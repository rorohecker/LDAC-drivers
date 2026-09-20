/* Kernel debugger errors only. ETW/WPP collection remains a Phase 4 gate. */
#pragma once
#define XM5_TRACE_INFO(msg, ...) ((void)0)
#define XM5_TRACE_WARN(msg, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_WARNING_LEVEL,"xm5ldac: " msg "\n",__VA_ARGS__)
#define XM5_TRACE_ERR(msg, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_ERROR_LEVEL,"xm5ldac: " msg "\n",__VA_ARGS__)
