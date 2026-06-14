#include "idt.h"
#include "vga.h"
#include "serial.h"
#include "keyboard.h"
#include "task.h"
#include "io.h"
#include "cpu.h"
#include "klog.h"
#include "mouse.h"
#include "apic.h"
#include "vmm.h"
#include "pmm.h"
#include "string.h"

extern void pit_handler();

static struct idt_entry idt[256];
static struct idt_ptr idt_p;

extern void idt_flush(struct idt_ptr* ptr);

// External ISR stubs
extern void isr0(); extern void isr1(); extern void isr2(); extern void isr3();
extern void isr4(); extern void isr5(); extern void isr6(); extern void isr7();
extern void isr8(); extern void isr9(); extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();
extern void isr32(); extern void isr33(); extern void isr34(); extern void isr35();
extern void isr36(); extern void isr37(); extern void isr38(); extern void isr39();
extern void isr40(); extern void isr41(); extern void isr42(); extern void isr43();
extern void isr44(); extern void isr45(); extern void isr46(); extern void isr47();

static void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].offset_middle = (base >> 16) & 0xFFFF;
    idt[num].offset_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].selector = sel;
    idt[num].ist = 0;
    idt[num].type_attr = flags;
    idt[num].reserved = 0;
}

static void pic_remap() {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFF); // Mask all Master IRQs
    outb(0xA1, 0xFF); // Mask all Slave IRQs
}

static void pic_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}

void idt_init_global() {
    idt_p.limit = (sizeof(struct idt_entry) * 256) - 1;
    idt_p.base = (uint64_t)&idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    pic_remap();

    void (*isrs[48])() = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
        isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39,
        isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47
    };

    for (int i = 0; i < 48; i++) {
        idt_set_gate(i, (uint64_t)isrs[i], 0x08, 0x8E); // 0x8E: Present, ring 0, interrupt gate
    }
}

void idt_init_per_cpu() {
    idt_flush(&idt_p);
}

static const char* exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

static uint64_t handle_page_fault(struct interrupt_frame* frame) {
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    
    cpu_t* cpu = get_cpu();
    if (!cpu->current_task) return (uint64_t)frame;

    uint64_t flags = vmm_get_flags(cpu->current_task->pml4, cr2);
    
    // Check if it's a write fault (ERR bit 1) and the page is CoW
    if ((frame->error_code & 2) && (flags & PAGE_COW)) {
        uint64_t old_phys = vmm_get_phys(cpu->current_task->pml4, cr2) & ~0xFFFULL;
        
        if (pmm_get_ref((void*)old_phys) > 1) {
            // Split: allocate new frame and copy
            void* new_frame = pmm_alloc_frame();
            // We need to access the physical memory. 
            // Since we don't have a physical map, we'll use a temporary mapping or 
            // rely on the fact that the page is currently mapped in the current address space.
            memcpy(new_frame, (void*)(cr2 & ~0xFFFULL), 4096);
            
            // Map new frame as writable, removing CoW bit
            uint64_t new_flags = (flags & ~PAGE_COW) | PAGE_WRITABLE;
            vmm_map(cpu->current_task->pml4, cr2 & ~0xFFFULL, (uint64_t)new_frame, new_flags);
            
            // Free the old frame (dec ref count)
            pmm_free_frame((void*)old_phys);
        } else {
            // We are the last owner, just make it writable
            uint64_t new_flags = (flags & ~PAGE_COW) | PAGE_WRITABLE;
            vmm_map(cpu->current_task->pml4, cr2 & ~0xFFFULL, old_phys, new_flags);
        }
        return (uint64_t)frame;
    }

    klog(LOG_ERROR, "MEM", "Unhandled Page Fault at %p (ERR: %x)", cr2, frame->error_code);
    if ((frame->cs & 3) == 3) {
        klog(LOG_ERROR, "TASK", "Terminating process %d due to segfault", cpu->current_task->id);
        cpu->current_task->state = TASK_EXITED;
        return task_schedule((uint64_t)frame);
    }

    while(1) __asm__("hlt");
}

uint64_t isr_handler(struct interrupt_frame* frame) {
    uint64_t rsp = (uint64_t)frame;

    if (frame->int_no < 32) {
        if (frame->int_no == 14) {
            return handle_page_fault(frame);
        }

        // Special case: User Mode exception (e.g. Page Fault)
        if ((frame->cs & 3) == 3) {
            klog(LOG_ERROR, "KERNEL", "User process violation! Terminating.");
            cpu_t* cpu = get_cpu();
            if (cpu->current_task) {
                cpu->current_task->state = TASK_EXITED;
                return task_schedule(rsp);
            }
        }

        vga_print("\nKERNEL PANIC: ");
        vga_print(exception_messages[frame->int_no]);
        vga_print("\n");
        serial_print("\nKERNEL PANIC: ");
        serial_print(exception_messages[frame->int_no]);
        serial_print("\n");
        
        vga_print("RIP: "); vga_print_hex(frame->rip);
        serial_print("RIP: "); serial_print_hex(frame->rip);
        vga_print(" CS: "); vga_print_hex(frame->cs);
        serial_print(" CS: "); serial_print_hex(frame->cs);
        vga_print(" RFLAGS: "); vga_print_hex(frame->rflags);
        serial_print(" RFLAGS: "); serial_print_hex(frame->rflags);
        vga_print("\n");
        serial_print("\n");

        vga_print("RSP: "); vga_print_hex(frame->rsp);
        serial_print("RSP: "); serial_print_hex(frame->rsp);
        vga_print(" SS: "); vga_print_hex(frame->ss);
        serial_print(" SS: "); serial_print_hex(frame->ss);
        vga_print(" ERR: "); vga_print_hex(frame->error_code);
        serial_print(" ERR: "); serial_print_hex(frame->error_code);
        vga_print("\n");
        serial_print("\n");
        
        vga_print("RAX: "); vga_print_hex(frame->rax);
        serial_print("RAX: "); serial_print_hex(frame->rax);
        vga_print(" RBX: "); vga_print_hex(frame->rbx);
        serial_print(" RBX: "); serial_print_hex(frame->rbx);
        vga_print(" RCX: "); vga_print_hex(frame->rcx);
        serial_print(" RCX: "); serial_print_hex(frame->rcx);
        vga_print("\n");
        serial_print("\n");

        vga_print("RDX: "); vga_print_hex(frame->rdx);
        serial_print("RDX: "); serial_print_hex(frame->rdx);
        vga_print(" RDI: "); vga_print_hex(frame->rdi);
        serial_print(" RDI: "); serial_print_hex(frame->rdi);
        vga_print(" RSI: "); vga_print_hex(frame->rsi);
        serial_print(" RSI: "); serial_print_hex(frame->rsi);
        vga_print("\n");
        serial_print("\n");

        while(1) __asm__("hlt");
    } else if (frame->int_no >= 32 && frame->int_no < 48) {
        // Handle IRQ
        uint8_t irq = (uint8_t)(frame->int_no - 32);

        if (frame->int_no == 32) {
            pit_handler();
            if (task_needs_schedule()) {
                rsp = task_schedule(rsp);
            }
        } else if (frame->int_no == 33) {
            keyboard_handler();
        } else if (frame->int_no == 36) {
            serial_handler();
        } else if (frame->int_no == 44) {
            mouse_handler();
        }

        pic_eoi(irq);
        lapic_eoi();
    }
    return rsp;
}
