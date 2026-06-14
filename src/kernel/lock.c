#include "lock.h"

static spinlock_t* registered_locks[64];
static volatile uint32_t registered_lock_count = 0;

void spin_init_named(spinlock_t* lock, const char* name) {
    /* Register the lock name and clear all counters. */
    lock->locked = 0;
    lock->name = name;
    lock->acquisitions = 0;
    lock->contentions = 0;
    lock->spin_loops = 0;

    uint32_t index = __sync_fetch_and_add(&registered_lock_count, 1);
    if (index < (sizeof(registered_locks) / sizeof(registered_locks[0]))) {
        registered_locks[index] = lock;
    }
}

void spin_lock(spinlock_t* lock) {
    /* Simple test-and-set spin loop with pause hints. */
    uint64_t spins = 0;
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        spins++;
        __asm__ volatile ("pause");
    }
    __sync_fetch_and_add(&lock->acquisitions, 1);
    if (spins != 0) {
        __sync_fetch_and_add(&lock->contentions, 1);
        __sync_fetch_and_add(&lock->spin_loops, spins);
    }
}

void spin_unlock(spinlock_t* lock) {
    /* Release the lock. */
    __sync_lock_release(&lock->locked);
}

uint64_t spin_lock_irqsave(spinlock_t* lock) {
    /* Save interrupt state before taking the lock. */
    uint64_t flags;
    __asm__ volatile (
        "pushfq\n"
        "pop %0\n"
        "cli"
        : "=rm"(flags)
        :
        : "memory"
    );
    spin_lock(lock);
    return flags;
}

void spin_unlock_irqrestore(spinlock_t* lock, uint64_t flags) {
    /* Release the lock and restore interrupt state. */
    spin_unlock(lock);
    __asm__ volatile (
        "push %0\n"
        "popfq"
        :
        : "rm"(flags)
        : "memory"
    );
}

uint32_t lock_get_registered_count(void) {
    uint32_t count = registered_lock_count;
    uint32_t capacity = (uint32_t)(sizeof(registered_locks) / sizeof(registered_locks[0]));
    return (count < capacity) ? count : capacity;
}

int lock_get_stats(uint32_t index, lock_stats_t* out_stats) {
    if (!out_stats) return 0;

    uint32_t count = lock_get_registered_count();
    if (index >= count) return 0;

    spinlock_t* lock = registered_locks[index];
    if (!lock) return 0;

    out_stats->name = lock->name;
    out_stats->acquisitions = lock->acquisitions;
    out_stats->contentions = lock->contentions;
    out_stats->spin_loops = lock->spin_loops;
    return 1;
}
