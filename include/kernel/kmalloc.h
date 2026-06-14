#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>
#include <stdint.h>

/**
 * @file kmalloc.h
 * @brief Simple kernel heap allocator.
 */

typedef struct {
    size_t alloc_count;
    size_t free_count;
    size_t live_allocations;
    size_t bytes_used;
    size_t peak_bytes_used;
    size_t bytes_free;
    size_t largest_free_block;
    size_t failed_allocations;
} kmalloc_stats_t;

/**
 * Initialize the kernel heap allocator.
 */
void kmalloc_init();

/**
 * Allocate a block from the kernel heap.
 */
void* kmalloc(size_t size);

/**
 * Free a previously allocated heap block.
 */
void kfree(void* ptr);

/**
 * Snapshot allocator statistics.
 */
void kmalloc_get_stats(kmalloc_stats_t* out_stats);

#endif
