#include "acpi.h"
#include "string.h"
#include "klog.h"
#include "kmalloc.h"

static uint32_t lapic_addr = 0;
static uint8_t cpu_ids[64];
static int cpu_count = 0;

static acpi_rsdp_t* find_rsdp() {
    /* ACPI RSDP lives in the legacy BIOS search window. */
    for (uint64_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        if (memcmp((void*)addr, "RSD PTR ", 8) == 0) {
            return (acpi_rsdp_t*)addr;
        }
    }
    return NULL;
}

void acpi_init() {
    klog(LOG_INFO, "ACPI", "Initializing ACPI...");
    acpi_rsdp_t* rsdp = find_rsdp();
    if (!rsdp) {
        klog(LOG_ERROR, "ACPI", "RSDP not found!");
        return;
    }

    /* Walk the RSDT to find the MADT. */
    acpi_sdt_header_t* rsdt = (acpi_sdt_header_t*)(uint64_t)rsdp->rsdt_address;
    int entries = (rsdt->length - sizeof(acpi_sdt_header_t)) / 4;
    uint32_t* sdt_ptrs = (uint32_t*)((uint8_t*)rsdt + sizeof(acpi_sdt_header_t));

    for (int i = 0; i < entries; i++) {
        acpi_sdt_header_t* header = (acpi_sdt_header_t*)(uint64_t)sdt_ptrs[i];
        if (memcmp(header->signature, "APIC", 4) == 0) {
            acpi_madt_t* madt = (acpi_madt_t*)header;
            lapic_addr = madt->local_apic_address;
            klog(LOG_INFO, "ACPI", "Found MADT");

            /* Read local APIC entries and keep only enabled CPUs. */
            uint8_t* entry_ptr = (uint8_t*)madt + sizeof(acpi_madt_t);
            uint8_t* end_ptr = (uint8_t*)madt + madt->header.length;

            while (entry_ptr < end_ptr) {
                acpi_madt_entry_t* entry = (acpi_madt_entry_t*)entry_ptr;
                if (entry->type == 0) { // Local APIC
                    acpi_madt_local_apic_t* lapic = (acpi_madt_local_apic_t*)entry;
                    if (lapic->flags & 1) { // Enabled
                        cpu_ids[cpu_count++] = lapic->apic_id;
                    }
                }
                entry_ptr += entry->length;
            }
            klog(LOG_INFO, "ACPI", "Enumerated CPUs");
        }
    }
}

uint32_t acpi_get_lapic_addr() {
    return lapic_addr;
}

uint8_t* acpi_get_cpu_ids(int* count) {
    *count = cpu_count;
    return cpu_ids;
}
