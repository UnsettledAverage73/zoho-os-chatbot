#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/**
 * @file idt.h
 * @brief Interrupt Descriptor Table structures and setup.
 */

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

/**
 * IDT pointer used by lidt.
 */
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/**
 * Interrupt stack frame saved by the assembly stubs.
 */
struct interrupt_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t int_no, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

/**
 * Build the shared IDT and remap the legacy PIC.
 */
void idt_init_global();

/**
 * Load the IDT on the current CPU.
 */
void idt_init_per_cpu();

#endif
