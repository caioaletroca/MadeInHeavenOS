#ifndef _FS_EXT2_DISK_FORMAT_H_
#define _FS_EXT2_DISK_FORMAT_H_

#include <stddef.h>
#include <stdint.h>

/*
 * ext2's on-disk structures, read in place from block cache buffers. ext2 is
 * little-endian, like x86: fields are used as they are. A big-endian arch
 * would need le16/le32 conversions here (Linux's __le32 + le32_to_cpu).
 */

#define EXT2_SUPERBLOCK_OFFSET 1024 // bytes from the start of the disk, whatever the block size
#define EXT2_MAGIC 0xEF53           // ext2 magic number
#define EXT2_VALID_FS 0x0001        // filesystem is clean
#define EXT2_ERROR_FS 0x0002        // errors were detected: fsck needed
#define EXT2_ROOT_INO 2             // inode 1 holds bad blocks; the root directory is always 2
#define EXT2_GOOD_OLD_REV 0         // revision 0: fixed 128-byte inodes, no feature flags
#define EXT2_DYNAMIC_REV 1          // revision 1: s_inode_size and the feature flags are valid
#define EXT2_GOOD_OLD_INODE_SIZE 128
#define EXT2_INODE_EXTRA_ISIZE 128 // large inodes: a uint16_t i_extra_isize, the size of the valid extra area after it

#define EXT2_DIRECT_BLOCKS 12   // i_block[0..11]: direct
#define EXT2_SINGLE_INDIRECT 12 // i_block[12]: one level of indirection
#define EXT2_DOUBLE_INDIRECT 13 // i_block[13]: two
#define EXT2_TRIPLE_INDIRECT 14 // i_block[14]: three
#define EXT2_BLOCK_POINTERS 15

// Incompatible features: a driver must refuse a disk with one it does not know
#define EXT2_FEATURE_INCOMPAT_FILETYPE 0x0002 // directory entries carry the file type

// Read-only compatible features: an unknown one still allows reading, never writing
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001 // superblock copies only in groups 0, 1 and powers of 3, 5 and 7
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE 0x0002   // i_size_high holds a file's size above 4 GiB
#define EXT2_FEATURE_RO_COMPAT_KNOWN (EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER | EXT2_FEATURE_RO_COMPAT_LARGE_FILE)

// i_mode: the type in the top 4 bits (the S_IF* values), permissions in the low 12
#define EXT2_S_IFMT 0xF000
#define EXT2_S_IFDIR 0x4000
#define EXT2_S_IFREG 0x8000

// i_flags: inode flags (EXT2_INDEX_FL for hashed directories)
#define EXT2_INDEX_FL 0x1000 // a hashed (htree) directory: its blocks carry an index we cannot update, so never changed here

// Directory entry file_type (with FILETYPE)
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR 2

// A directory record's size: the 8-byte header and the name, rounded up to 4
#define EXT2_DIRENT_SIZE(name_len) ((sizeof(ext2_dirent_t) + (name_len) + 3) & ~(size_t)3)

typedef struct ext2_super
{
    uint32_t s_inodes_count;      // 0
    uint32_t s_blocks_count;      // 4
    uint32_t s_r_blocks_count;    // 8
    uint32_t s_free_blocks_count; // 12
    uint32_t s_free_inodes_count; // 16
    uint32_t s_first_data_block;  // 20: 1 with 1 KiB blocks (block 0 is the boot block), else 0
    uint32_t s_log_block_size;    // 24: block size = 1024 << this
    uint32_t s_log_frag_size;     // 28
    uint32_t s_blocks_per_group;  // 32
    uint32_t s_frags_per_group;   // 36
    uint32_t s_inodes_per_group;  // 40
    uint32_t s_mtime;             // 44
    uint32_t s_wtime;             // 48
    uint16_t s_mnt_count;         // 52
    uint16_t s_max_mnt_count;     // 54
    uint16_t s_magic;             // 56: EXT2_MAGIC
    uint16_t s_state;             // 58
    uint16_t s_errors;            // 60
    uint16_t s_minor_rev_level;   // 62
    uint32_t s_lastcheck;         // 64
    uint32_t s_checkinterval;     // 68
    uint32_t s_creator_os;        // 72
    uint32_t s_rev_level;         // 76
    uint16_t s_def_resuid;        // 80
    uint16_t s_def_resgid;        // 82
    // EXT2_DYNAMIC_REV only
    uint32_t s_first_ino;         // 84
    uint16_t s_inode_size;        // 88
    uint16_t s_block_group_nr;    // 90
    uint32_t s_feature_compat;    // 92: unknown ones are ignored
    uint32_t s_feature_incompat;  // 96: unknown ones refuse the mount
    uint32_t s_feature_ro_compat; // 100: unknown ones only forbid writing
    uint8_t s_uuid[16];           // 104
    char s_volume_name[16];       // 120
    char s_last_mounted[64];      // 136
    uint32_t s_algo_bitmap;       // 200
    uint8_t s_unused1[144];       // 204..347: preallocation, journal, htree seeds, 64-bit counts (unused here)
    uint16_t s_min_extra_isize;   // 348: every inode has at least this much of its extra area valid
    uint16_t s_want_extra_isize;  // 350: what new inodes should have
    uint8_t s_unused2[672];       // 352..1023
} ext2_super_t;

typedef struct ext2_group
{
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table; // first block of this group's part of the inode table
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint8_t bg_reserved[12];
} ext2_group_t;

typedef struct ext2_inode
{
    uint16_t i_mode;                       // 0: EXT2_S_IF* | permissions
    uint16_t i_uid;                        // 2
    uint32_t i_size;                       // 4: low 32 bits of the size
    uint32_t i_atime;                      // 8
    uint32_t i_ctime;                      // 12
    uint32_t i_mtime;                      // 16
    uint32_t i_dtime;                      // 20
    uint16_t i_gid;                        // 24
    uint16_t i_links_count;                // 26
    uint32_t i_blocks;                     // 28: 512-byte units, indirect blocks included
    uint32_t i_flags;                      // 32
    uint32_t i_osd1;                       // 36
    uint32_t i_block[EXT2_BLOCK_POINTERS]; // 40: 12 direct, then single, double, triple indirect; 0 = hole
    uint32_t i_generation;                 // 100
    uint32_t i_file_acl;                   // 104
    uint32_t i_size_high;                  // 108: high 32 bits of a regular file's size (large_file)
    uint32_t i_faddr;                      // 112
    uint8_t i_osd2[12];                    // 116
} ext2_inode_t;                            // 128: revision 1 inodes may be bigger; the rest is ignored

typedef struct ext2_dirent
{
    uint32_t inode;   // 0: an unused (deleted) entry
    uint16_t rec_len; // bytes to the next entry, padding included; never crosses a block
    uint8_t name_len;
    uint8_t file_type; // EXT2_FT_*
    char name[];       // name_len bytes, not '\0'-terminated
} ext2_dirent_t;

_Static_assert(sizeof(ext2_super_t) == 1024, "ext2 superblock size");
_Static_assert(offsetof(ext2_super_t, s_magic) == 56, "ext2 s_magic offset");
_Static_assert(offsetof(ext2_super_t, s_volume_name) == 120, "ext2 s_volume_name offset");
_Static_assert(offsetof(ext2_super_t, s_first_ino) == 84, "ext2 s_first_ino offset");
_Static_assert(offsetof(ext2_super_t, s_want_extra_isize) == 350, "ext2 s_want_extra_isize offset");
_Static_assert(sizeof(ext2_group_t) == 32, "ext2 group descriptor size");
_Static_assert(sizeof(ext2_inode_t) == 128, "ext2 inode size");
_Static_assert(offsetof(ext2_inode_t, i_block) == 40, "ext2 i_block offset");
_Static_assert(offsetof(ext2_dirent_t, name) == 8, "ext2 directory entry header");

#endif /* _FS_EXT2_DISK_FORMAT_H_ */