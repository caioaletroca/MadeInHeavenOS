#ifndef _FS_EXT2_INTERNAL_H_
#define _FS_EXT2_INTERNAL_H_

/*
 * What the files of fs/ext2/ share, and nothing outside it includes (the rest of
 * the kernel sees include/fs/ext2.h): the volume and node structs, and the
 * functions one file defines and another calls. Those are not static, so they keep
 * the ext2_ prefix; everything only one file uses stays static in it. Like C#'s
 * `internal`: visible to the whole folder, never to the rest of the kernel.
 */

#include <fs/ext2.h>
#include <fs/vfs.h>
#include <driver/block.h>
#include <driver/block_cache.h>
#include <sched/sync.h>
#include <sys/hashtable.h>
#include <power.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "disk_format.h"

#define EXT2_NODE_HASH_BITS 6 // 64 buckets for cached nodes

typedef struct ext2_volume
{
    block_device_t *disk;
    uint64_t dev;              // st_dev (vfs_dev_alloc)
    uint32_t block_size;       // 1024, 2048 or 4096
    uint32_t blocks_count;     // valid block numbers: 1 .. blocks_count - 1
    uint32_t first_data_block; // s_first_data_block: 1 with 1 KiB blocks, else 0; bit i of a group's bitmap is block first_data_block + group * blocks_per_group + i
    uint32_t blocks_per_group;
    uint32_t inodes_count;
    uint32_t inodes_per_group;
    uint32_t first_ino;        // s_first_ino: the first inode files may get (the ones below are reserved)
    uint16_t want_extra_isize; // new large inodes' i_extra_isize (0 with 128-byte inodes)
    uint32_t inode_size;       // bytes per inode in the table (128 or more)
    uint32_t groups;
    bool read_only;            // an unknown ro_compat feature: writing could damage the disk
    uint16_t state;            // s_state as it is on the disk (EXT2_VALID_FS cleared while mounted writable)
    uint32_t dtime;            // i_dtime for freed inodes: s_wtime (no clock), never below s_inodes_count
    halt_hook_t halt_hook;     // writable volumes: marks the volume clean before the power goes off
    uint64_t max_file_size;    // the largest file: what three levels of tables reach, or 2 GiB - 1 without LARGE_FILE
    ext2_group_t *group_table; // kmalloc'd copy of the group descriptors
    uint32_t free_blocks;      // s_free_blocks_count, under allocation_lock
    uint32_t free_inodes;      // s_free_inodes_count, under allocation_lock (for W6)
    mutex_t allocation_lock;   // the bitmaps, the group table's counters, free_blocks and free_inodes
    mutex_t lock;              // the node cache: nodes[] and every node's hash link
    hlist_head_t nodes[HASH_BUCKETS(EXT2_NODE_HASH_BITS)];
} ext2_volume_t;

typedef struct ext2_node
{
    vfs_node_t base; // what the VFS sees (the "base class")
    ext2_volume_t *volume;
    uint32_t ino;
    uint32_t block[EXT2_BLOCK_POINTERS]; // copy of the inode's i_block
    uint32_t blocks;                     // i_blocks: 512-byte units the file owns, indirect tables included
    uint32_t flags;                      // i_flags: inode flags (EXT2_INDEX_FL for hashed directories)
    bool free_on_release;                // its last name was removed: its blocks and inode go at the last put
    mutex_t lock;                        // block[], blocks, base.size and nlink, and a directory's records: held by reads, writes,
                                         // walks, creates and unlinks; a directory's lock is taken before its children's
    hlist_node_t cache_link;             // in volume->nodes, under volume->lock
} ext2_node_t;

#define EXT2_NODE(n) list_container(n, ext2_node_t, base)

// disk.c
int ext2_block_get(ext2_volume_t *volume, uint32_t block, block_cache_buf_t **lease, const uint8_t **data);
int ext2_block_write(ext2_volume_t *volume, uint32_t block, size_t within, const void *data, size_t count);
int ext2_block_zero(ext2_volume_t *volume, uint32_t block, size_t within, size_t count);
int ext2_disk_write(ext2_volume_t *volume, uint64_t byte, const void *data, size_t count);
int ext2_disk_zero(ext2_volume_t *volume, uint64_t byte, size_t count);

// allocation.c
int ext2_block_alloc(ext2_volume_t *volume, uint32_t goal, uint32_t *out);
int ext2_block_free(ext2_volume_t *volume, uint32_t block);
int ext2_inode_alloc(ext2_volume_t *volume, uint32_t parent, bool directory, uint32_t *out);
int ext2_inode_init(ext2_volume_t *volume, uint32_t ino, uint16_t mode, uint16_t links, uint32_t time);
int ext2_inode_free(ext2_volume_t *volume, uint32_t ino, bool directory);

// inode.c
uint64_t ext2_inode_byte(const ext2_volume_t *volume, uint32_t ino);
int ext2_inode_store(ext2_node_t *n);
int ext2_node_get(ext2_volume_t *volume, uint32_t ino, vfs_node_t *parent, vfs_node_t **out);

// file_blocks.c
int ext2_file_block(ext2_volume_t *volume, ext2_node_t *node, uint64_t logical, uint32_t *goal, bool create, uint32_t *out);
int ext2_truncate_branch(ext2_volume_t *volume, ext2_node_t *node, uint32_t *slot, int depth, uint64_t span, uint64_t from);

// directory.c
bool ext2_entry_type(uint8_t file_type, vfs_node_type_t *type);

/**
 * Called for each used entry of a directory, while its block is leased:
 * `entry` points into the cache buffer and is only valid during the call.
 *
 * @return 0 to go on, 1 to stop the walk, or -errno to fail it.
 */
typedef int (*ext2_visit_t)(const ext2_dirent_t *entry, void *context);

int ext2_dir_walk(ext2_node_t *dir, ext2_visit_t visit, void *context);
int ext2_dir_add(ext2_node_t *dir, const char *name, size_t length, uint32_t ino, uint8_t file_type);
int ext2_dir_remove(ext2_node_t *dir, const char *name, size_t length);
int ext2_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry);

/**
 * @brief Context for looking up a directory entry by name
 */
typedef struct ext2_lookup_context
{
    const char *name;
    size_t length;
    uint32_t ino;
    uint8_t file_type;
} ext2_lookup_context_t;

int ext2_lookup_visit(const ext2_dirent_t *e, void *context);
int ext2_lookup(vfs_node_t *dir, const char *name, size_t length, vfs_node_t **child);

// file_data.c
long ext2_read(vfs_node_t *file, void *buffer, size_t count, uint64_t offset);
long ext2_write(vfs_node_t *file, const void *buffer, size_t count, uint64_t offset);
int ext2_truncate(vfs_node_t *file, uint64_t size);

// namespace.c
int ext2_create(vfs_node_t *dir, const char *name, size_t length, uint32_t mode, vfs_node_t **child);
void ext2_release(vfs_node_t *node);
int ext2_unlink(vfs_node_t *dir, const char *name, size_t length);

#endif /* _FS_EXT2_INTERNAL_H_ */
