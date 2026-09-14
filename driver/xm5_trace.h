/* Lightweight trace stubs for Phase 4 ETW/WPP integration.
 * On a WDK build these can be replaced with WPP_INIT_TRACING / DoTraceMessage.
 * Keep macros side-effect free when tracing is disabled.
 */
#pragma once
#ifndef XM5_TRACE_H
#define XM5_TRACE_H

#if defined(XM5_ENABLE_WPP)
/* Placeholder: include generated WPP header when the provider is authored. */
#define XM5_TRACE_INFO(msg, ...) ((void)0)
#define XM5_TRACE_WARN(msg, ...) ((void)0)
#define XM5_TRACE_ERR(msg, ...)  ((void)0)
#else
#define XM5_TRACE_INFO(msg, ...) ((void)0)
#define XM5_TRACE_WARN(msg, ...) ((void)0)
#define XM5_TRACE_ERR(msg, ...)  ((void)0)
#endif

#endif
