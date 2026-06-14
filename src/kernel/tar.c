#include "tar.h"
#include "ata.h"
#include "string.h"
#include "klog.h"
#include "kmalloc.h"

static uint32_t tar_start_lba = 0;

struct sector_cache {
    uint32_t lba;
    uint8_t data[512];
    int valid;
};

static struct sector_cache cache = {0, {0}, 0};

static void cached_read_sector(uint32_t lba, uint8_t* buffer) {
    /* Tiny one-sector cache to reduce repeated ATA reads. */
    if (cache.valid && cache.lba == lba) {
        memcpy(buffer, cache.data, 512);
        return;
    }
    ata_read_sector(lba, cache.data);
    cache.lba = lba;
    cache.valid = 1;
    memcpy(buffer, cache.data, 512);
}

void cached_write_sector(uint32_t lba, uint8_t* buffer) {
    /* Keep the cache coherent after sector writes. */
    ata_write_sector(lba, buffer);
    memcpy(cache.data, buffer, 512);
    cache.lba = lba;
    cache.valid = 1;
}

static uint32_t octal_to_int(const char* oct) {
    /* TAR stores sizes as ASCII octal. */
    uint32_t res = 0;
    for (int i = 0; i < 11; i++) {
        if (oct[i] < '0' || oct[i] > '7') break;
        res = (res << 3) | (oct[i] - '0');
    }
    return res;
}

void tar_init_disk(uint32_t start_lba) {
    /* Remember where the TAR image starts on disk. */
    tar_start_lba = start_lba;
    klog(LOG_INFO, "TAR", "Disk-backed TAR initialized.");
}

int tar_find_file(const char* name, uint32_t* out_lba, uint32_t* out_size) {
    /* Walk TAR headers until the requested file is found. */
    uint32_t current_lba = tar_start_lba;
    uint8_t buffer[512];
    tar_header_t* header = (tar_header_t*)buffer;

    while (1) {
        cached_read_sector(current_lba, buffer);
        if (header->name[0] == '\0') break;

        if (strcmp(header->name, name) == 0) {
            *out_lba = current_lba + 1;
            *out_size = octal_to_int(header->size);
            return 0;
        }

        uint32_t size = octal_to_int(header->size);
        current_lba += 1 + ((size + 511) / 512);
        
        /* Safety break to avoid wandering off into bad disk data. */
        if (current_lba > tar_start_lba + 20000) break; 
    }

    return -1;
}

int tar_get_file_by_index(int index, char* out_name) {
    /* Enumerate TAR entries by linear scan. */
    uint32_t current_lba = tar_start_lba;
    uint8_t buffer[512];
    tar_header_t* header = (tar_header_t*)buffer;
    int current_idx = 0;

    while (1) {
        cached_read_sector(current_lba, buffer);
        if (header->name[0] == '\0') break;

        if (current_idx == index) {
            strcpy(out_name, header->name);
            return 0;
        }

        uint32_t size = octal_to_int(header->size);
        current_lba += 1 + ((size + 511) / 512);
        current_idx++;
        
        if (current_lba > tar_start_lba + 20000) break; 
    }

    return -1;
}
