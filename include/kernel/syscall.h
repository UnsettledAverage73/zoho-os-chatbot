#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "window.h"
#include "task.h"

/**
 * @file syscall.h
 * @brief Syscall interface exposed to user space.
 */

/**
 * Initialize the syscall MSRs and entry path.
 */
void syscall_init();

/**
 * Set the kernel stack used on syscall entry.
 *
 * @param stack Kernel stack pointer for the current CPU.
 */
void syscall_set_kernel_stack(uint64_t stack);

/**
 * Create a new GUI window and map its client buffer into user space.
 */
uint32_t sys_create_window(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

/**
 * Pop the next GUI event for a window.
 */
int sys_get_event(uint32_t win_id, gui_event_t* out_event);

/**
 * Terminate the current task.
 */
void sys_exit(int status);

/**
 * Open a file through VFS.
 */
int sys_open(const char* path);

/**
 * Read from a file descriptor through VFS.
 */
int sys_read(int fd, void* buffer, uint32_t count);

/**
 * Write bytes to the console.
 */
int sys_write(int fd, const void* buffer, uint32_t count);

/**
 * Close a file descriptor.
 */
void sys_close(int fd);

/**
 * Execute a file as a task.
 */
uint32_t sys_exec(const char* path);

/**
 * Yield the CPU to the scheduler.
 */
void sys_yield();

/**
 * Fork the current task.
 */
uint64_t sys_fork();

/**
 * Report free physical frame count.
 */
uint64_t sys_free_frames();

typedef struct {
    uint64_t total_frames;
    uint64_t free_frames;
    uint32_t cpu_count;
    struct {
        uint64_t total_ticks;
        uint64_t idle_ticks;
    } cpus[16];
    uint32_t task_count;
} sys_info_t;

/**
 * Fill a sys_info_t structure with system telemetry.
 */
int sys_get_sys_info(sys_info_t* info);

/**
 * Copy a snapshot of task info records.
 */
int sys_get_tasks(task_info_t* tasks, uint32_t max_tasks);

#endif
