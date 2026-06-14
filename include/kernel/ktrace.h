#ifndef KTRACE_H
#define KTRACE_H

#include <stdint.h>

/**
 * @file ktrace.h
 * @brief Lightweight kernel trace buffer.
 */

/**
 * Initialize the trace buffer.
 */
void ktrace_init(void);

/**
 * Append one trace event.
 */
void ktrace_log(const char* event);

#endif
