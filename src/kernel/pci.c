#include "pci.h"
#include "io.h"
#include "klog.h"
#include "e1000.h"

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
              
    /* Select a config-space DWORD through the PCI config address port. */
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    /* Write a DWORD to the selected PCI config register. */
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

void pci_check_device(uint8_t bus, uint8_t device) {
    uint8_t function = 0;
    uint32_t reg0 = pci_config_read(bus, device, function, 0);
    uint16_t vendor_id = (uint16_t)(reg0 & 0xFFFF);

    if (vendor_id == 0xFFFF) return;

    // Found a device
    uint16_t device_id = (uint16_t)(reg0 >> 16);
    uint32_t reg8 = pci_config_read(bus, device, function, 8);
    uint8_t class_code = (uint8_t)(reg8 >> 24);
    uint8_t subclass = (uint8_t)(reg8 >> 16);

    klog(LOG_INFO, "PCI", "Found device: Vendor=0x%x, Device=0x%x, Class=0x%x, Subclass=0x%x", 
         vendor_id, device_id, class_code, subclass);

    if (vendor_id == E1000_VENDOR_ID && device_id == E1000_DEVICE_ID) {
        e1000_init(bus, device, function);
    }

    // Check for multi-function device
    uint32_t reg12 = pci_config_read(bus, device, function, 0x0C);
    uint8_t header_type = (uint8_t)(reg12 >> 16);
    if ((header_type & 0x80) != 0) {
        /* Multi-function device, check other functions too. */
        for (function = 1; function < 8; function++) {
            reg0 = pci_config_read(bus, device, function, 0);
            if ((uint16_t)(reg0 & 0xFFFF) != 0xFFFF) {
                device_id = (uint16_t)(reg0 >> 16);
                reg8 = pci_config_read(bus, device, function, 8);
                class_code = (uint8_t)(reg8 >> 24);
                subclass = (uint8_t)(reg8 >> 16);
                klog(LOG_INFO, "PCI", "Found function: Vendor=0x%x, Device=0x%x, Class=0x%x, Subclass=0x%x", 
                     (uint16_t)(reg0 & 0xFFFF), device_id, class_code, subclass);
            }
        }
    }
}

void pci_init() {
    /* Enumerate the full PCI bus space. */
    klog(LOG_INFO, "PCI", "Enumerating PCI devices...");
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            pci_check_device((uint8_t)bus, device);
        }
    }
}
