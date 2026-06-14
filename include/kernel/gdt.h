#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/**
 * @file gdt.h
 * @brief Global Descriptor Table and TSS definitions.
 */

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

/**
 * GDT pointer used by lgdt.
 */
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/**
 * Task State Segment layout for ring transitions.
 */
struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

/**
 * 16-byte TSS descriptor layout.
 */
struct gdt_tss_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
    uint32_t base_higher;
    uint32_t reserved;
} __attribute__((packed));

/**
 * Initialize the per-CPU GDT and TSS.
 */
void gdt_init();

/**
 * Update ring-0 stack pointer in the current CPU's TSS.
 */
void tss_set_rsp0(uint64_t rsp);

#endif
