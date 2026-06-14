#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include "multiboot2.h"

#define PAGE_SIZE 4096

/**
 * @file pmm.h
 * @brief Physical memory manager interface.
 *
 * The PMM tracks physical 4 KiB frames, marks usable memory from the
 * Multiboot2 memory map, and provides frame-level allocation and refcounts.
 */

/**
 * Initialize the physical memory manager from Multiboot2 boot info.
 *
 * @param mb_info Multiboot2 information block provided by GRUB.
 */
void pmm_init(struct multiboot_info* mb_info);

/**
 * Allocate one free physical frame.
 *
 * @return Physical frame address, or NULL if no frame is available.
 */
void* pmm_alloc_frame();

/**
 * Return a physical frame to the allocator when its reference count reaches 0.
 *
 * @param frame Physical frame address to free.
 */
void pmm_free_frame(void* frame);

/**
 * Increment the reference count for a shared physical frame.
 *
 * @param frame Physical frame address to reference.
 */
void pmm_ref_inc(void* frame);

/**
 * Get the current reference count for a physical frame.
 *
 * @param frame Physical frame address to inspect.
 * @return Current reference count.
 */
uint32_t pmm_get_ref(void* frame);

/**
 * Count the number of frames currently available for allocation.
 *
 * @return Number of free frames.
 */
uint64_t pmm_get_free_count();

/**
 * Report the total number of frames discovered by the PMM.
 *
 * @return Total usable frame count.
 */
uint64_t pmm_get_total_frames();

#endif
