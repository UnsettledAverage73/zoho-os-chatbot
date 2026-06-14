#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

/**
 * @file acpi.h
 * @brief ACPI data structures and discovery helpers.
 *
 * The kernel uses ACPI primarily to find the MADT and extract the LAPIC
 * address plus enabled CPU APIC IDs.
 */

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed)) acpi_rsdp_t;

typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct {
    acpi_sdt_header_t header;
    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed)) acpi_madt_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) acpi_madt_entry_t;

typedef struct {
    acpi_madt_entry_t header;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed)) acpi_madt_local_apic_t;

/**
 * Discover ACPI tables and cache the LAPIC address plus CPU IDs.
 */
void acpi_init();

/**
 * Get the physical LAPIC base address discovered from ACPI.
 *
 * @return LAPIC physical address, or 0 if not found.
 */
uint32_t acpi_get_lapic_addr();

/**
 * Get the list of enabled CPU APIC IDs.
 *
 * @param count Output parameter that receives the number of IDs.
 * @return Pointer to an internal APIC-ID array.
 */
uint8_t* acpi_get_cpu_ids(int* count);

#endif
