#include "acpi.h"
#include "apic.h"
#include "klog.h"
#include "string.h"
#include "kmalloc.h"
#include "vmm.h"
#include "pit.h"
#include "cpu.h"
#include "gdt.h"
#include "idt.h"
#include "task.h"
#include "serial.h"

extern uint8_t trampoline_start[];
extern uint8_t trampoline_end[];
extern uint64_t trampoline_p4_addr;
extern uint64_t trampoline_stack_addr;
extern uint64_t trampoline_entry_addr;
extern uint64_t trampoline_apic_id;

static volatile int ap_started_count = 0;

void ap_main(uint64_t apic_id_64) {
    uint8_t apic_id = (uint8_t)apic_id_64;
    
    /* Each AP boots like a tiny kernel: CPU, GDT, IDT, LAPIC, timer. */
    cpu_init(apic_id);
    gdt_init();
    idt_init_per_cpu();
    lapic_init();
    lapic_timer_init(100);
    
    task_init_per_cpu();
    __sync_fetch_and_add(&ap_started_count, 1);

    __asm__ volatile ("sti");

    while(1) {
        __asm__ volatile ("hlt");
    }
}

void smp_init() {
    klog(LOG_INFO, "SMP", "Starting AP bring-up...");
    
    /* 1. Copy the real-mode/early-boot trampoline into low memory. */
    uint64_t trampoline_size = (uint64_t)trampoline_end - (uint64_t)trampoline_start;
    memcpy((void*)0x8000, trampoline_start, trampoline_size);

    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    
    uint64_t p4_off = (uint64_t)&trampoline_p4_addr - (uint64_t)trampoline_start;
    uint64_t stack_off = (uint64_t)&trampoline_stack_addr - (uint64_t)trampoline_start;
    uint64_t entry_off = (uint64_t)&trampoline_entry_addr - (uint64_t)trampoline_start;
    uint64_t apic_id_off = (uint64_t)&trampoline_apic_id - (uint64_t)trampoline_start;

    *(uint64_t*)(0x8000 + p4_off) = cr3;
    *(uint64_t*)(0x8000 + entry_off) = (uint64_t)ap_main;

    /* 2. Enumerate enabled CPUs from ACPI. */
    int cpu_count;
    uint8_t* cpu_ids = acpi_get_cpu_ids(&cpu_count);
    uint8_t bsp_id = lapic_get_id();

    for (int i = 0; i < cpu_count; i++) {
        uint8_t apic_id = cpu_ids[i];
        if (apic_id == bsp_id) continue; /* Skip BSP. */

        /* Pass the target APIC ID to the trampoline. */
        *(uint64_t*)(0x8000 + apic_id_off) = apic_id;

        /* Give the AP a private kernel stack. */
        void* stack = kmalloc(4096 * 4);
        *(uint64_t*)(0x8000 + stack_off) = (uint64_t)stack + 4096 * 4;

        int current_count = ap_started_count;

        /* INIT IPI resets the target core. */
        lapic_send_ipi(apic_id, 0x00000500);
        delay_ms(10); // Spec: 10ms after INIT

        /* SIPI starts the trampoline at 0x8000. */
        lapic_send_ipi(apic_id, 0x00000608); // Vector 0x08 -> 0x8000
        delay_us(200); // Spec: 200us after SIPI

        /* A second SIPI is standard if the first wakeup is missed. */
        if (ap_started_count == current_count) {
            lapic_send_ipi(apic_id, 0x00000608);
            delay_us(200);
        }

        /* Wait for the AP to increment the startup counter. */
        uint32_t timeout = 100000; // ~100ms
        while (ap_started_count == current_count && timeout--) {
            __asm__ volatile ("pause");
        }
    }

    klog(LOG_INFO, "SMP", "Multi-core bring-up complete.");
}
