#include "task.h"
#include "kmalloc.h"
#include "klog.h"
#include "vga.h"
#include "serial.h"
#include "vmm.h"
#include "gdt.h"
#include "syscall.h"
#include "elf.h"
#include "lock.h"
#include "cpu.h"
#include "window.h"
#include "vfs.h"
#include "string.h"
#include <stddef.h>

static uint64_t next_id = 1;
static spinlock_t id_lock;

#define TASK_DEFAULT_TIMESLICE 4

void task_init_global() {
    /* Reset the global task ID allocator. */
    next_id = 1;
    spin_init(&id_lock);
}

static void enqueue_task(runqueue_t* rq, task_t* task) {
    /* Append a task to the tail of a runqueue. */
    uint64_t flags = spin_lock_irqsave(&rq->lock);
    task->next = NULL;
    if (rq->tail) {
        rq->tail->next = task;
        rq->tail = task;
    } else {
        rq->head = rq->tail = task;
    }
    rq->count++;
    spin_unlock_irqrestore(&rq->lock, flags);
}

static task_t* dequeue_task(runqueue_t* rq) {
    /* Remove the head task from a runqueue. */
    uint64_t flags = spin_lock_irqsave(&rq->lock);
    if (!rq->head) {
        spin_unlock_irqrestore(&rq->lock, flags);
        return NULL;
    }
    task_t* task = rq->head;
    rq->head = task->next;
    if (!rq->head) rq->tail = NULL;
    rq->count--;
    spin_unlock_irqrestore(&rq->lock, flags);
    return task;
}

static cpu_t* pick_least_loaded_cpu() {
    /* Choose the CPU with the smallest runnable load. */
    cpu_lock_all();
    int count = cpu_get_count_unlocked();
    cpu_t* least = cpu_get_by_index_unlocked(0);
    
    /* load = tasks in runqueue + current non-idle task */
    int min_load = least->runqueue.count;
    if (least->current_task && least->current_task->id != 0) min_load++;
    
    for (int i = 1; i < count; i++) {
        cpu_t* cpu = cpu_get_by_index_unlocked(i);
        int load = cpu->runqueue.count;
        if (cpu->current_task && cpu->current_task->id != 0) load++;
        
        if (load < min_load) {
            min_load = load;
            least = cpu;
        }
    }
    cpu_unlock_all();
    return least;
}

void task_init_per_cpu() {
    cpu_t* cpu = get_cpu();
    /* Each CPU gets its own runqueue and idle task. */
    spin_init(&cpu->runqueue.lock);
    cpu->runqueue.head = cpu->runqueue.tail = NULL;
    cpu->runqueue.count = 0;
    
    task_t* idle_task = kmalloc(sizeof(task_t));
    idle_task->id = 0;
    idle_task->state = TASK_RUNNING;
    idle_task->cpu_id = cpu->id;
    idle_task->next = NULL;
    idle_task->rsp = 0; 
    idle_task->kernel_rsp = 0;
    idle_task->stack_bottom = NULL;
    idle_task->user_stack_bottom = NULL;
    idle_task->timeslice = 0;
    
    /* The idle task shares the current address space. */
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    idle_task->pml4 = (void*)cr3;

    cpu->idle_task = idle_task;
    cpu->current_task = idle_task;
    cpu->total_ticks = 0;
    cpu->idle_ticks = 0;
    
    klog(LOG_INFO, "TASK", "Task system initialized for CPU");
}

task_t* task_create(void (*entry)()) {
    task_t* new_task = kmalloc(sizeof(task_t));
    
    /* Assign a unique task ID. */
    uint64_t id_flags = spin_lock_irqsave(&id_lock);
    new_task->id = next_id++;
    spin_unlock_irqrestore(&id_lock, id_flags);
    
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    new_task->pml4 = (void*)cr3;

    /* Build a kernel stack and an initial interrupt frame. */
    size_t stack_size = 4096 * 4;
    new_task->stack_bottom = kmalloc(stack_size);
    new_task->user_stack_bottom = NULL;
    uint64_t* stack = (uint64_t*)((uint8_t*)new_task->stack_bottom + stack_size);
    new_task->kernel_rsp = (uint64_t)stack;
    
    *(--stack) = 0x10;            // SS
    *(--stack) = (uint64_t)stack + 8; // RSP
    *(--stack) = 0x202;           // RFLAGS
    *(--stack) = 0x08;            // CS
    *(--stack) = (uint64_t)entry; // RIP
    
    *(--stack) = 0; 
    *(--stack) = 0;

    for (int i = 0; i < 15; i++) *(--stack) = 0;

    new_task->rsp = (uint64_t)stack;
    new_task->state = TASK_READY;
    new_task->timeslice = TASK_DEFAULT_TIMESLICE;
    new_task->total_ticks = 0;
    
    cpu_t* target = pick_least_loaded_cpu();
    new_task->cpu_id = target->id;

    enqueue_task(&target->runqueue, new_task);

    return new_task;
}

task_t* task_create_user(void (*entry)()) {
    task_t* new_task = kmalloc(sizeof(task_t));
    
    /* Assign a unique task ID. */
    uint64_t id_flags = spin_lock_irqsave(&id_lock);
    new_task->id = next_id++;
    spin_unlock_irqrestore(&id_lock, id_flags);

    /* User tasks get their own cloned address space. */
    new_task->pml4 = vmm_create_address_space();

    /* Allocate separate user and kernel stacks. */
    size_t ustack_size = 4096 * 4;
    new_task->user_stack_bottom = kmalloc(ustack_size);
    uint64_t* ustack = (uint64_t*)((uint8_t*)new_task->user_stack_bottom + ustack_size);

    size_t kstack_size = 4096 * 4;
    new_task->stack_bottom = kmalloc(kstack_size);
    uint64_t* kstack = (uint64_t*)((uint8_t*)new_task->stack_bottom + kstack_size);
    new_task->kernel_rsp = (uint64_t)kstack;
    
    uint64_t* stack = kstack;

    *(--stack) = 0x1B;            // SS (User Data)
    *(--stack) = (uint64_t)ustack; // RSP
    *(--stack) = 0x202;           // RFLAGS
    *(--stack) = 0x23;            // CS (User Code)
    *(--stack) = (uint64_t)entry; // RIP
    
    *(--stack) = 0; 
    *(--stack) = 0;

    for (int i = 0; i < 15; i++) *(--stack) = 0;

    new_task->rsp = (uint64_t)stack;
    new_task->state = TASK_READY;
    new_task->timeslice = TASK_DEFAULT_TIMESLICE;
    new_task->total_ticks = 0;
    
    cpu_t* target = pick_least_loaded_cpu();
    new_task->cpu_id = target->id;

    enqueue_task(&target->runqueue, new_task);

    return new_task;
}

task_t* task_exec(void* elf_data) {
    void* entry;
    void* pml4;
    if (!elf_load(elf_data, &entry, &pml4)) {
        return NULL;
    }

    /* Create the runnable task from the loaded ELF entry point. */
    task_t* new_task = kmalloc(sizeof(task_t));
    
    /* Assign a unique task ID. */
    uint64_t id_flags = spin_lock_irqsave(&id_lock);
    new_task->id = next_id++;
    spin_unlock_irqrestore(&id_lock, id_flags);

    new_task->pml4 = pml4;

    /* User tasks get separate user and kernel stacks. */
    size_t ustack_size = 4096 * 4;
    new_task->user_stack_bottom = kmalloc(ustack_size);
    uint64_t* ustack = (uint64_t*)((uint8_t*)new_task->user_stack_bottom + ustack_size);

    size_t kstack_size = 4096 * 4;
    new_task->stack_bottom = kmalloc(kstack_size);
    uint64_t* kstack = (uint64_t*)((uint8_t*)new_task->stack_bottom + kstack_size);
    new_task->kernel_rsp = (uint64_t)kstack;
    
    uint64_t* stack = kstack;

    *(--stack) = 0x1B;            // SS (User Data)
    *(--stack) = (uint64_t)ustack; // RSP
    *(--stack) = 0x202;           // RFLAGS
    *(--stack) = 0x23;            // CS (User Code)
    *(--stack) = (uint64_t)entry; // RIP
    
    *(--stack) = 0; 
    *(--stack) = 0;

    for (int i = 0; i < 15; i++) *(--stack) = 0;

    new_task->rsp = (uint64_t)stack;
    new_task->state = TASK_READY;
    new_task->timeslice = TASK_DEFAULT_TIMESLICE;
    new_task->total_ticks = 0;
    
    cpu_t* target = pick_least_loaded_cpu();
    new_task->cpu_id = target->id;

    enqueue_task(&target->runqueue, new_task);

    return new_task;
}

uint64_t task_fork() {
    cpu_t* cpu = get_cpu();
    task_t* parent = cpu->current_task;
    
    /* Fork duplicates the current task into a child process. */
    task_t* child = kmalloc(sizeof(task_t));
    
    uint64_t id_flags = spin_lock_irqsave(&id_lock);
    child->id = next_id++;
    spin_unlock_irqrestore(&id_lock, id_flags);

    child->pml4 = vmm_clone_address_space(parent->pml4);
    
    size_t kstack_size = 4096 * 4;
    child->stack_bottom = kmalloc(kstack_size);
    child->user_stack_bottom = parent->user_stack_bottom ? kmalloc(4096 * 4) : NULL;
    
    /* Copy the parent's kernel stack so the child resumes in the same context. */
    uint64_t stack_top_offset = parent->kernel_rsp - parent->rsp;
    memcpy((void*)(child->stack_bottom + kstack_size - stack_top_offset), (void*)parent->rsp, stack_top_offset);
    
    child->kernel_rsp = (uint64_t)child->stack_bottom + kstack_size;
    child->rsp = child->kernel_rsp - stack_top_offset;
    
    /* Child returns 0 from fork. */
    uint64_t* child_rax = (uint64_t*)child->rsp; 
    *child_rax = 0; 

    child->state = TASK_READY;
    child->cpu_id = parent->cpu_id;
    child->timeslice = TASK_DEFAULT_TIMESLICE;
    child->total_ticks = 0;
    
    enqueue_task(&cpu->runqueue, child);
    
    return child->id;
}

static void task_reap(task_t* task) {
    /* Reclaim all memory and mappings owned by a finished task. */
    if (task->pml4) {
        vmm_destroy_address_space(task->pml4);
    }
    window_destroy_by_pid(task->id);
    if (task->stack_bottom) kfree(task->stack_bottom);
    if (task->user_stack_bottom) kfree(task->user_stack_bottom);
    kfree(task);
}

task_t* task_exec_file(const char* path) {
    /* Load a file into memory and hand it to the ELF loader. */
    int fd = vfs_open(path);
    if (fd < 0) return NULL;

    uint32_t size = vfs_size(fd);
    void* buf = kmalloc(size);
    vfs_read(fd, buf, size);
    vfs_close(fd);

    task_t* task = task_exec(buf);
    kfree(buf);
    return task;
}

static task_t* task_steal() {
    /* Idle CPUs try to steal runnable work from others. */
    cpu_t* self = get_cpu();
    int count = cpu_get_count();
    for (int i = 0; i < count; i++) {
        cpu_t* victim = cpu_get_by_index(i);
        if (victim == self) continue;
        
        /* Attempt to steal from the victim's runqueue. */
        task_t* task = dequeue_task(&victim->runqueue);
        if (task) {
            task->cpu_id = self->id;
            return task;
        }
    }
    return NULL;
}

uint64_t task_schedule(uint64_t current_rsp) {
    cpu_t* cpu = get_cpu();
    if (!cpu->current_task) return current_rsp;
    cpu->need_resched = 0;

    cpu->current_task->rsp = current_rsp;
    
    /* Requeue the outgoing task if it is still runnable. */
    if (cpu->current_task->state == TASK_RUNNING) {
        cpu->current_task->state = TASK_READY;
        if (cpu->current_task->id != 0) {
            enqueue_task(&cpu->runqueue, cpu->current_task);
        }
    }

    /* Pull the next task from the local runqueue or steal work if needed. */
    task_t* next = NULL;
    while (1) {
        next = dequeue_task(&cpu->runqueue);
        
        /* Try work stealing if the local queue is empty. */
        if (!next) {
            next = task_steal();
            if (!next) break;
        }

        if (next->state == TASK_EXITED) {
            task_reap(next);
            continue;
        }

        if (next->state == TASK_READY) {
            break;
        }
        
        enqueue_task(&cpu->runqueue, next);
    }

    if (!next) {
        next = cpu->idle_task;
    }

    cpu->current_task = next;
    cpu->current_task->state = TASK_RUNNING;
    cpu->current_task->timeslice = (cpu->current_task->id == 0) ? 0 : TASK_DEFAULT_TIMESLICE;

    tss_set_rsp0(cpu->current_task->kernel_rsp);
    syscall_set_kernel_stack(cpu->current_task->kernel_rsp);
    vmm_switch_address_space(cpu->current_task->pml4);

    return cpu->current_task->rsp;
}

void task_yield() {
    /* Request a reschedule and trigger the scheduler interrupt. */
    task_request_reschedule();
    __asm__ volatile ("int $32"); 
}

void task_request_reschedule() {
    /* Mark the current CPU as needing a reschedule. */
    cpu_t* cpu = get_cpu();
    cpu->need_resched = 1;
}

int task_get_all_info(task_info_t* tasks, uint32_t max_tasks) {
    /* Snapshot current-task and runqueue state across CPUs. */
    uint32_t count = 0;
    int cpu_count = cpu_get_count();
    
    for (int i = 0; i < cpu_count; i++) {
        cpu_t* cpu = cpu_get_by_index(i);
        
        /* 1. Current task. */
        if (cpu->current_task && count < max_tasks) {
            tasks[count].id = cpu->current_task->id;
            tasks[count].cpu_id = cpu->id;
            tasks[count].total_ticks = cpu->current_task->total_ticks;
            tasks[count].state = (uint32_t)cpu->current_task->state;
            count++;
        }
        
        /* 2. Tasks in runqueue. */
        uint64_t flags = spin_lock_irqsave(&cpu->runqueue.lock);
        task_t* curr = cpu->runqueue.head;
        while (curr && count < max_tasks) {
            tasks[count].id = curr->id;
            tasks[count].cpu_id = cpu->id;
            tasks[count].total_ticks = curr->total_ticks;
            tasks[count].state = (uint32_t)curr->state;
            count++;
            curr = curr->next;
        }
        spin_unlock_irqrestore(&cpu->runqueue.lock, flags);
    }
    
    return count;
}

int task_needs_schedule() {
    cpu_t* cpu = get_cpu();
    return cpu->need_resched;
}

void task_timer_tick() {
    /* Advance CPU/task accounting and enforce the current timeslice. */
    cpu_t* cpu = get_cpu();
    task_t* current = cpu->current_task;
    
    cpu->total_ticks++;
    if (current) {
        current->total_ticks++;
        if (current->id == 0) {
            cpu->idle_ticks++;
        }
    }

    if (!current) {
        cpu->need_resched = 1;
        return;
    }

    if (current->state != TASK_RUNNING) {
        cpu->need_resched = 1;
        return;
    }

    if (current->id == 0) {
        if (cpu->runqueue.count != 0) {
            cpu->need_resched = 1;
        }
        return;
    }

    if (current->timeslice > 0) {
        current->timeslice--;
    }

    if (current->timeslice == 0) {
        cpu->need_resched = 1;
    }
}
