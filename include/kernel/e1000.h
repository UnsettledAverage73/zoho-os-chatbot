#ifndef E1000_H
#define E1000_H

#include <stdint.h>

/**
 * @file e1000.h
 * @brief Intel E1000 network adapter interface.
 */

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100E

/* Register offsets. */
#define REG_CTRL          0x0000
#define REG_STATUS        0x0008
#define REG_EECD          0x0010
#define REG_EERD          0x0014
#define REG_ICR           0x00C0
#define REG_IMS           0x00D0
#define REG_IMC           0x00D8
#define REG_RCTL          0x0100
#define REG_TCTL          0x0400
#define REG_TIPG          0x0410
#define REG_RDBAL         0x2800
#define REG_RDBAH         0x2804
#define REG_RDLEN         0x2808
#define REG_RDH           0x2810
#define REG_RDT           0x2818
#define REG_TDBAL         0x3800
#define REG_TDBAH         0x3804
#define REG_TDLEN         0x3808
#define REG_TDH           0x3810
#define REG_TDT           0x3818
#define REG_MTA           0x5200
#define REG_RAL           0x5400
#define REG_RAH           0x5404

/* CTRL bits. */
#define CTRL_RST          (1 << 26)
#define CTRL_SLU          (1 << 6)
#define CTRL_ASDE         (1 << 5)

/* Receive control bits. */
#define RCTL_EN           (1 << 1)
#define RCTL_SBP          (1 << 2)
#define RCTL_UPE          (1 << 3)
#define RCTL_MPE          (1 << 4)
#define RCTL_LPE          (1 << 5)
#define RCTL_LBM_NONE     (0 << 6)
#define RCTL_RDMTS_HALF   (0 << 8)
#define RCTL_MO_36        (0 << 12)
#define RCTL_BAM          (1 << 15)
#define RCTL_BSIZE_2048   (0 << 16)
#define RCTL_SECRC        (1 << 26)

/* Transmit control bits. */
#define TCTL_EN           (1 << 1)
#define TCTL_PSP          (1 << 3)
#define TCTL_CT           (0x10 << 4)
#define TCTL_COLD         (0x40 << 12)

#define TSTA_DD           (1 << 0) // Descriptor Done

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

typedef struct {
    uint8_t mac[6];
} e1000_device_t;

/**
 * Initialize an Intel E1000 device found on PCI.
 */
void e1000_init(uint8_t bus, uint8_t slot, uint8_t func);

/**
 * Send a packet through the transmit ring.
 */
void e1000_send_packet(const void* data, uint16_t len);

/**
 * Poll the receive ring for completed packets.
 */
void e1000_poll();

/**
 * Copy the NIC MAC address into the caller buffer.
 */
void e1000_get_mac(uint8_t* mac);

#endif
