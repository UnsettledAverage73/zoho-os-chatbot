#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file elf.h
 * @brief ELF loader structures and helpers.
 */

#define ELF_MAGIC 0x464C457F

typedef struct {
    uint32_t magic;
    uint8_t  elf[12];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) elf_header_t;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed)) elf_phdr_t;

#define PT_LOAD 1

/**
 * Load an ELF image into a new address space.
 *
 * @param elf_data Pointer to the ELF file contents.
 * @param entry_point Output entry address.
 * @param pml4 Output address-space root.
 * @return true on success.
 */
bool elf_load(void* elf_data, void** entry_point, void** pml4);

#endif
