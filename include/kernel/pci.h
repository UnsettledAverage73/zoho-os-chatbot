#ifndef PCI_H
#define PCI_H

#include <stdint.h>

/**
 * @file pci.h
 * @brief PCI configuration space helpers.
 */

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t  revision_id;
    uint8_t  prog_if;
    uint8_t  subclass;
    uint8_t  class_code;
    uint8_t  cache_line_size;
    uint8_t  latency_timer;
    uint8_t  header_type;
    uint8_t  bist;
} pci_header_t;

/**
 * Read a 32-bit PCI config dword.
 */
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

/**
 * Write a 32-bit PCI config dword.
 */
void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);

/**
 * Enumerate PCI devices and initialize known drivers.
 */
void pci_init();

#endif
