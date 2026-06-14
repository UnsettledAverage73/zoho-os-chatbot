#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER (1ULL << 2)
#define PAGE_COW (1ULL << 9) // Available bit in page table entry
#define PAGE_HUGE (1ULL << 7) // Page Size bit in PDE

/**
 * @file vmm.h
 * @brief Virtual memory manager interface.
 *
 * The VMM manages x86_64 page tables, address-space creation, mapping,
 * unmapping, and address translation.
 */

/**
 * Capture the active page table root from CR3.
 */
void vmm_init();

/**
 * Create a new address space with kernel mappings copied in.
 *
 * @return Pointer to a new PML4, or NULL on allocation failure.
 */
void* vmm_create_address_space();

/**
 * Clone an address space using copy-on-write semantics.
 *
 * @param src_pml4 Source PML4 to clone.
 * @return Pointer to a cloned PML4, or NULL on allocation failure.
 */
void* vmm_clone_address_space(void* src_pml4);

/**
 * Destroy an address space and release its page-table frames.
 *
 * @param pml4 Root page table to destroy.
 */
void vmm_destroy_address_space(void* pml4);

/**
 * Switch the active address space by loading CR3.
 *
 * @param pml4 PML4 to activate.
 */
void vmm_switch_address_space(void* pml4);

/**
 * Map one virtual address to one physical address.
 *
 * @param pml4 Root page table of the address space to modify.
 * @param virt Virtual address to map.
 * @param phys Physical address to map to.
 * @param flags Page table flags to apply.
 */
void vmm_map(void* pml4, uint64_t virt, uint64_t phys, uint64_t flags);

/**
 * Map a contiguous range of memory.
 *
 * @param pml4 Root page table of the address space to modify.
 * @param virt Starting virtual address.
 * @param phys Starting physical address.
 * @param size Number of bytes to map.
 * @param flags Page table flags to apply.
 */
void vmm_map_range(void* pml4, uint64_t virt, uint64_t phys, uint64_t size, uint64_t flags);

/**
 * Unmap one virtual address.
 *
 * @param pml4 Root page table of the address space to modify.
 * @param virt Virtual address to unmap.
 */
void vmm_unmap(void* pml4, uint64_t virt);

/**
 * Unmap a contiguous range of memory.
 *
 * @param pml4 Root page table of the address space to modify.
 * @param virt Starting virtual address.
 * @param size Number of bytes to unmap.
 */
void vmm_unmap_range(void* pml4, uint64_t virt, uint64_t size);

/**
 * Translate a virtual address into a physical address.
 *
 * @param pml4 Root page table to inspect.
 * @param virt Virtual address to translate.
 * @return Physical address, or 0 if unmapped.
 */
uint64_t vmm_get_phys(void* pml4, uint64_t virt);

/**
 * Read the flags for a mapped virtual address.
 *
 * @param pml4 Root page table to inspect.
 * @param virt Virtual address to inspect.
 * @return Raw page table flags, or 0 if unmapped.
 */
uint64_t vmm_get_flags(void* pml4, uint64_t virt);

#endif
