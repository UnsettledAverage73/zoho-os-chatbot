#include "vmm.h"
#include "pmm.h"
#include "lock.h"
#include <stdint.h>
#include <stddef.h>

#define PML4_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr) (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)   (((addr) >> 12) & 0x1FF)

static uint64_t* current_pml4;
static spinlock_t vmm_lock;

void vmm_init() {
    spin_init(&vmm_lock);
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    /* Record the currently active PML4 so later mappings can extend it. */
    current_pml4 = (uint64_t*)cr3;
}

void* vmm_create_address_space() {
    /* Allocate a fresh PML4 for the new address space. */
    uint64_t* new_pml4 = (uint64_t*)pmm_alloc_frame();
    if (!new_pml4) return NULL;

    /* Clear all entries before wiring in the kernel mapping. */
    for (int i = 0; i < 512; i++) new_pml4[i] = 0;

    uint64_t flags = spin_lock_irqsave(&vmm_lock);
    /* Share the kernel half of the address space. */
    new_pml4[0] = current_pml4[0];
    spin_unlock_irqrestore(&vmm_lock, flags);

    return new_pml4;
}

void vmm_switch_address_space(void* pml4) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pml4));
}

static uint64_t* alloc_zeroed_table() {
    /* Page tables are just physical frames zeroed before use. */
    uint64_t* next_table = (uint64_t*)pmm_alloc_frame();
    if (!next_table) return NULL;

    for (int i = 0; i < 512; i++) next_table[i] = 0;
    return next_table;
}

static uint64_t* get_existing_table(uint64_t* table, uint64_t index) {
    if (!(table[index] & PAGE_PRESENT)) return NULL;
    return (uint64_t*)(table[index] & ~0xFFFULL);
}

static uint64_t* ensure_next_table(uint64_t* table, uint64_t index) {
    uint64_t* next = get_existing_table(table, index);
    if (next) return next;

    /* Create the next paging level only if it does not already exist. */
    uint64_t* new_table = alloc_zeroed_table();
    if (!new_table) return NULL;

    uint64_t irq_flags = spin_lock_irqsave(&vmm_lock);
    next = get_existing_table(table, index);
    if (!next) {
        table[index] = (uint64_t)new_table | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        next = new_table;
        new_table = NULL;
    }
    spin_unlock_irqrestore(&vmm_lock, irq_flags);

    if (new_table) {
        pmm_free_frame(new_table);
    }

    return next;
}

void* vmm_clone_address_space(void* src_pml4_ptr) {
    uint64_t* src_pml4 = (uint64_t*)src_pml4_ptr;
    uint64_t* dst_pml4 = (uint64_t*)vmm_create_address_space();
    
    /* Clone only user space. The shared kernel mapping stays intact. */
    for (int i = 1; i < 512; i++) {
        if (src_pml4[i] & PAGE_PRESENT) {
            uint64_t* src_pdpt = (uint64_t*)(src_pml4[i] & ~0xFFFULL);
            uint64_t* dst_pdpt = ensure_next_table(dst_pml4, i);
            
            for (int j = 0; j < 512; j++) {
                if (src_pdpt[j] & PAGE_PRESENT) {
                    uint64_t* src_pd = (uint64_t*)(src_pdpt[j] & ~0xFFFULL);
                    uint64_t* dst_pd = ensure_next_table(dst_pdpt, j);
                    
                    for (int k = 0; k < 512; k++) {
                        if (src_pd[k] & PAGE_PRESENT) {
                            if (src_pd[k] & PAGE_HUGE) {
                                /* Mark huge pages as copy-on-write if writable. */
                                uint64_t phys = src_pd[k] & ~0x1FFFFFULL;
                                uint64_t flags = src_pd[k] & 0xFFF;
                                if (flags & PAGE_WRITABLE) {
                                    flags = (flags & ~PAGE_WRITABLE) | PAGE_COW;
                                    src_pd[k] = phys | flags | PAGE_PRESENT;
                                }
                                dst_pd[k] = phys | flags | PAGE_PRESENT;
                                /* Track shared ownership of the underlying huge page. */
                                pmm_ref_inc((void*)phys);
                                continue;
                            }
                            
                            uint64_t* src_pt = (uint64_t*)(src_pd[k] & ~0xFFFULL);
                            uint64_t* dst_pt = ensure_next_table(dst_pd, k);
                            
                            for (int l = 0; l < 512; l++) {
                                if (src_pt[l] & PAGE_PRESENT) {
                                    uint64_t phys = src_pt[l] & ~0xFFFULL;
                                    uint64_t flags = src_pt[l] & 0xFFF;
                                    
                                    /* Writable pages become shared copy-on-write pages. */
                                    if (flags & PAGE_WRITABLE) {
                                        flags = (flags & ~PAGE_WRITABLE) | PAGE_COW;
                                        src_pt[l] = phys | flags | PAGE_PRESENT;
                                    }
                                    
                                    dst_pt[l] = phys | flags | PAGE_PRESENT;
                                    pmm_ref_inc((void*)phys);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    /* Flush the TLB because we changed permissions in the current address space. */
    __asm__ volatile ("mov %%cr3, %%rax\nmov %%rax, %%cr3" ::: "rax");
    
    return dst_pml4;
}

void vmm_destroy_address_space(void* pml4_ptr) {
    uint64_t* pml4 = (uint64_t*)pml4_ptr;
    for (int i = 1; i < 512; i++) {
        if (pml4[i] & PAGE_PRESENT) {
            uint64_t* pdpt = (uint64_t*)(pml4[i] & ~0xFFFULL);
            for (int j = 0; j < 512; j++) {
                if (pdpt[j] & PAGE_PRESENT) {
                    uint64_t* pd = (uint64_t*)(pdpt[j] & ~0xFFFULL);
                    for (int k = 0; k < 512; k++) {
                        if (pd[k] & PAGE_PRESENT) {
                            uint64_t* pt = (uint64_t*)(pd[k] & ~0xFFFULL);
                            for (int l = 0; l < 512; l++) {
                                if (pt[l] & PAGE_PRESENT) {
                                    pmm_free_frame((void*)(pt[l] & ~0xFFFULL));
                                }
                            }
                            pmm_free_frame(pt);
                        }
                    }
                    pmm_free_frame(pd);
                }
            }
            pmm_free_frame(pdpt);
        }
    }
    pmm_free_frame(pml4);
}

void vmm_map(void* pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t* pdpt = ensure_next_table((uint64_t*)pml4, PML4_INDEX(virt));
    if (!pdpt) return;
    uint64_t* pd = ensure_next_table(pdpt, PDPT_INDEX(virt));
    if (!pd) return;

    /* Support 2 MiB huge pages for large contiguous mappings. */
    if (flags & PAGE_HUGE) {
        uint64_t irq_flags = spin_lock_irqsave(&vmm_lock);
        pd[PD_INDEX(virt)] = (phys & ~0x1FFFFFULL) | flags | PAGE_PRESENT;
        spin_unlock_irqrestore(&vmm_lock, irq_flags);
        return;
    }

    uint64_t* pt = ensure_next_table(pd, PD_INDEX(virt));
    if (!pt) return;

    uint64_t irq_flags = spin_lock_irqsave(&vmm_lock);
    pt[PT_INDEX(virt)] = (phys & ~0xFFFULL) | flags | PAGE_PRESENT;
    spin_unlock_irqrestore(&vmm_lock, irq_flags);
}

void vmm_map_range(void* pml4, uint64_t virt, uint64_t phys, uint64_t size, uint64_t flags) {
    for (uint64_t i = 0; i < size; ) {
        /* Prefer huge pages when alignment and size allow it. */
        if ((virt + i) % 0x200000 == 0 && (phys + i) % 0x200000 == 0 && (size - i) >= 0x200000) {
            vmm_map(pml4, virt + i, phys + i, flags | PAGE_HUGE);
            i += 0x200000;
        } else {
            vmm_map(pml4, virt + i, phys + i, flags);
            i += 0x1000;
        }
    }
}

void vmm_unmap(void* pml4, uint64_t virt) {
    uint64_t irq_flags = spin_lock_irqsave(&vmm_lock);
    uint64_t* pdpt = get_existing_table((uint64_t*)pml4, PML4_INDEX(virt));
    if (!pdpt) { spin_unlock_irqrestore(&vmm_lock, irq_flags); return; }
    uint64_t* pd   = get_existing_table(pdpt, PDPT_INDEX(virt));
    if (!pd) { spin_unlock_irqrestore(&vmm_lock, irq_flags); return; }
    
    /* Unmap the correct level based on whether this is a huge page. */
    if (pd[PD_INDEX(virt)] & PAGE_HUGE) {
        pd[PD_INDEX(virt)] = 0;
    } else {
        uint64_t* pt = get_existing_table(pd, PD_INDEX(virt));
        if (pt) {
            pt[PT_INDEX(virt)] = 0;
        }
    }
    
    /* Remove the cached translation for this virtual address. */
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    spin_unlock_irqrestore(&vmm_lock, irq_flags);
}

void vmm_unmap_range(void* pml4, uint64_t virt, uint64_t size) {
    for (uint64_t i = 0; i < size; ) {
        /* Advance by the correct page size for huge vs standard mappings. */
        uint64_t* pdpt = get_existing_table((uint64_t*)current_pml4, PML4_INDEX(virt + i));
        uint64_t* pd = pdpt ? get_existing_table(pdpt, PDPT_INDEX(virt + i)) : NULL;
        
        if (pd && (pd[PD_INDEX(virt + i)] & PAGE_HUGE)) {
            vmm_unmap(pml4, virt + i);
            i += 0x200000;
        } else {
            vmm_unmap(pml4, virt + i);
            i += 0x1000;
        }
    }
}

uint64_t vmm_get_phys(void* pml4_ptr, uint64_t virt) {
    uint64_t* pml4 = (uint64_t*)pml4_ptr;
    uint64_t* pdpt = get_existing_table(pml4, PML4_INDEX(virt));
    if (!pdpt) return 0;
    uint64_t* pd = get_existing_table(pdpt, PDPT_INDEX(virt));
    if (!pd) return 0;
    
    if (pd[PD_INDEX(virt)] & PAGE_HUGE) {
        return (pd[PD_INDEX(virt)] & ~0x1FFFFFULL) | (virt & 0x1FFFFF);
    }
    
    uint64_t* pt = get_existing_table(pd, PD_INDEX(virt));
    if (!pt) return 0;
    
    return (pt[PT_INDEX(virt)] & ~0xFFFULL) | (virt & 0xFFF);
}

uint64_t vmm_get_flags(void* pml4_ptr, uint64_t virt) {
    uint64_t* pml4 = (uint64_t*)pml4_ptr;
    uint64_t* pdpt = get_existing_table(pml4, PML4_INDEX(virt));
    if (!pdpt) return 0;
    uint64_t* pd = get_existing_table(pdpt, PDPT_INDEX(virt));
    if (!pd) return 0;
    
    if (pd[PD_INDEX(virt)] & PAGE_HUGE) {
        return pd[PD_INDEX(virt)] & 0xFFF;
    }
    
    uint64_t* pt = get_existing_table(pd, PD_INDEX(virt));
    if (!pt) return 0;
    
    return pt[PT_INDEX(virt)] & 0xFFF;
}
