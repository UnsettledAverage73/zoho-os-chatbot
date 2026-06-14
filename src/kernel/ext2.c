#include "ext2.h"
#include "ata.h"
#include "klog.h"
#include "kmalloc.h"
#include "string.h"

static ext2_superblock_t* superblock = NULL;
static uint32_t block_size = 0;

static void read_block(uint32_t block, uint8_t* buffer) {
    uint32_t sectors_per_block = block_size / 512;
    for (uint32_t i = 0; i < sectors_per_block; i++) {
        ata_read_sector(block * sectors_per_block + i, buffer + (i * 512));
    }
}

static void read_inode(uint32_t inode_num, ext2_inode_t* inode) {
    uint32_t group = (inode_num - 1) / superblock->inodes_per_group;
    uint32_t index = (inode_num - 1) % superblock->inodes_per_group;

    // Read group descriptor
    uint32_t bgdt_block = (block_size == 1024) ? 2 : 1;
    uint8_t* bgdt_buf = kmalloc(block_size);
    read_block(bgdt_block, bgdt_buf);
    ext2_group_desc_t* bgd = (ext2_group_desc_t*)bgdt_buf + group;

    uint32_t inode_table_block = bgd->inode_table;
    uint32_t offset = index * sizeof(ext2_inode_t);
    uint32_t block = inode_table_block + (offset / block_size);
    uint32_t block_offset = offset % block_size;

    uint8_t* block_buf = kmalloc(block_size);
    read_block(block, block_buf);
    memcpy(inode, block_buf + block_offset, sizeof(ext2_inode_t));
    
    kfree(bgdt_buf);
    kfree(block_buf);
}

static uint32_t ext2_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    ext2_inode_t inode;
    read_inode(node->inode, &inode);

    if (offset >= inode.size) return 0;
    if (offset + size > inode.size) size = inode.size - offset;

    uint32_t bytes_read = 0;
    uint8_t* block_buf = kmalloc(block_size);

    while (bytes_read < size) {
        uint32_t block_idx = (offset + bytes_read) / block_size;
        uint32_t block_off = (offset + bytes_read) % block_size;
        uint32_t to_read = block_size - block_off;
        if (to_read > (size - bytes_read)) to_read = size - bytes_read;

        uint32_t physical_block = 0;
        if (block_idx < 12) {
            physical_block = inode.block[block_idx];
        } else {
            // TODO: Indirect blocks
            klog(LOG_WARN, "EXT2", "Indirect blocks not yet supported");
            break;
        }

        if (physical_block == 0) {
            memset(buffer + bytes_read, 0, to_read);
        } else {
            read_block(physical_block, block_buf);
            memcpy(buffer + bytes_read, block_buf + block_off, to_read);
        }

        bytes_read += to_read;
    }

    kfree(block_buf);
    return bytes_read;
}

static vfs_node_t* ext2_finddir(vfs_node_t* node, char* name) {
    ext2_inode_t inode;
    read_inode(node->inode, &inode);

    if (!(inode.mode & 0x4000)) return NULL; // Not a directory

    uint8_t* block_buf = kmalloc(block_size);
    for (int i = 0; i < 12; i++) {
        if (inode.block[i] == 0) continue;
        read_block(inode.block[i], block_buf);

        ext2_dir_entry_t* entry = (ext2_dir_entry_t*)block_buf;
        uint32_t offset = 0;
        while (offset < block_size) {
            char entry_name[256];
            memcpy(entry_name, entry->name, entry->name_len);
            entry_name[entry->name_len] = '\0';

            if (strcmp(entry_name, name) == 0) {
                vfs_node_t* res = kmalloc(sizeof(vfs_node_t));
                memcpy(res->name, entry_name, strlen(entry_name) + 1);
                res->inode = entry->inode;
                res->flags = (entry->file_type == 2) ? VFS_DIRECTORY : VFS_FILE;
                res->read = ext2_read;
                res->finddir = ext2_finddir;
                kfree(block_buf);
                return res;
            }

            offset += entry->rec_len;
            entry = (ext2_dir_entry_t*)((uint8_t*)entry + entry->rec_len);
        }
    }

    kfree(block_buf);
    return NULL;
}

void ext2_init() {
    klog(LOG_INFO, "EXT2", "Initializing Ext2 driver...");

    uint8_t* buffer = kmalloc(1024);
    ata_read_sector(2, buffer);
    ata_read_sector(3, buffer + 512);

    superblock = (ext2_superblock_t*)buffer;
    if (superblock->magic != EXT2_MAGIC) {
        klog(LOG_WARN, "EXT2", "Not present, falling back to TAR");
        return;
    }

    block_size = 1024 << superblock->log_block_size;
    klog(LOG_INFO, "EXT2", "Found Ext2 filesystem.");

    // Create root node
    vfs_root = kmalloc(sizeof(vfs_node_t));
    memcpy(vfs_root->name, "/", 2);
    vfs_root->flags = VFS_DIRECTORY;
    vfs_root->inode = 2; // Root inode is always 2
    vfs_root->read = ext2_read;
    vfs_root->finddir = ext2_finddir;
}
