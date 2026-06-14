#include "gdt.h"
#include "cpu.h"
#include <stddef.h>
#include "string.h"

extern void gdt_load(struct gdt_ptr* ptr);
extern void tss_load();

static void gdt_set_entry(cpu_t* cpu, int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    cpu->gdt.entries[i].base_low = (base & 0xFFFF);
    cpu->gdt.entries[i].base_middle = (base >> 16) & 0xFF;
    cpu->gdt.entries[i].base_high = (base >> 24) & 0xFF;
    cpu->gdt.entries[i].limit_low = (limit & 0xFFFF);
    cpu->gdt.entries[i].granularity = (limit >> 16) & 0x0F;
    cpu->gdt.entries[i].granularity |= gran & 0xF0;
    cpu->gdt.entries[i].access = access;
}

static void gdt_set_tss(cpu_t* cpu, uint64_t base, uint32_t limit) {
    struct gdt_tss_entry* entry = &cpu->gdt.tss;
    entry->limit_low = (limit & 0xFFFF);
    entry->base_low = (base & 0xFFFF);
    entry->base_middle = (base >> 16) & 0xFF;
    entry->access = 0x89; // Present, Executable, Accessible, TSS
    entry->granularity = (limit >> 16) & 0x0F;
    entry->base_high = (base >> 24) & 0xFF;
    entry->base_higher = (base >> 32) & 0xFFFFFFFF;
    entry->reserved = 0;
}

void tss_set_rsp0(uint64_t rsp) {
    cpu_t* cpu = get_cpu();
    cpu->tss_entry.rsp0 = rsp;
}

void gdt_init() {
    cpu_t* cpu = get_cpu();

    cpu->gdt_ptr.limit = sizeof(cpu->gdt) - 1;
    cpu->gdt_ptr.base = (uint64_t)&cpu->gdt;

    gdt_set_entry(cpu, 0, 0, 0, 0, 0);                // Null
    gdt_set_entry(cpu, 1, 0, 0xFFFFFFFF, 0x9A, 0xAF); // Kernel Code
    gdt_set_entry(cpu, 2, 0, 0xFFFFFFFF, 0x92, 0xAF); // Kernel Data
    gdt_set_entry(cpu, 3, 0, 0xFFFFFFFF, 0xF2, 0xAF); // User Data
    gdt_set_entry(cpu, 4, 0, 0xFFFFFFFF, 0xFA, 0xAF); // User Code

    // Setup TSS
    memset(&cpu->tss_entry, 0, sizeof(struct tss_entry));
    cpu->tss_entry.iopb_offset = sizeof(struct tss_entry);
    
    gdt_set_tss(cpu, (uint64_t)&cpu->tss_entry, sizeof(struct tss_entry) - 1);

    gdt_load(&cpu->gdt_ptr);
    tss_load();
}
