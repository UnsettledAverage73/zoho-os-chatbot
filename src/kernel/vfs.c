#include "vfs.h"
#include "tar.h"
#include "string.h"
#include "klog.h"
#include "ata.h"
#include "lock.h"

vfs_node_t* vfs_root = NULL;
static fd_t fd_table[MAX_FDS];
static spinlock_t vfs_lock;

void vfs_init() {
    spin_init(&vfs_lock);
    uint64_t flags = spin_lock_irqsave(&vfs_lock);
    for (int i = 0; i < MAX_FDS; i++) {
        fd_table[i].in_use = 0;
    }
    spin_unlock_irqrestore(&vfs_lock, flags);
    klog(LOG_INFO, "VFS", "Virtual File System initialized (Block-backed).");
}

int vfs_open(const char* path) {
    uint32_t lba;
    uint32_t size;

    if (tar_find_file(path, &lba, &size) != 0) {
        return -1;
    }

    uint64_t flags = spin_lock_irqsave(&vfs_lock);
    for (int i = 0; i < MAX_FDS; i++) {
        if (!fd_table[i].in_use) {
            fd_table[i].in_use = 1;
            fd_table[i].start_lba = lba;
            fd_table[i].size = size;
            fd_table[i].offset = 0;
            spin_unlock_irqrestore(&vfs_lock, flags);
            return i;
        }
    }
    spin_unlock_irqrestore(&vfs_lock, flags);
    return -1;
}

int vfs_read(int fd, void* buffer, uint32_t count) {
    uint32_t start_lba;
    uint32_t offset;
    uint64_t flags = spin_lock_irqsave(&vfs_lock);
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
        spin_unlock_irqrestore(&vfs_lock, flags);
        return -1;
    }

    fd_t* f = &fd_table[fd];
    if (f->offset >= f->size) {
        spin_unlock_irqrestore(&vfs_lock, flags);
        return 0;
    }
    if (count > f->size - f->offset) count = f->size - f->offset;
    start_lba = f->start_lba;
    offset = f->offset;
    f->offset += count;
    spin_unlock_irqrestore(&vfs_lock, flags);

    uint32_t bytes_read = 0;
    uint8_t sector_buf[512];

    while (bytes_read < count) {
        uint32_t lba = start_lba + (offset / 512);
        uint32_t sector_offset = offset % 512;
        uint32_t to_copy = 512 - sector_offset;
        if (to_copy > count - bytes_read) to_copy = count - bytes_read;

        if (sector_offset == 0 && to_copy == 512) {
            ata_read_sector(lba, (uint8_t*)buffer + bytes_read);
        } else {
            ata_read_sector(lba, sector_buf);
            memcpy((uint8_t*)buffer + bytes_read, sector_buf + sector_offset, to_copy);
        }

        bytes_read += to_copy;
        offset += to_copy;
    }
    return bytes_read;
}

int vfs_write(int fd, const void* buffer, uint32_t count) {
    uint32_t start_lba;
    uint32_t offset;
    uint64_t flags = spin_lock_irqsave(&vfs_lock);
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
        spin_unlock_irqrestore(&vfs_lock, flags);
        return -1;
    }

    fd_t* f = &fd_table[fd];
    if (f->offset + count > f->size) {
        count = f->size - f->offset;
    }
    start_lba = f->start_lba;
    offset = f->offset;
    f->offset += count;
    spin_unlock_irqrestore(&vfs_lock, flags);

    uint32_t bytes_written = 0;
    uint8_t sector_buf[512];

    while (bytes_written < count) {
        uint32_t lba = start_lba + (offset / 512);
        uint32_t sector_offset = offset % 512;
        uint32_t to_write = 512 - sector_offset;
        if (to_write > count - bytes_written) to_write = count - bytes_written;

        ata_read_sector(lba, sector_buf);
        memcpy(sector_buf + sector_offset, (uint8_t*)buffer + bytes_written, to_write);
        cached_write_sector(lba, sector_buf);

        bytes_written += to_write;
        offset += to_write;
    }
    return bytes_written;
}

uint32_t vfs_size(int fd) {
    uint64_t flags = spin_lock_irqsave(&vfs_lock);
    uint32_t size = (fd >= 0 && fd < MAX_FDS && fd_table[fd].in_use) ? fd_table[fd].size : 0;
    spin_unlock_irqrestore(&vfs_lock, flags);
    return size;
}

void vfs_close(int fd) {
    uint64_t flags = spin_lock_irqsave(&vfs_lock);
    if (fd >= 0 && fd < MAX_FDS) {
        fd_table[fd].in_use = 0;
    }
    spin_unlock_irqrestore(&vfs_lock, flags);
}

int vfs_readdir(int index, char* out_name) {
    // readdir uses internal TAR traversal which is currently read-only
    return tar_get_file_by_index(index, out_name);
}
