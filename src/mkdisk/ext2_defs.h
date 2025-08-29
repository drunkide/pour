#ifndef MKDISK_EXT2_DEFS_H
#define MKDISK_EXT2_DEFS_H

#include <common/common.h>

#define EXT2_MAGIC 0xEF53

#define EXT2_ROOT_DIR_INODE 2
#define EXT2_RESERVED_INODES 10

#define EXT2_SUPERBLOCK_START_OFFSET 1024
#define EXT2_SUPERBLOCK_SIZE 1024

#define EXT2_GROUP_TABLE_START_OFFSET (EXT2_SUPERBLOCK_START_OFFSET + EXT2_SUPERBLOCK_SIZE)

#define EXT2_SMALL_SYMLINK_LEN 60

/* ext2_superblock.state */
#define EXT2_STATE_CLEAN 1
#define EXT2_STATE_HAS_ERRORS 2

/* ext2_superblock.error_action */
#define EXT2_ERROR_IGNORE 1
#define EXT2_ERROR_READONLY 2
#define EXT2_ERROR_KERNEL_PANIC 3

/* ext2_superblock.creator_os */
#define EXT2_OS_LINUX 0
#define EXT2_OS_HURD 1
#define EXT2_OS_MASIX 2
#define EXT2_OS_FREEBSD 3
#define EXT2_OS_OTHER 4

/* ext2_inode.type_and_perm */
#define EXT2_TYPE_MASK 0xF000
#define EXT2_PERM_MASK 0x07FF
#define EXT2_STICKYBIT 0x0200
#define EXT2_SET_GID   0x0400
#define EXT2_SET_UID   0x0800
enum {
    EXT2_TYPE_FIFO      = 0x1000,
    EXT2_TYPE_CHAR_DEV  = 0x2000,
    EXT2_TYPE_DIRECTORY = 0x4000,
    EXT2_TYPE_BLOCK_DEV = 0x6000,
    EXT2_TYPE_FILE      = 0x8000,
    EXT2_TYPE_SYMLINK   = 0xA000,
    EXT2_TYPE_SOCKET    = 0xC000
};

/* ext2_inode.flags */
#define EXT2_SECURE_DELETION    0x00000001
#define EXT2_KEEP_AFTER_DELETE  0x00000002
#define EXT2_FILE_COMPRESSION   0x00000004
#define EXT2_SYNC_WRITE         0x00000008
#define EXT2_IMMUTABLE          0x00000010
#define EXT2_APPEND_ONLY        0x00000020
#define EXT2_EXCLUDE_FROM_DUMP  0x00000040
#define EXT2_DONT_UPDATE_TIME   0x00000080
#define EXT2_HASH_INDEXED_DIR   0x00010000
#define EXT2_AFS_DIR            0x00020000
#define EXT2_JOURNAL_FILE_DATA  0x00040000

#define EXT2_NUM_DIRECT_BLOCKS 12

#define EXT2_LOG2_BLOCKSIZE 0
#define EXT2_BLOCKSIZE (1024 << EXT2_LOG2_BLOCKSIZE)
#define EXT2_BLOCKS_PER_GROUP 8192

#pragma pack(push, 1)

STRUCT(ext2_superblock_t) {
    uint32_t total_inodes;
    uint32_t total_blocks;
    uint32_t superuser_blocks;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint32_t first_data_block;
    uint32_t log2_blocksize; /* log2(block size) - 10 */
    uint32_t log2_fragmentsize; /* log2(fragment size) - 10 */
    uint32_t blocks_per_group;
    uint32_t fragments_per_group;
    uint32_t inodes_per_group;
    uint32_t last_mount_time;
    uint32_t last_write_time;
    uint16_t times_mounted;
    uint16_t mounts_allowed;
    uint16_t magic;
    uint16_t state;
    uint16_t error_action;
    uint16_t version_minor;
    uint32_t last_check_time;
    uint32_t interval_between_checks;
    uint32_t creator_os;
    uint32_t version_major;
    uint16_t root_user_id;
    uint16_t root_group_id;
};

STRUCT(ext2_blockgroupdesc) {
    uint32_t block_usage_bitmap_blockno;
    uint32_t inode_usage_bitmap_blockno;
    uint32_t inode_table_blockno;
    uint16_t free_blocks;
    uint16_t free_inodes;
    uint16_t num_directories;
    uint8_t unused[14];
};

STRUCT(ext2_inode) {
    uint16_t type_and_perm;
    uint16_t user_id;
    uint32_t file_size;
    uint32_t last_access_time;
    uint32_t creation_time;
    uint32_t last_modify_time;
    uint32_t deletion_time;
    uint16_t group_id;
    uint16_t num_hard_links;
    uint32_t disk_sectors;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t block_pointers[EXT2_NUM_DIRECT_BLOCKS];
    uint32_t indirect_singly;
    uint32_t indirect_doubly;
    uint32_t indirect_triply;
    uint32_t generation_number;
    uint32_t reserved2[2];
    uint32_t fragment_block;
    uint32_t reserved3[3];
};

STRUCT(ext2_direntry) {
    uint32_t inode;
    uint16_t entry_size;
    uint16_t name_length;
};

#pragma pack(pop)

#define EXT2_BLOCKGROUP_FOR_INODE(superblock, inode) (((inode) - 1) / ((superblock).inodes_per_group))
#define EXT2_INODE_INDEX(superblock, inode) (((inode) - 1) % ((superblock).inodes_per_group))

#endif
