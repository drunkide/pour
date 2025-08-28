#ifdef NDEBUG
#undef NDEBUG
#endif
#include <common/common.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>
#include <mkdisk/mkdisk.h>
#include <mkdisk/disk_config.h>
#include <mkdisk/ext2.h>
#include <mkdisk/vhd.h>
#include <mkdisk/mbr.h>
#include <mkdisk/ext2_defs.h>
#include <mkdisk/mbr_defs.h>
#include <mkdisk/vhd_defs.h>

#define MAX_DIR_ENTRIES 16384

typedef struct direntry {
    size_t inode;
    char* name;
} direntry;

struct FSDir {
    struct FSDir* next;
    struct FSDir* parent;
    Disk* disk;
    size_t inode;
    size_t entryCount;
    direntry entries[MAX_DIR_ENTRIES];
};

time_t t;
ext2_superblock_t ext2_superblock;
ext2_blockgroupdesc* g_blockgroups;
uint8_t** g_block_usage_bitmap;
uint8_t** g_inode_usage_bitmap;
ext2_inode** g_inode_table;
size_t g_block_group_count;
size_t g_total_inode_count;
size_t g_blocks_per_group;
size_t g_inodes_per_group;
size_t g_inode_table_blocks;

static FSDir root_dir;
static FSDir* last_dir;

static size_t ext2_alloc_inode(void)
{
    for (size_t i = 0; i < g_block_group_count; i++) {
        for (size_t j = 0; j < g_inodes_per_group; j++) {
            if ((g_inode_usage_bitmap[i][j >> 3] & (1 << (j & 7))) == 0) {
                g_inode_usage_bitmap[i][j >> 3] |= (1 << (j & 7));
                if (g_blockgroups[i].free_inodes == 0) {
                    fprintf(stderr, "mismatch of free_inodes counter for block group %d.\n", (int)i);
                    exit(1);
                }
                if (ext2_superblock.free_inodes == 0) {
                    fprintf(stderr, "mismatch of ext2_superblock.free_inodes.\n");
                    exit(1);
                }
                g_blockgroups[i].free_inodes--;
                ext2_superblock.free_inodes--;
                return (i * g_inodes_per_group + j) + 1;
            }
        }

        if (g_blockgroups[i].free_inodes != 0) {
            fprintf(stderr, "free_inodes counter is not zero for block group %d.\n", (int)i);
            exit(1);
        }
    }

    fprintf(stderr, "no free inodes left.\n");
    if (ext2_superblock.free_inodes != 0)
        fprintf(stderr, "mismatch of ext2_superblock.free_inodes.\n");

    exit(1);
}

static ext2_inode* ext2_get_inode(size_t inode)
{
    if (inode == 0 || inode > g_total_inode_count) {
        fprintf(stderr, "inode %lu is out of range.\n", (unsigned long)inode);
        exit(1);
    }

    return &g_inode_table[EXT2_BLOCKGROUP_FOR_INODE(inode)][EXT2_INODE_INDEX(inode)];
}

static size_t ext2_alloc_block(bool is_directory)
{
    for (size_t i = 0; i < g_block_group_count; i++) {
        for (size_t j = 0; j < g_blocks_per_group; j++) {
            if ((g_block_usage_bitmap[i][j >> 3] & (1 << (j & 7))) == 0) {
                g_block_usage_bitmap[i][j >> 3] |= (1 << (j & 7));
                if (g_blockgroups[i].free_blocks == 0) {
                    fprintf(stderr, "mismatch of free_blocks counter for block group %d.\n", (int)i);
                    exit(1);
                }
                if (ext2_superblock.free_blocks == 0) {
                    fprintf(stderr, "mismatch of ext2_superblock.free_blocks.\n");
                    exit(1);
                }
                if (is_directory)
                    ++g_blockgroups[i].num_directories;
                g_blockgroups[i].free_blocks--;
                ext2_superblock.free_blocks--;
                return i * g_blocks_per_group + j;
            }
        }

        if (g_blockgroups[i].free_blocks != 0) {
            fprintf(stderr, "free_blocks counter is not zero for block group %d.\n", (int)i);
            exit(1);
        }
    }

    fprintf(stderr, "no free blocks left.\n");
    if (ext2_superblock.free_blocks != 0)
        fprintf(stderr, "mismatch of ext2_superblock.free_blocks.\n");

    exit(1);
}

static void Ext2_WriteBlocks(Disk* dsk, size_t blockIndex, const void* data, size_t length)
{
    const disk_config_t* disk_config = dsk->config;
    VHD_WriteSectors(dsk, MBR_DISK_START + (blockIndex * EXT2_BLOCKSIZE) / VHD_SECTOR_SIZE, data, length);
}

/********************************************************************************************************************/

typedef struct stream_t {
    Disk* disk;
    ext2_inode* inode;
    uint32_t* singly;
    uint32_t* doubly;
    uint32_t* doublyInner;
    size_t currentSize;
    size_t extraAllocatedBlocks;
    size_t currentBlock;
    size_t currentDoublyInnerBlock;
    size_t nextBlockOffset;
    size_t nextSinglyBlockOffset;
    size_t nextDoublyBlockOffset;
    size_t nextDoublyInnerBlockOffset;
    uint8_t buffer[EXT2_BLOCKSIZE];
    uint8_t* bufferPtr;
    bool isDir;
} stream_t;

static void ext2_write_current_block(stream_t* stream)
{
    if (stream->currentBlock != 0) {
        Ext2_WriteBlocks(stream->disk, stream->currentBlock, stream->buffer, stream->bufferPtr - stream->buffer);
        //printf("      write -> %lu\n", (unsigned long)stream->currentBlock);
    }
}

static void ext2_open(stream_t* stream, Disk* disk, ext2_inode* inode, bool isDir)
{
    stream->disk = disk;
    stream->inode = inode;
    stream->singly = NULL;
    stream->doubly = NULL;
    stream->doublyInner = NULL;
    stream->currentSize = 0;
    stream->extraAllocatedBlocks = 0;
    stream->currentBlock = 0;
    stream->nextBlockOffset = 0;
    stream->nextSinglyBlockOffset = 0;
    stream->nextDoublyBlockOffset = 0;
    stream->nextDoublyInnerBlockOffset = 0;
    stream->bufferPtr = stream->buffer + EXT2_BLOCKSIZE;
    stream->isDir = isDir;
}

static void ext2_close(stream_t* stream)
{
    ext2_write_current_block(stream);

    if (stream->singly) {
        Ext2_WriteBlocks(stream->disk, stream->inode->indirect_singly,
            stream->singly, stream->nextSinglyBlockOffset * sizeof(uint32_t));
        free(stream->singly);
    }

    if (stream->doublyInner) {
        size_t block = stream->doubly[stream->nextDoublyBlockOffset - 1];
        Ext2_WriteBlocks(stream->disk, block,
            stream->doublyInner, stream->nextDoublyInnerBlockOffset * sizeof(uint32_t));
        free(stream->doublyInner);
    }

    if (stream->doubly) {
        Ext2_WriteBlocks(stream->disk, stream->inode->indirect_doubly,
            stream->doubly, stream->nextDoublyBlockOffset * sizeof(uint32_t));
        free(stream->doubly);
    }

    stream->inode->file_size = stream->currentSize;
    stream->inode->disk_sectors =
        ((stream->currentSize + EXT2_BLOCKSIZE - 1) / EXT2_BLOCKSIZE
        + stream->extraAllocatedBlocks)
            * EXT2_BLOCKSIZE / VHD_SECTOR_SIZE;
}

static void ext2_stream_alloc_block(stream_t* stream)
{
    ext2_write_current_block(stream);

    stream->bufferPtr = stream->buffer;
    memset(stream->buffer, 0, sizeof(stream->buffer));

    /* direct */

    if (stream->nextBlockOffset < EXT2_NUM_DIRECT_BLOCKS) {
        stream->currentBlock = ext2_alloc_block(stream->isDir);
        //printf("direct: %d\n", (int)stream->currentBlock);
        stream->inode->block_pointers[stream->nextBlockOffset] = stream->currentBlock;
        ++stream->nextBlockOffset;
        return;
    }

    /* singly */

    if (!stream->singly) {
        ++stream->extraAllocatedBlocks;
        stream->inode->indirect_singly = ext2_alloc_block(stream->isDir);
        //printf("  singly @ %d\n", (int)stream->inode->indirect_singly);
        stream->singly = (uint32_t*)calloc(1, EXT2_BLOCKSIZE);
    }

    if (stream->nextSinglyBlockOffset < EXT2_BLOCKSIZE / sizeof(uint32_t)) {
        stream->currentBlock = ext2_alloc_block(stream->isDir);
        //printf("  singly[%d]: %d\n", (int)stream->nextSinglyBlockOffset, (int)stream->currentBlock);
        stream->singly[stream->nextSinglyBlockOffset] = stream->currentBlock;
        ++stream->nextSinglyBlockOffset;
        return;
    }

    /* doubly */

    if (!stream->doubly) {
        stream->extraAllocatedBlocks += 2;

        stream->inode->indirect_doubly = ext2_alloc_block(stream->isDir);
        //printf("  doubly @ %d\n", (int)stream->inode->indirect_doubly);
        stream->doubly = (uint32_t*)calloc(1, EXT2_BLOCKSIZE);

        size_t doublyBlock = ext2_alloc_block(stream->isDir);
        stream->doublyInner = (uint32_t*)calloc(1, EXT2_BLOCKSIZE);
        //printf("  doubly[%d] @ %d\n", (int)stream->nextDoublyBlockOffset, (int)doublyBlock);
        stream->doubly[stream->nextDoublyBlockOffset] = doublyBlock;
        ++stream->nextDoublyBlockOffset;
    }

    if (stream->nextDoublyInnerBlockOffset < EXT2_BLOCKSIZE / sizeof(uint32_t)) {
      doInner:
        stream->currentBlock = ext2_alloc_block(stream->isDir);
        //printf("    doublyInner[%d]: %d\n", (int)stream->nextDoublyInnerBlockOffset, (int)stream->currentBlock);
        stream->doublyInner[stream->nextDoublyInnerBlockOffset] = stream->currentBlock;
        ++stream->nextDoublyInnerBlockOffset;
        return;
    }

    if (stream->nextDoublyBlockOffset < EXT2_BLOCKSIZE / sizeof(uint32_t)) {
        size_t block = stream->doubly[stream->nextDoublyBlockOffset - 1];
        Ext2_WriteBlocks(stream->disk, block,
            stream->doublyInner, stream->nextDoublyInnerBlockOffset * sizeof(uint32_t));

        ++stream->extraAllocatedBlocks;

        size_t doublyBlock = ext2_alloc_block(stream->isDir);
        memset(stream->doublyInner, 0, EXT2_BLOCKSIZE);
        //printf("  doubly[%d] @ %d\n", (int)stream->nextDoublyBlockOffset, (int)doublyBlock);
        stream->doubly[stream->nextDoublyBlockOffset] = doublyBlock;
        ++stream->nextDoublyBlockOffset;

        stream->nextDoublyInnerBlockOffset = 0;
        goto doInner;
    }

    fprintf(stderr, "file is too large!\n");
    exit(1);
}

static void ext2_append(stream_t* stream, const void* data, size_t size)
{
    const uint8_t* ptr = (const uint8_t*)data;

    while (size-- != 0) {
        if (stream->bufferPtr >= stream->buffer + sizeof(stream->buffer))
            ext2_stream_alloc_block(stream);

        *stream->bufferPtr++ = *ptr++;
        ++stream->currentSize;
    }
}

/********************************************************************************************************************/

static stream_t dir_stream;
static ext2_direntry dir_entry;
static const char* dir_name;
static size_t dir_size;

static void ext2_flush_dir_entry(void)
{
    ext2_append(&dir_stream, &dir_entry, sizeof(dir_entry));
    ext2_append(&dir_stream, dir_name, dir_entry.name_length);
    assert(dir_entry.entry_size >= sizeof(dir_entry) + dir_entry.name_length);
    for (size_t i = sizeof(dir_entry) + dir_entry.name_length; i < dir_entry.entry_size; i++) {
        uint8_t tmp = 0;
        ext2_append(&dir_stream, &tmp, 1);
    }
    dir_size += dir_entry.entry_size;
}

static void Ext2_BuildDir(const FSDir* d)
{
    Disk* dsk = d->disk;

    dir_size = 0;

    dir_entry.inode = 0;
    dir_entry.name_length = 0;
    dir_entry.entry_size = sizeof(dir_entry);

    ext2_open(&dir_stream, dsk, ext2_get_inode(d->inode), true);
    for (size_t i = 0; i < d->entryCount; i++) {
        size_t nameLength = strlen(d->entries[i].name);
        size_t entrySize = sizeof(ext2_direntry) + nameLength;
        entrySize = (entrySize + 3) & ~3;

        if (entrySize > EXT2_BLOCKSIZE) {
            fprintf(stderr, "file name too long!\n");
            exit(1);
        }

        if (dir_size + dir_entry.entry_size + entrySize > EXT2_BLOCKSIZE) {
            assert(i != 0);
            size_t oldSize = dir_entry.entry_size;
            dir_entry.entry_size = EXT2_BLOCKSIZE - dir_size;
            assert(dir_entry.entry_size >= oldSize);
        }

        if (i != 0)
            ext2_flush_dir_entry();

        if (dir_size >= EXT2_BLOCKSIZE) {
            assert(dir_size == EXT2_BLOCKSIZE);
            dir_size = 0;
        }

        dir_name = d->entries[i].name;
        dir_entry.inode = d->entries[i].inode;
        dir_entry.name_length = nameLength;
        dir_entry.entry_size = entrySize;
    }

    assert(EXT2_BLOCKSIZE - dir_size >= sizeof(dir_entry));
    dir_entry.entry_size = EXT2_BLOCKSIZE - dir_size;
    ext2_flush_dir_entry();

    ext2_close(&dir_stream);

    assert(dir_size == EXT2_BLOCKSIZE);
}

static void ext2_add_direntry(FSDir* dir, const char* name, size_t inode)
{
    if (dir->entryCount >= MAX_DIR_ENTRIES) {
        fprintf(stderr, "too many directory entries!\n");
        exit(1);
    }

    size_t idx = dir->entryCount++;
    dir->entries[idx].name = strdup(name);
    dir->entries[idx].inode = inode;

    ext2_get_inode(inode)->num_hard_links++;
}

/********************************************************************************************************************/

void Ext2_Init(Disk* dsk, FSDir** outRoot)
{
    const disk_config_t* disk_config = dsk->config;

    t = time(NULL) / (10*86400) * (10*86400);

    memset(&root_dir, 0, sizeof(root_dir));
    root_dir.disk = dsk; /* FIXME */

    size_t disk_size = MBR_DISK_SIZE * VHD_SECTOR_SIZE;

    size_t block_count = disk_size / EXT2_BLOCKSIZE;
    g_blocks_per_group = EXT2_BLOCKSIZE * 8; /* bitmap = 1 block, 8 bits per byte */;
    size_t first_data_block = (EXT2_BLOCKSIZE == EXT2_SUPERBLOCK_START_OFFSET ? 1 : 0);

    size_t group_count = ((block_count - first_data_block) + (g_blocks_per_group - 1)) / g_blocks_per_group;

    const size_t inode_ratio = 8192;
    g_inodes_per_group = ((disk_size / inode_ratio) / group_count + 7) / 8 * 8;
    g_total_inode_count = g_inodes_per_group * group_count;
    g_inode_table_blocks = (g_inodes_per_group * sizeof(ext2_inode) + EXT2_BLOCKSIZE - 1) / EXT2_BLOCKSIZE;

    g_block_group_count = (block_count + g_blocks_per_group - 1) / g_blocks_per_group;
    size_t initialMetadataBlocks = (EXT2_GROUP_TABLE_START_OFFSET + g_block_group_count * sizeof(ext2_blockgroupdesc) + EXT2_BLOCKSIZE - 1) / EXT2_BLOCKSIZE;

    ext2_superblock.total_inodes = g_total_inode_count;
    ext2_superblock.total_blocks = block_count;
    ext2_superblock.superuser_blocks = 0;
    ext2_superblock.free_blocks = block_count;
    ext2_superblock.free_inodes = g_total_inode_count - EXT2_RESERVED_INODES;
    ext2_superblock.first_data_block = first_data_block;
    ext2_superblock.log2_blocksize = EXT2_LOG2_BLOCKSIZE;
    ext2_superblock.log2_fragmentsize = EXT2_LOG2_BLOCKSIZE;
    ext2_superblock.blocks_per_group = g_blocks_per_group;
    ext2_superblock.fragments_per_group = g_blocks_per_group;
    ext2_superblock.inodes_per_group = g_inodes_per_group;
    ext2_superblock.last_mount_time =  t;
    ext2_superblock.last_write_time = t;
    ext2_superblock.times_mounted = 1;
    ext2_superblock.mounts_allowed = 2000;
    ext2_superblock.magic = EXT2_MAGIC;
    ext2_superblock.state = EXT2_STATE_CLEAN;
    ext2_superblock.error_action = EXT2_ERROR_IGNORE;
    ext2_superblock.version_minor = 0;
    ext2_superblock.last_check_time = t;
    ext2_superblock.interval_between_checks = 15552000; /* 6 months */
    ext2_superblock.creator_os = EXT2_OS_LINUX;
    ext2_superblock.version_major = 0;
    ext2_superblock.root_user_id = 0;
    ext2_superblock.root_group_id = 0;

    g_blockgroups = (ext2_blockgroupdesc*)calloc(g_block_group_count, sizeof(ext2_blockgroupdesc));
    g_block_usage_bitmap = (uint8_t**)calloc(g_block_group_count, sizeof(uint8_t*));
    g_inode_usage_bitmap = (uint8_t**)calloc(g_block_group_count, sizeof(uint8_t*));
    g_inode_table = (ext2_inode**)calloc(g_block_group_count, sizeof(ext2_inode*));

    size_t overhead_per_group = 1 /* block bitmap */
                              + 1 /* inode bitmap */
                              + g_inode_table_blocks;

    ext2_superblock.free_blocks -= initialMetadataBlocks + overhead_per_group * g_block_group_count;

    for (size_t i = 0; i < g_block_group_count; i++) {
        ext2_blockgroupdesc* p = &g_blockgroups[i];
        g_block_usage_bitmap[i] = (uint8_t*)calloc(g_blocks_per_group, sizeof(uint8_t));
        g_inode_usage_bitmap[i] = (uint8_t*)calloc(g_inodes_per_group, sizeof(uint8_t));
        g_inode_table[i] = (ext2_inode*)calloc(g_inodes_per_group, sizeof(ext2_inode));

        size_t firstBlock = i * g_blocks_per_group;
        size_t numBlocks = g_blocks_per_group;
        if (i == g_block_group_count - 1) {
            numBlocks = block_count - firstBlock;
            for (size_t idx = numBlocks; idx < g_blocks_per_group; idx++)
                g_block_usage_bitmap[i][idx >> 3] |= (1 << (idx & 7));
        }

        size_t bitmapBlock = ext2_superblock.first_data_block;
        ext2_superblock.free_blocks -= bitmapBlock;

        if (i == 0) {
            bitmapBlock += initialMetadataBlocks;

            root_dir.inode = EXT2_ROOT_DIR_INODE;
            ext2_inode* inode = ext2_get_inode(root_dir.inode);

            inode->type_and_perm = EXT2_TYPE_DIRECTORY | 0755;
            inode->user_id = 0;
            inode->last_access_time = t;
            inode->creation_time = t;
            inode->last_modify_time = t;
            inode->group_id = 0;
            inode->num_hard_links = 0;

            ext2_add_direntry(&root_dir, ".", root_dir.inode);
            ext2_add_direntry(&root_dir, "..", root_dir.inode);

            for (size_t j = 0; j < EXT2_RESERVED_INODES; j++)
                g_inode_usage_bitmap[i][j >> 3] |= 1 << (j & 7);
        }

        for (size_t j = 0; j < bitmapBlock + overhead_per_group; j++)
            g_block_usage_bitmap[i][j >> 3] |= 1 << (j & 7);

        p->block_usage_bitmap_blockno = firstBlock + bitmapBlock;
        p->inode_usage_bitmap_blockno = firstBlock + bitmapBlock + 1;
        p->inode_table_blockno = firstBlock + bitmapBlock + 2;
        p->free_blocks = numBlocks - (bitmapBlock + overhead_per_group);
        p->free_inodes = g_inodes_per_group;
        p->num_directories = 0;

        if (i == 0)
            p->free_inodes -= EXT2_RESERVED_INODES;
    }

    last_dir = &root_dir;
    *outRoot = &root_dir;
}

FSDir* ext2_create_directory(FSDir* parent, const char* name, const ext2_meta* meta)
{
    FSDir* d = (FSDir*)calloc(1, sizeof(FSDir));
    if (!d) {
        fprintf(stderr, "memory allocation failed.\n");
        exit(1);
    }

    d->disk = parent->disk;

    if (parent->entryCount >= MAX_DIR_ENTRIES) {
        fprintf(stderr, "too many directory entries!\n");
        exit(1);
    }

    assert((meta->type_and_perm & EXT2_TYPE_MASK) == EXT2_TYPE_DIRECTORY);

    size_t inode = ext2_alloc_inode();
    ext2_inode* inode_ptr = ext2_get_inode(inode);
    inode_ptr->type_and_perm = meta->type_and_perm;
    inode_ptr->last_access_time = t;
    inode_ptr->creation_time = t;
    inode_ptr->last_modify_time = t;

    d->inode = inode;

    d->entryCount = 0;
    ext2_add_direntry(d, ".", inode);
    ext2_add_direntry(d, "..", parent->inode);

    d->parent = parent;
    ext2_add_direntry(d->parent, name, inode);

    last_dir->next = d;
    last_dir = d;

    return d;
}

void ext2_add_file(FSDir* parent, const char* name, const void* data, size_t size, const ext2_meta* meta)
{
    size_t inode = ext2_alloc_inode();
    ext2_inode* inode_ptr = ext2_get_inode(inode);
    inode_ptr->type_and_perm = meta->type_and_perm;
    inode_ptr->user_id = meta->uid;
    inode_ptr->group_id = meta->gid;
    inode_ptr->last_access_time = t;
    inode_ptr->creation_time = t;
    inode_ptr->last_modify_time = t;

    ext2_add_direntry(parent, name, inode);

    unsigned type = (meta->type_and_perm & EXT2_TYPE_MASK);
    assert(type != EXT2_TYPE_DIRECTORY);
    if (type == EXT2_TYPE_SYMLINK && size <= EXT2_SMALL_SYMLINK_LEN) {
        memcpy(inode_ptr->block_pointers, data, size);
        inode_ptr->file_size = size;
    } else if (type == EXT2_TYPE_CHAR_DEV || type == EXT2_TYPE_BLOCK_DEV) {
        if (size > 32) {
            fprintf(stderr, "invalid device file!\n");
            exit(1);
        }
        char buf[33];
        memcpy(buf, data, size);
        buf[size] = 0;
        unsigned major, minor;
        if (sscanf(buf, "%u:%u", &major, &minor) != 2 || major > 0xffff || minor > 0xffff) {
            fprintf(stderr, "invalid device file!\n");
            exit(1);
        }
        inode_ptr->block_pointers[0] = (major << 16) | minor;
    } else {
        stream_t stream;
        ext2_open(&stream, parent->disk, inode_ptr, false);
        ext2_append(&stream, data, size);
        ext2_close(&stream);
    }
}

void Ext2_Write(Disk* dsk)
{
    const disk_config_t* disk_config = dsk->config;

    for (FSDir* p = &root_dir; p; p = p->next)
        Ext2_BuildDir(p);

    size_t superblockSector = MBR_DISK_START + EXT2_SUPERBLOCK_START_OFFSET / VHD_SECTOR_SIZE;
    VHD_WriteSectors(dsk, superblockSector, &ext2_superblock, sizeof(ext2_superblock));

    size_t grouptableSector = MBR_DISK_START + EXT2_GROUP_TABLE_START_OFFSET / VHD_SECTOR_SIZE;
    VHD_WriteSectors(dsk, grouptableSector, g_blockgroups, sizeof(ext2_blockgroupdesc) * g_block_group_count);

    for (size_t i = 0; i < g_block_group_count; i++) {
        ext2_blockgroupdesc* p = &g_blockgroups[i];
        Ext2_WriteBlocks(dsk, p->block_usage_bitmap_blockno, g_block_usage_bitmap[i], g_blocks_per_group);
        Ext2_WriteBlocks(dsk, p->inode_usage_bitmap_blockno, g_inode_usage_bitmap[i], g_inodes_per_group);
        Ext2_WriteBlocks(dsk, p->inode_table_blockno, g_inode_table[i], g_inodes_per_group * sizeof(ext2_inode));
    }

  #if 0
    printf("total inodes = %lu\n", (unsigned long)ext2_superblock.total_inodes);
    printf("total blocks = %lu\n", (unsigned long)ext2_superblock.total_blocks);
    printf("superuser blocks = %lu\n", (unsigned long)ext2_superblock.superuser_blocks);
    printf("free blocks = %lu\n", (unsigned long)ext2_superblock.free_blocks);
    printf("free inodes = %lu\n", (unsigned long)ext2_superblock.free_inodes);
    printf("starting block = %lu\n", (unsigned long)ext2_superblock.first_data_block);
    printf("block size = %lu (1024 << %lu)\n", (unsigned long)EXT2_BLOCKSIZE, (unsigned long)ext2_superblock.log2_blocksize);
    printf("fragment size = %lu (1024 << %lu)\n", (unsigned long)(1024 << ext2_superblock.log2_fragmentsize), (unsigned long)ext2_superblock.log2_fragmentsize);
    printf("blocks per group = %lu\n", (unsigned long)ext2_superblock.blocks_per_group);
    printf("fragments per group = %lu\n", (unsigned long)ext2_superblock.fragments_per_group);
    printf("inodes per group = %lu\n", (unsigned long)ext2_superblock.inodes_per_group);
    printf("last_mount_time = %lu\n", (unsigned long)ext2_superblock.last_mount_time);
    printf("last_write_time = %lu\n", (unsigned long)ext2_superblock.last_write_time);
    printf("times_mounted = %u\n", (unsigned)ext2_superblock.times_mounted);
    printf("mounts_allowed = %u\n", (unsigned)ext2_superblock.mounts_allowed);
    printf("state = %u\n", (unsigned)ext2_superblock.state);
    printf("error_action = %u\n", (unsigned)ext2_superblock.error_action);
    printf("version_minor = %u\n", (unsigned)ext2_superblock.version_minor);
    printf("last_check_time = %lu\n", (unsigned long)ext2_superblock.last_check_time);
    printf("interval_between_checks = %lu\n", (unsigned long)ext2_superblock.interval_between_checks);
    printf("creator_os = %lu\n", (unsigned long)ext2_superblock.creator_os);
    printf("version_major = %lu\n", (unsigned long)ext2_superblock.version_major);
    printf("root_user_id = %u\n", (unsigned)ext2_superblock.root_user_id);
    printf("root_group_id = %u\n", (unsigned)ext2_superblock.root_group_id);
    printf("--------------------------------------\n");

    printf("block group count = %lu\n", (unsigned long)g_block_group_count);

    for (size_t i = 0; i < g_block_group_count; i++) {
        ext2_blockgroupdesc* p = &g_blockgroups[i];
        printf("block_use_bmp=%lu inode_use_bmp=%lu inode_table=%lu..%lu free_blocks=%u free_inodes=%u dirs=%u\n",
            (unsigned long)p->block_usage_bitmap_blockno,
            (unsigned long)p->inode_usage_bitmap_blockno,
            (unsigned long)p->inode_table_blockno,
            (unsigned long)(p->inode_table_blockno + g_inode_table_blocks - 1),
            (unsigned)p->free_blocks,
            (unsigned)p->free_inodes,
            (unsigned)p->num_directories
        );
        unsigned long used = 0, free = 0;
        for (size_t j = 0; j < g_blocks_per_group; j++) {
            if ((g_block_usage_bitmap[i][j >> 3] & (1 << (j & 7))) != 0) {
                //printf("%d ", (int)j);
                ++used;
            }
            else {
                ++free;
            }
        }
        printf("   bitmap: used=%lu free=%lu\n", used, free);
    }
    printf("--------------------------------------\n");
  #endif
}
