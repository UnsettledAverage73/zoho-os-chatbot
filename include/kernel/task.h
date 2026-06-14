#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stddef.h>
#include "lock.h"

/**
 * @file task.h
 * @brief Task and runqueue management.
 */

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_EXITED
} task_state_t;

typedef struct task {
    uint64_t id;
    uint64_t rsp; // Current stack pointer
    void* stack_bottom;
    void* user_stack_bottom;
    uint64_t kernel_rsp; // Kernel stack pointer for Ring 3 -> Ring 0
    void* pml4;
    task_state_t state;
    int cpu_id; 
    uint32_t timeslice;
    uint64_t total_ticks;
    struct task* next;
} task_t;

typedef struct {
    spinlock_t lock;
    task_t* head;
    task_t* tail;
    uint32_t count;
} runqueue_t;

typedef struct {
    uint64_t id;
    uint32_t cpu_id;
    uint64_t total_ticks;
    uint32_t state;
} task_info_t;

/**
 * Initialize global task state.
 */
void task_init_global();

/**
 * Initialize per-CPU task state.
 */
void task_init_per_cpu();

/**
 * Create a kernel task.
 */
task_t* task_create(void (*entry)());

/**
 * Create a user task.
 */
task_t* task_create_user(void (*entry)());

/**
 * Execute an ELF image as a user task.
 */
task_t* task_exec(void* elf_data);

/**
 * Load and execute a file as a task.
 */
task_t* task_exec_file(const char* path);

/**
 * Fork the current task.
 */
uint64_t task_fork();

/**
 * Yield the CPU to the scheduler.
 */
void task_yield();

/**
 * Account one timer tick for the current task.
 */
void task_timer_tick();

/**
 * Check whether the current CPU needs rescheduling.
 */
int task_needs_schedule();

/**
 * Request a reschedule on the current CPU.
 */
void task_request_reschedule();

/**
 * Run the scheduler and return the next stack pointer to restore.
 */
uint64_t task_schedule(uint64_t current_rsp);

/**
 * Snapshot all tasks into an output array.
 */
int task_get_all_info(task_info_t* tasks, uint32_t max_tasks);

#endif
