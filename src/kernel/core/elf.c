#include <kernel/core/elf.h>
#include <kernel/mm/vmm.h>
#include <kernel/mm/pmm.h>
#include <kernel/lib/klog.h>
#include <kernel/lib/string.h>

bool elf_load(void* elf_data, void** entry_point, void** pml4) {
    elf_header_t* header = (elf_header_t*)elf_data;

    if (header->magic != ELF_MAGIC) {
        klog(LOG_ERROR, "ELF", "Invalid ELF magic");
        return false;
    }

    *pml4 = vmm_create_address_space();
    *entry_point = (void*)header->entry;

    for (int i = 0; i < header->phnum; i++) {
        elf_phdr_t* phdr = (elf_phdr_t*)((uint8_t*)elf_data + header->phoff + (i * header->phentsize));

        if (phdr->type == PT_LOAD) {
            uint64_t flags = PAGE_USER;
            if (phdr->flags & 2) flags |= PAGE_WRITABLE; // PF_W

            uint64_t vaddr_start = phdr->vaddr;
            uint64_t vaddr_end = phdr->vaddr + phdr->memsz;
            uint64_t page_start = vaddr_start & ~0xFFFULL;
            uint64_t page_end = (vaddr_end + 4095) & ~0xFFFULL;
            uint64_t num_pages = (page_end - page_start) / 4096;

            for (uint64_t p = 0; p < num_pages; p++) {
                uint64_t current_vpage = page_start + (p * 4096);
                
                // Only allocate and map if the page isn't already mapped
                if (!(vmm_get_flags(*pml4, current_vpage) & PAGE_PRESENT)) {
                    void* frame = pmm_alloc_frame();
                    vmm_map(*pml4, current_vpage, (uint64_t)frame, flags);
                    memset(frame, 0, 4096);
                } else {
                    // Update flags if it needs to be writable
                    uint64_t old_flags = vmm_get_flags(*pml4, current_vpage);
                    if ((flags & PAGE_WRITABLE) && !(old_flags & PAGE_WRITABLE)) {
                        uint64_t phys = vmm_get_phys(*pml4, current_vpage);
                        vmm_map(*pml4, current_vpage, phys & ~0xFFFULL, old_flags | PAGE_WRITABLE);
                    }
                }

                // If we need to write to it, temporarily switch or use phys map.
                // It is simpler to temporarily map the frame into kernel space, but 
                // we'll just switch CR3 briefly to copy data.
                uint64_t old_cr3;
                __asm__ volatile ("mov %%cr3, %0" : "=r"(old_cr3));
                vmm_switch_address_space(*pml4);
                
                // Determine what part of the file to copy to this page
                uint64_t copy_offset_in_page = 0;
                uint64_t copy_src_offset = phdr->offset + (p * 4096);
                uint64_t copy_size = 4096;

                if (p == 0) {
                    copy_offset_in_page = vaddr_start & 0xFFF;
                    copy_size = 4096 - copy_offset_in_page;
                } else {
                    copy_src_offset -= (vaddr_start & 0xFFF);
                }

                uint64_t bytes_from_file = 0;
                if (copy_src_offset < phdr->offset + phdr->filesz) {
                    bytes_from_file = (phdr->offset + phdr->filesz) - copy_src_offset;
                    if (bytes_from_file > copy_size) bytes_from_file = copy_size;
                    
                    // Copy data using the virtual address mapped in the new space
                    memcpy((void*)(current_vpage + copy_offset_in_page), (uint8_t*)elf_data + copy_src_offset, bytes_from_file);
                }

                vmm_switch_address_space((void*)old_cr3);
            }
        }
    }

    return true;
}
