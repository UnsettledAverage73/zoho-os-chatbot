#ifndef LOCK_H
#define LOCK_H

#include <stdint.h>

/**
 * @file lock.h
 * @brief Spinlock primitives and lock statistics.
 */

typedef struct {
    volatile int locked;
    const char* name;
    volatile uint64_t acquisitions;
    volatile uint64_t contentions;
    volatile uint64_t spin_loops;
} spinlock_t;

typedef struct {
    const char* name;
    uint64_t acquisitions;
    uint64_t contentions;
    uint64_t spin_loops;
} lock_stats_t;

/**
 * Initialize a spinlock with a human-readable name.
 */
void spin_init_named(spinlock_t* lock, const char* name);
#define spin_init(lock) spin_init_named((lock), #lock)

/**
 * Acquire a spinlock.
 */
void spin_lock(spinlock_t* lock);

/**
 * Release a spinlock.
 */
void spin_unlock(spinlock_t* lock);

/**
 * Acquire a spinlock and disable interrupts.
 */
uint64_t spin_lock_irqsave(spinlock_t* lock);

/**
 * Release a spinlock and restore interrupt state.
 */
void spin_unlock_irqrestore(spinlock_t* lock, uint64_t flags);

/**
 * Get the number of registered locks.
 */
uint32_t lock_get_registered_count(void);

/**
 * Read statistics for one registered lock.
 */
int lock_get_stats(uint32_t index, lock_stats_t* out_stats);

#endif
