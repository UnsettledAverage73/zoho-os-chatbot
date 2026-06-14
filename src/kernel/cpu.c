#include "cpu.h"
#include "kmalloc.h"
#include "klog.h"

#define MSR_GS_BASE      0xC0000101
#define MSR_KERNEL_GS_BASE 0xC0000102

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static cpu_t bsp_cpu;
static cpu_t* cpus[64];
static int cpu_count = 0;
static spinlock_t cpu_lock;

cpu_t* get_cpu() {
    cpu_t* cpu;
    // We use swapgs in syscalls, so in kernel gs_base is the cpu structure
    __asm__ volatile ("mov %%gs:16, %0" : "=r"(cpu)); // Offset 16 is 'self'
    return cpu;
}

void cpu_early_init() {
    spin_init(&cpu_lock);
    cpu_t* cpu = &bsp_cpu;
    cpu->id = 0;
    cpu->current_task = NULL;
    cpu->self = cpu;
    cpu->kernel_stack = 0;
    cpu->user_stack = 0;
    cpu->need_resched = 0;

    uint64_t flags = spin_lock_irqsave(&cpu_lock);
    cpus[cpu_count++] = cpu;
    spin_unlock_irqrestore(&cpu_lock, flags);

    // Set GS base
    wrmsr(MSR_GS_BASE, (uint64_t)cpu);
    wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)cpu);
}

void cpu_init(uint8_t apic_id) {
    if (apic_id == 0) return; // Already handled by early init
    
    cpu_t* cpu = kmalloc(sizeof(cpu_t));
    cpu->id = apic_id;
    cpu->current_task = NULL;
    cpu->self = cpu;
    cpu->kernel_stack = 0;
    cpu->user_stack = 0;
    cpu->need_resched = 0;

    uint64_t flags = spin_lock_irqsave(&cpu_lock);
    cpus[cpu_count++] = cpu;
    spin_unlock_irqrestore(&cpu_lock, flags);

    // Set GS base
    wrmsr(MSR_GS_BASE, (uint64_t)cpu);
    // Also set KERNEL_GS_BASE for swapgs to work properly
    wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)cpu);
    
    klog(LOG_INFO, "CPU", "CPU initialized");
}

int cpu_get_count() {
    uint64_t flags = spin_lock_irqsave(&cpu_lock);
    int count = cpu_count;
    spin_unlock_irqrestore(&cpu_lock, flags);
    return count;
}

int cpu_get_count_unlocked() {
    return cpu_count;
}

cpu_t* cpu_get_by_index(int index) {
    uint64_t flags = spin_lock_irqsave(&cpu_lock);
    cpu_t* cpu = cpus[index];
    spin_unlock_irqrestore(&cpu_lock, flags);
    return cpu;
}

cpu_t* cpu_get_by_index_unlocked(int index) {
    return cpus[index];
}

void cpu_lock_all() {
    spin_lock(&cpu_lock);
}

void cpu_unlock_all() {
    spin_unlock(&cpu_lock);
}
