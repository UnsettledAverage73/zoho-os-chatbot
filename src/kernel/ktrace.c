#include "ktrace.h"
#include "klog.h"
#include "string.h"

#define MAX_TRACE_EVENTS 1024

static const char* trace_buffer[MAX_TRACE_EVENTS];
static uint32_t trace_index = 0;

void ktrace_init(void) {
    klog(LOG_INFO, "KTRACE", "Initializing kernel tracing...");
    /* Reset the in-memory trace ring. */
    memset(trace_buffer, 0, sizeof(trace_buffer));
    trace_index = 0;
    klog(LOG_INFO, "KTRACE", "Kernel tracing initialized.");
}

void ktrace_log(const char* event) {
    /* Store trace events sequentially until the buffer fills. */
    if (trace_index < MAX_TRACE_EVENTS) {
        trace_buffer[trace_index++] = event;
    }
}
