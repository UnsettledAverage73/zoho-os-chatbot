#include "kmalloc.h"
#include "vmm.h"
#include "pmm.h"
#include "klog.h"
#include "lock.h"
#include "string.h"

#define HEAP_START 0x1000000 // 16MB
#define HEAP_SIZE  0x1000000 // 16MB

struct kmalloc_header {
    size_t size;
    int is_free;
    struct kmalloc_header* next;
};

static struct kmalloc_header* free_list_head = NULL;
static spinlock_t kmalloc_lock;
static kmalloc_stats_t kmalloc_stats;

void kmalloc_init() {
    spin_init(&kmalloc_lock);
    
    /* Reserve heap frames in PMM; the bootstrap identity map already covers them. */
    for (uint64_t addr = HEAP_START; addr < HEAP_START + HEAP_SIZE; addr += PAGE_SIZE) {
        (void)pmm_alloc_frame(); 
    }

    free_list_head = (struct kmalloc_header*)HEAP_START;
    free_list_head->size = HEAP_SIZE - sizeof(struct kmalloc_header);
    free_list_head->is_free = 1;
    free_list_head->next = NULL;
    
    memset(&kmalloc_stats, 0, sizeof(kmalloc_stats_t));
    kmalloc_stats.bytes_free = free_list_head->size;
    kmalloc_stats.largest_free_block = free_list_head->size;

    klog(LOG_INFO, "KMALLOC", "Kernel heap initialized at 16MB (Size: 16MB)");
}

void* kmalloc(size_t size) {
    uint64_t flags = spin_lock_irqsave(&kmalloc_lock);
    /* Align all allocations to 8 bytes. */
    size = (size + 7) & ~7;

    struct kmalloc_header* curr = free_list_head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            /* Split the block if enough room remains. */
            if (curr->size >= size + sizeof(struct kmalloc_header) + 8) {
                struct kmalloc_header* new_block = (struct kmalloc_header*)((uint8_t*)curr + sizeof(struct kmalloc_header) + size);
                new_block->size = curr->size - size - sizeof(struct kmalloc_header);
                new_block->is_free = 1;
                new_block->next = curr->next;

                curr->size = size;
                curr->next = new_block;
            }
            curr->is_free = 0;
            kmalloc_stats.alloc_count++;
            kmalloc_stats.live_allocations++;
            kmalloc_stats.bytes_used += curr->size;
            if (kmalloc_stats.bytes_used > kmalloc_stats.peak_bytes_used) {
                kmalloc_stats.peak_bytes_used = kmalloc_stats.bytes_used;
            }
            spin_unlock_irqrestore(&kmalloc_lock, flags);
            return (void*)((uint8_t*)curr + sizeof(struct kmalloc_header));
        }
        curr = curr->next;
    }

    kmalloc_stats.failed_allocations++;
    spin_unlock_irqrestore(&kmalloc_lock, flags);
    klog(LOG_ERROR, "KMALLOC", "Out of memory!");
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;
    uint64_t flags = spin_lock_irqsave(&kmalloc_lock);

    struct kmalloc_header* header = (struct kmalloc_header*)((uint8_t*)ptr - sizeof(struct kmalloc_header));
    if (header->is_free) {
        spin_unlock_irqrestore(&kmalloc_lock, flags);
        return;
    }

    header->is_free = 1;
    kmalloc_stats.free_count++;
    if (kmalloc_stats.live_allocations > 0) {
        kmalloc_stats.live_allocations--;
    }
    if (kmalloc_stats.bytes_used >= header->size) {
        kmalloc_stats.bytes_used -= header->size;
    } else {
        kmalloc_stats.bytes_used = 0;
    }

    /* Merge adjacent free blocks to reduce fragmentation. */
    struct kmalloc_header* curr = free_list_head;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += curr->next->size + sizeof(struct kmalloc_header);
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
    spin_unlock_irqrestore(&kmalloc_lock, flags);
}

void kmalloc_get_stats(kmalloc_stats_t* out_stats) {
    if (!out_stats) return;

    uint64_t flags = spin_lock_irqsave(&kmalloc_lock);
    /* Recompute free-space summary from the free list. */
    size_t bytes_free = 0;
    size_t largest_free = 0;

    struct kmalloc_header* curr = free_list_head;
    while (curr) {
        if (curr->is_free) {
            bytes_free += curr->size;
            if (curr->size > largest_free) {
                largest_free = curr->size;
            }
        }
        curr = curr->next;
    }

    kmalloc_stats.bytes_free = bytes_free;
    kmalloc_stats.largest_free_block = largest_free;
    *out_stats = kmalloc_stats;
    spin_unlock_irqrestore(&kmalloc_lock, flags);
}
