#include "e1000.h"
#include "pci.h"
#include "vmm.h"
#include "pmm.h"
#include "klog.h"
#include "kmalloc.h"
#include "string.h"
#include "net.h"

#define E1000_MMIO_VIRT 0xFFFFFFFF20000000

static uint64_t mmio_base = 0;
static e1000_rx_desc_t* rx_descs = NULL;
static e1000_tx_desc_t* tx_descs = NULL;
static uint8_t* rx_buffers = NULL;
static uint8_t* tx_buffers = NULL;
static uint16_t tx_tail = 0;
static uint16_t rx_cur = 0;
static uint8_t mac_addr[6];

#define NUM_RX_DESCS 128
#define NUM_TX_DESCS 128

static void write_reg(uint16_t reg, uint32_t val) {
    /* E1000 registers are MMIO, not port I/O. */
    *(volatile uint32_t*)(mmio_base + reg) = val;
}

static uint32_t read_reg(uint16_t reg) {
    /* Read one MMIO register from the NIC. */
    return *(volatile uint32_t*)(mmio_base + reg);
}

static void e1000_read_mac() {
    uint32_t low = read_reg(REG_RAL);
    uint32_t high = read_reg(REG_RAH);

    mac_addr[0] = low & 0xFF;
    mac_addr[1] = (low >> 8) & 0xFF;
    mac_addr[2] = (low >> 16) & 0xFF;
    mac_addr[3] = (low >> 24) & 0xFF;
    mac_addr[4] = high & 0xFF;
    mac_addr[5] = (high >> 8) & 0xFF;
}

void e1000_get_mac(uint8_t* mac) {
    memcpy(mac, mac_addr, 6);
}

void e1000_send_packet(const void* data, uint16_t len) {
    /* Fill one transmit descriptor and advance the tail pointer. */
    tx_descs[tx_tail].length = len;
    tx_descs[tx_tail].status = 0;
    tx_descs[tx_tail].cmd = (1 << 3) | (1 << 0); // RS (Report Status) | EOP (End of Packet)
    
    memcpy((void*)tx_descs[tx_tail].addr, data, len);

    uint16_t old_tail = tx_tail;
    tx_tail = (tx_tail + 1) % NUM_TX_DESCS;
    write_reg(REG_TDT, tx_tail);

    while (!(tx_descs[old_tail].status & 0xF)); // Wait for DD (Descriptor Done)
}

void e1000_poll() {
    if (!mmio_base) return;

    /* Drain completed receive descriptors. */
    while (rx_descs[rx_cur].status & 0x1) { // 0x1 is DD (Descriptor Done)
        uint16_t len = rx_descs[rx_cur].length;
        void* data = (void*)rx_descs[rx_cur].addr;

        net_handle_packet(data, len);

        rx_descs[rx_cur].status = 0;
        uint16_t old_cur = rx_cur;
        rx_cur = (rx_cur + 1) % NUM_RX_DESCS;
        write_reg(REG_RDT, old_cur);
    }
}

void e1000_init(uint8_t bus, uint8_t slot, uint8_t func) {
    klog(LOG_INFO, "E1000", "Initializing Intel E1000 NIC...");

    /* Read BAR0 to locate the MMIO register window. */
    uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
    uint64_t phys_addr = bar0 & ~0xF;
    
    klog(LOG_INFO, "E1000", "BAR0 Physical Address: 0x%x", phys_addr);

    /* Identity-map the MMIO range so the driver can access it. */
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    
    for (uint64_t i = 0; i < 0x20000; i += 4096) {
        vmm_map((void*)cr3, phys_addr + i, phys_addr + i, PAGE_WRITABLE | PAGE_PRESENT | (1 << 3) | (1 << 4)); // PWT | PCD for MMIO
    }
    mmio_base = phys_addr;

    /* Enable bus mastering and memory-space access. */
    uint32_t command = pci_config_read(bus, slot, func, 0x04);
    command |= (1 << 2); // Bus Master
    command |= (1 << 1); // Memory Space
    pci_config_write(bus, slot, func, 0x04, command);

    /* Reset the device. */
    write_reg(REG_CTRL, read_reg(REG_CTRL) | CTRL_RST);
    for(int i = 0; i < 10000; i++) __asm__ ("pause");

    /* Bring the link up and enable auto speed detection. */
    write_reg(REG_CTRL, read_reg(REG_CTRL) | CTRL_SLU | CTRL_ASDE);

    /* Clear the multicast table. */
    for (int i = 0; i < 128; i++) {
        write_reg(REG_MTA + (i * 4), 0);
    }

    /* Read the MAC address from the RAL/RAH registers. */
    e1000_read_mac();
    klog(LOG_INFO, "E1000", "MAC Address: %x:%x:%x:%x:%x:%x", 
         mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    /* Set up the receive ring and buffers. */
    rx_descs = kmalloc(sizeof(e1000_rx_desc_t) * NUM_RX_DESCS + 16);
    rx_descs = (e1000_rx_desc_t*)(((uint64_t)rx_descs + 15) & ~15);
    memset(rx_descs, 0, sizeof(e1000_rx_desc_t) * NUM_RX_DESCS);

    rx_buffers = kmalloc(NUM_RX_DESCS * 2048 + 16);
    rx_buffers = (uint8_t*)(((uint64_t)rx_buffers + 15) & ~15);

    for (int i = 0; i < NUM_RX_DESCS; i++) {
        rx_descs[i].addr = (uint64_t)rx_buffers + (i * 2048);
        rx_descs[i].status = 0;
    }

    write_reg(REG_RDBAL, (uint32_t)((uint64_t)rx_descs & 0xFFFFFFFF));
    write_reg(REG_RDBAH, (uint32_t)((uint64_t)rx_descs >> 32));
    write_reg(REG_RDLEN, NUM_RX_DESCS * sizeof(e1000_rx_desc_t));
    write_reg(REG_RDH, 0);
    write_reg(REG_RDT, NUM_RX_DESCS - 1);
    write_reg(REG_RCTL, RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | RCTL_BAM | RCTL_SECRC | RCTL_BSIZE_2048);

    /* Set up the transmit ring and buffers. */
    tx_descs = kmalloc(sizeof(e1000_tx_desc_t) * NUM_TX_DESCS + 16);
    tx_descs = (e1000_tx_desc_t*)(((uint64_t)tx_descs + 15) & ~15);
    memset(tx_descs, 0, sizeof(e1000_tx_desc_t) * NUM_TX_DESCS);

    tx_buffers = kmalloc(NUM_TX_DESCS * 2048 + 16);
    tx_buffers = (uint8_t*)(((uint64_t)tx_buffers + 15) & ~15);

    for (int i = 0; i < NUM_TX_DESCS; i++) {
        tx_descs[i].addr = (uint64_t)tx_buffers + (i * 2048);
        tx_descs[i].cmd = 0;
        tx_descs[i].status = 0;
    }

    write_reg(REG_TDBAL, (uint32_t)((uint64_t)tx_descs & 0xFFFFFFFF));
    write_reg(REG_TDBAH, (uint32_t)((uint64_t)tx_descs >> 32));
    write_reg(REG_TDLEN, NUM_TX_DESCS * sizeof(e1000_tx_desc_t));
    write_reg(REG_TDH, 0);
    write_reg(REG_TDT, 0);
    write_reg(REG_TIPG, 0x0060200A); // Standard IPG values
    write_reg(REG_TCTL, TCTL_EN | TCTL_PSP | TCTL_CT | TCTL_COLD);

    klog(LOG_INFO, "E1000", "E1000 Initialized successfully.");
}
