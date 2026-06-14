#ifndef MULTIBOOT2_H
#define MULTIBOOT2_H

#include <stdint.h>

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_info {
    uint32_t total_size;
    uint32_t reserved;
    struct multiboot_tag tags[0];
};

struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
};

struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot_mmap_entry entries[0];
};

#define MULTIBOOT_TAG_TYPE_END 0
#define MULTIBOOT_TAG_TYPE_MODULE 3
#define MULTIBOOT_TAG_TYPE_MMAP 6
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8

struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t type_fb;
    uint16_t reserved;
};

struct multiboot_tag_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    char cmdline[0];
};

#endif
