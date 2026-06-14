#include "pmm.h"
#include "vga.h"
#include "serial.h"
#include "lock.h"

#define MAX_FRAMES (1024 * 1024) // 4GiB / 4KiB
static uint32_t frame_bitmap[MAX_FRAMES / 32];
static uint64_t free_stack[MAX_FRAMES];
static int stack_ptr = -1;
static uint32_t ref_counts[MAX_FRAMES];
static uint64_t total_frames = 0;
static spinlock_t pmm_lock;

extern uint8_t kernel_start[];
extern uint8_t kernel_end[];

/*
 * Bitmap helpers.
 * One bit represents one physical 4 KiB frame.
 */
static void bitmap_set(uint64_t frame) {
    frame_bitmap[frame / 32] |= (1 << (frame % 32));
}

static void bitmap_unset(uint64_t frame) {
    frame_bitmap[frame / 32] &= ~(1 << (frame % 32));
}

static int bitmap_test(uint64_t frame) {
    return frame_bitmap[frame / 32] & (1 << (frame % 32));
}

void pmm_init(struct multiboot_info* mb_info) {
    spin_init(&pmm_lock);
    uint64_t flags = spin_lock_irqsave(&pmm_lock);

    /* Start pessimistic: assume every frame is unavailable. */
    for (int i = 0; i < MAX_FRAMES / 32; i++) frame_bitmap[i] = 0xFFFFFFFF;
    for (int i = 0; i < MAX_FRAMES; i++) ref_counts[i] = 0;

    struct multiboot_tag* tag;
    uint64_t mbi_end = (uint64_t)mb_info + mb_info->total_size;
    uint64_t max_addr = 0;

    for (tag = (struct multiboot_tag*)mb_info->tags;
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7))) {
        
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            struct multiboot_tag_mmap* mmap = (struct multiboot_tag_mmap*)tag;
            for (struct multiboot_mmap_entry* entry = mmap->entries;
                 (uint8_t*)entry < (uint8_t*)tag + tag->size;
                 entry = (struct multiboot_mmap_entry*)((uint8_t*)entry + mmap->entry_size)) {

                if (entry->type == 1) { // Available
                    /* Mark firmware-reported usable RAM as free frame-by-frame. */
                    for (uint64_t addr = entry->addr; addr < entry->addr + entry->len; addr += PAGE_SIZE) {
                        uint64_t frame = addr / PAGE_SIZE;
                        if (frame < MAX_FRAMES) {
                            bitmap_unset(frame);
                            if (addr + PAGE_SIZE > max_addr) max_addr = addr + PAGE_SIZE;
                        }
                    }
                }
            }
        }
    }

    uint64_t k_end = (uint64_t)kernel_end;
    /* Reserve the kernel image itself. */
    for (uint64_t addr = 0; addr < k_end; addr += PAGE_SIZE) {
        bitmap_set(addr / PAGE_SIZE);
    }
    
    /* Reserve the Multiboot info block so later code can still read it. */
    for (uint64_t addr = (uint64_t)mb_info; addr < mbi_end; addr += PAGE_SIZE) {
        bitmap_set(addr / PAGE_SIZE);
    }

    /* Only scan up to the highest usable address discovered. */
    uint64_t max_frame = max_addr / PAGE_SIZE;
    if (max_frame > MAX_FRAMES) max_frame = MAX_FRAMES;

    /* Build the free-frame stack from the bitmap. */
    for (uint64_t f = 0; f < max_frame; f++) {
        if (!bitmap_test(f)) {
            free_stack[++stack_ptr] = f * PAGE_SIZE;
            total_frames++;
        }
    }
    
    spin_unlock_irqrestore(&pmm_lock, flags);
    serial_print("PMM Initialized. Total free frames: ");
    // Note: serial_print doesn't support numbers easily in some versions, 
    // but we'll stick to basic messages or implement a helper if needed.
    serial_print("Done.\n");
}

uint64_t pmm_get_total_frames() {
    return total_frames;
}

void* pmm_alloc_frame() {
    uint64_t flags = spin_lock_irqsave(&pmm_lock);
    if (stack_ptr < 0) {
        spin_unlock_irqrestore(&pmm_lock, flags);
        return NULL;
    }
    /* Pop one free frame and mark it in-use. */
    uint64_t addr = free_stack[stack_ptr--];
    bitmap_set(addr / PAGE_SIZE);
    ref_counts[addr / PAGE_SIZE] = 1;
    spin_unlock_irqrestore(&pmm_lock, flags);
    return (void*)addr;
}

void pmm_free_frame(void* frame) {
    uint64_t flags = spin_lock_irqsave(&pmm_lock);
    uint64_t addr = (uint64_t)frame;
    uint64_t f = addr / PAGE_SIZE;
    
    if (f >= MAX_FRAMES) {
        spin_unlock_irqrestore(&pmm_lock, flags);
        return;
    }

    if (ref_counts[f] > 0) {
        ref_counts[f]--;
        if (ref_counts[f] == 0) {
            /* Return the frame to the free stack only when nobody references it. */
            if (bitmap_test(f)) {
                bitmap_unset(f);
                free_stack[++stack_ptr] = addr;
            }
        }
    }
    spin_unlock_irqrestore(&pmm_lock, flags);
}

void pmm_ref_inc(void* frame) {
    uint64_t flags = spin_lock_irqsave(&pmm_lock);
    uint64_t f = (uint64_t)frame / PAGE_SIZE;
    if (f < MAX_FRAMES) {
        ref_counts[f]++;
    }
    spin_unlock_irqrestore(&pmm_lock, flags);
}

uint32_t pmm_get_ref(void* frame) {
    uint64_t flags = spin_lock_irqsave(&pmm_lock);
    uint64_t f = (uint64_t)frame / PAGE_SIZE;
    uint32_t count = (f < MAX_FRAMES) ? ref_counts[f] : 0;
    spin_unlock_irqrestore(&pmm_lock, flags);
    return count;
}

uint64_t pmm_get_free_count() {
    uint64_t flags = spin_lock_irqsave(&pmm_lock);
    uint64_t count = stack_ptr + 1;
    spin_unlock_irqrestore(&pmm_lock, flags);
    return count;
}
