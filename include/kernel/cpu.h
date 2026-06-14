#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "task.h"
#include "gdt.h"

/**
 * @file cpu.h
 * @brief Per-CPU state and helpers.
 */

typedef struct cpu {
    uint64_t user_stack;    // 0x00
    uint64_t kernel_stack;  // 0x08
    struct cpu* self;       // 0x10
    task_t* current_task;   // 0x18
    uint8_t id;             // 0x20
    uint8_t need_resched;   // 0x21
    uint8_t pad[6];

    uint64_t total_ticks;
    uint64_t idle_ticks;

    struct {
        struct gdt_entry entries[5];
        struct gdt_tss_entry tss;
    } __attribute__((packed)) gdt;
    struct tss_entry tss_entry;
    struct gdt_ptr gdt_ptr;
    runqueue_t runqueue;
    task_t* idle_task;
} __attribute__((packed)) cpu_t;

/**
 * Get the current CPU structure via GS base.
 */
cpu_t* get_cpu();

/**
 * Initialize the bootstrap processor state.
 */
void cpu_early_init();

/**
 * Initialize an application processor state.
 */
void cpu_init(uint8_t apic_id);

/**
 * Get the number of known CPUs.
 */
int cpu_get_count();

/**
 * Get the number of known CPUs without locking.
 */
int cpu_get_count_unlocked();

/**
 * Get a CPU by index with locking.
 */
cpu_t* cpu_get_by_index(int index);

/**
 * Get a CPU by index without locking.
 */
cpu_t* cpu_get_by_index_unlocked(int index);

/**
 * Lock the global CPU list.
 */
void cpu_lock_all();

/**
 * Unlock the global CPU list.
 */
void cpu_unlock_all();

#endif
