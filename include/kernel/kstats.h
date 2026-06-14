#ifndef KSTATS_H
#define KSTATS_H

#include <stdint.h>

/**
 * @file kstats.h
 * @brief Kernel-wide telemetry snapshot.
 */

typedef struct {
    uint64_t uptime_ms;
    uint64_t total_memory;
    uint64_t used_memory;
    uint32_t active_tasks;
    uint64_t page_faults;
} kernel_stats_t;

/**
 * Initialize kernel statistics tracking.
 */
void kstats_init(void);

/**
 * Read the current statistics snapshot.
 */
kernel_stats_t kstats_get(void);

/**
 * Refresh live statistics values.
 */
void kstats_update(void);

#endif
