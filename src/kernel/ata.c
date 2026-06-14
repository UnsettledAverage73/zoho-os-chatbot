#include "ata.h"
#include "io.h"
#include "klog.h"

static int ata_wait(uint8_t bit, int set) {
    /* Poll the ATA status register until a condition is met or we time out. */
    uint32_t timeout = 1000000;
    while (timeout--) {
        uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return -1;
        if (set) {
            if (status & bit) return 0;
        } else {
            if (!(status & bit)) return 0;
        }
    }
    return -2; // Timeout
}

int ata_init() {
    klog(LOG_INFO, "ATA", "Initializing ATA driver (Polling mode)...");
    
    /* Issue IDENTIFY to the primary master. */
    outb(ATA_PRIMARY_IO + ATA_REG_DRIVE, 0xA0);
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_LOW, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_MID, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_HIGH, 0);
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    if (status == 0) {
        klog(LOG_INFO, "ATA", "No drive found.");
        return -1;
    }

    if (ata_wait(ATA_SR_BSY, 0) != 0) return -1;
    if (ata_wait(ATA_SR_DRQ, 1) != 0) return -1;

    /* Drain the IDENTIFY data block. */
    for (int i = 0; i < 256; i++) inw(ATA_PRIMARY_IO + ATA_REG_DATA);

    klog(LOG_INFO, "ATA", "Primary Master detected.");
    return 0;
}

void ata_read_sector(uint32_t lba, uint8_t* buffer) {
    if (ata_wait(ATA_SR_BSY, 0) != 0) return;

    /* Program a 28-bit LBA read command. */
    outb(ATA_PRIMARY_IO + ATA_REG_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, 1);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_LOW, (uint8_t)lba);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    if (ata_wait(ATA_SR_BSY, 0) != 0) return;
    if (ata_wait(ATA_SR_DRQ, 1) != 0) {
        klog(LOG_ERROR, "ATA", "Read error: DRQ not set.");
        return;
    }

    /* Read the 512-byte sector as 256 16-bit words. */
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(ATA_PRIMARY_IO + ATA_REG_DATA);
    }
}

void ata_write_sector(uint32_t lba, uint8_t* buffer) {
    if (ata_wait(ATA_SR_BSY, 0) != 0) return;

    /* Program a 28-bit LBA write command. */
    outb(ATA_PRIMARY_IO + ATA_REG_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT, 1);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_LOW, (uint8_t)lba);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO + ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    if (ata_wait(ATA_SR_BSY, 0) != 0) return;
    if (ata_wait(ATA_SR_DRQ, 1) != 0) {
        klog(LOG_ERROR, "ATA", "Write error: DRQ not set.");
        return;
    }

    /* Write the 512-byte sector as 256 16-bit words. */
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        outw(ATA_PRIMARY_IO + ATA_REG_DATA, ptr[i]);
    }

    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait(ATA_SR_BSY, 0);
}
