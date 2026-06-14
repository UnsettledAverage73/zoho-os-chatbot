#ifndef KLOG_H
#define KLOG_H

#include <stdarg.h>
#include <stddef.h>

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} log_level_t;

#ifndef KLOG_MIN_LEVEL
#define KLOG_MIN_LEVEL LOG_WARN
#endif

void klog(log_level_t level, const char* module, const char* fmt, ...);
void klog_set_level(log_level_t level);
log_level_t klog_get_level();

int vsnprintf(char* str, size_t size, const char* format, va_list ap);

#endif
