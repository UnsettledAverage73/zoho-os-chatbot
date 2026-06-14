#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

/**
 * @file vfs.h
 * @brief Read-only block-backed VFS helpers.
 */

#define VFS_FILE 1
#define VFS_DIRECTORY 2

struct vfs_node;

typedef struct vfs_node {
    char name[128];
    uint32_t flags;
    uint32_t length;
    uint32_t inode;
    uint32_t (*read)(struct vfs_node*, uint32_t, uint32_t, uint8_t*);
    struct vfs_node* (*finddir)(struct vfs_node*, char*);
} vfs_node_t;

extern vfs_node_t* vfs_root;

#define MAX_FDS 16

typedef struct {
    uint32_t start_lba;
    uint32_t size;
    uint32_t offset;
    int in_use;
} fd_t;

/**
 * Initialize the file descriptor table and VFS state.
 */
void vfs_init();

/**
 * Open a file path from the TAR-backed filesystem.
 */
int vfs_open(const char* path);

/**
 * Read bytes from an open file descriptor.
 */
int vfs_read(int fd, void* buffer, uint32_t count);

/**
 * Write bytes to an open file descriptor.
 */
int vfs_write(int fd, const void* buffer, uint32_t count);

/**
 * Return the size of an open file descriptor.
 */
uint32_t vfs_size(int fd);

/**
 * Close an open file descriptor.
 */
void vfs_close(int fd);

/**
 * Read a directory entry name by index.
 */
int vfs_readdir(int index, char* out_name);

#endif
