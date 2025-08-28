#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <io.h>
#include "byteswap.h"
#include "ext2_defs.h"
#include "mbr_defs.h"
#include "vhd_defs.h"

typedef struct stream_t {
    const ext2_inode* inode;
    size_t bytes_left;
    size_t current_direct_block;
    size_t singly_blocks_left;
    size_t doubly_blocks_left;
    const uint32_t* singly_blocks;
    const uint32_t* doubly_blocks;
    const uint8_t* current_block;
    size_t current_block_bytes_left;
} stream_t;

typedef struct exclude_t {
    struct exclude_t* next;
    const char* name;
} exclude_t;

static exclude_t* g_exclude;
static const char* g_dst_dir;
static size_t g_dst_dir_prefix_len;
static uint8_t* g_disk;
static ext2_superblock_t ext2_superblock;
static uint32_t g_blocksize;
static ext2_blockgroupdesc* g_blockgroups;
static uint32_t g_block_group_count;
static bool g_verbose;

static const ext2_inode* get_inode(size_t inode_no)
{
    size_t blockgroup_no = EXT2_BLOCKGROUP_FOR_INODE(inode_no);
    ext2_blockgroupdesc* blockgroup = &g_blockgroups[blockgroup_no];

    const ext2_inode* inode_table = (const ext2_inode*)(g_disk + blockgroup->inode_table_blockno * g_blocksize);
    return &inode_table[EXT2_INODE_INDEX(inode_no)];
}

static void begin(stream_t* stream, const ext2_inode* inode)
{
    stream->inode = inode;
    stream->bytes_left = stream->inode->file_size;
    stream->current_direct_block = 0;
    stream->current_block = (g_disk + stream->inode->block_pointers[0] * g_blocksize);
    stream->current_block_bytes_left = g_blocksize;
    stream->singly_blocks = NULL;
    stream->doubly_blocks = NULL;
}

static int read_byte(stream_t* stream, uint8_t* out)
{
    if (stream->bytes_left == 0)
        return 0;


    if (stream->current_block_bytes_left == 0) {
        if (stream->current_direct_block < EXT2_NUM_DIRECT_BLOCKS - 1) {
            ++stream->current_direct_block;
            stream->current_block = (g_disk + stream->inode->block_pointers[stream->current_direct_block] * g_blocksize);
            stream->current_block_bytes_left = g_blocksize;
            goto perform;
        }

        if (!stream->singly_blocks && !stream->doubly_blocks) {
            stream->singly_blocks = (const uint32_t*)(g_disk + stream->inode->indirect_singly * g_blocksize);
            stream->singly_blocks_left = g_blocksize / sizeof(uint32_t);
        }
        if (stream->singly_blocks_left > 0) {
          singly:
            stream->current_block = (g_disk + *stream->singly_blocks * g_blocksize);
            stream->current_block_bytes_left = g_blocksize;
            ++stream->singly_blocks;
            --stream->singly_blocks_left;
            goto perform;
        }

        if (!stream->doubly_blocks) {
            stream->doubly_blocks = (const uint32_t*)(g_disk + stream->inode->indirect_doubly * g_blocksize);
            stream->doubly_blocks_left = g_blocksize / sizeof(uint32_t);
        }
        if (stream->doubly_blocks_left > 0) {
            stream->singly_blocks = (const uint32_t*)(g_disk + *stream->doubly_blocks * g_blocksize);
            stream->singly_blocks_left = g_blocksize / sizeof(uint32_t);
            ++stream->doubly_blocks;
            --stream->doubly_blocks_left;
            goto singly;
        }

        fprintf(stderr, "triply indirect blocks are not supported!\n");
        exit(1);
    }

  perform:
    --stream->bytes_left;
    --stream->current_block_bytes_left;
    *out = *stream->current_block++;
    return 1;
}

static int read_bytes(stream_t* stream, void* buf, size_t bytes)
{
    uint8_t* dst = (uint8_t*)buf;
    while (bytes != 0) {
        if (!read_byte(stream, dst))
            return 0;
        ++dst;
        --bytes;
    }
    return 1;
}

static int skip(stream_t* stream, size_t bytes)
{
    while (bytes != 0) {
        uint8_t tmp;
        if (!read_byte(stream, &tmp))
            return 0;
        --bytes;
    }
    return 1;
}

static void write_file(const char* path, const void* data, size_t size)
{
    FILE* f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "can't create file %s: %s\n", path, strerror(errno));
        exit(1);
    }

    size_t bytesWritten = fwrite(data, 1, size, f);
    if (ferror(f) || bytesWritten != size) {
        fprintf(stderr, "can't write file %s: %s\n", path, strerror(errno));
        fclose(f);
        remove(path);
        exit(1);
    }

    fclose(f);
}

static bool write_file_if_unchanged(const char* path, const void* data, size_t size)
{
    FILE* f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long oldSize = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (!ferror(f) && oldSize == size) {
            char* tmp = malloc(size);
            size_t bytesRead = fread(tmp, 1, size, f);
            if (bytesRead == size && memcmp(data, tmp, size) == 0) {
                free(tmp);
                fclose(f);
                return false;
            }

            free(tmp);
        }

        fclose(f);
    }

    write_file(path, data, size);
    return true;
}

static bool write_meta(const char* path, const ext2_inode* inode)
{
    const char* types = NULL;
    switch (inode->type_and_perm & EXT2_TYPE_MASK) {
        case EXT2_TYPE_DIRECTORY: types = "D"; break;
        case EXT2_TYPE_FIFO: types = "Q"; break;
        case EXT2_TYPE_CHAR_DEV: types = "C"; break;
        case EXT2_TYPE_BLOCK_DEV: types = "B"; break;
        case EXT2_TYPE_FILE: types = "F"; break;
        case EXT2_TYPE_SYMLINK: types = "L"; break;
        case EXT2_TYPE_SOCKET: types = "S"; break;
    }

    char buf[64];
    sprintf(buf, "%s %04o %d:%d\n",
        types, inode->type_and_perm & EXT2_PERM_MASK, inode->user_id, inode->group_id);

    return write_file_if_unchanged(path, buf, strlen(buf));
}

static bool dump_file(const char* name, const ext2_inode* inode)
{
    bool written = false;

    char path[1024];
    sprintf(path, "%s/%s", g_dst_dir, name);

    int type = (inode->type_and_perm & EXT2_TYPE_MASK);
    if (type == EXT2_TYPE_SYMLINK && inode->file_size <= EXT2_SMALL_SYMLINK_LEN) {
        written = write_file_if_unchanged(path, inode->block_pointers, inode->file_size);
    } else if (type == EXT2_TYPE_CHAR_DEV || type == EXT2_TYPE_BLOCK_DEV) {
        int major = (inode->block_pointers[0] >> 16) & 0xffff;
        int minor = inode->block_pointers[0] & 0xffff;
        char buf[32];
        sprintf(buf, "%d:%d\n", major, minor);
        written = write_file_if_unchanged(path, buf, strlen(buf));
    } else {
        uint8_t* ptr = malloc(inode->file_size);
        uint8_t* dst = ptr;
        stream_t stream;
        begin(&stream, inode);
        while (read_byte(&stream, dst))
            ++dst;
        written = write_file_if_unchanged(path, ptr, inode->file_size);
    }

    strcat(path, "[meta]");
    written = write_meta(path, inode) || written;

    return written;
}

typedef struct pending_dir_t {
    struct pending_dir_t* next;
    const ext2_inode* inode;
    char* entry_name;
} pending_dir_t;

static bool lf_pending = false;

static void dump_directory(const char* name, const ext2_inode* inode)
{
    stream_t stream;
    char path[1024];
    char entry_name[1024];
    int index = 0;

    pending_dir_t* first = NULL;
    pending_dir_t* last = NULL;

    const char* old_dst_dir = g_dst_dir;
    if (strcmp(name, "<root directory>") != 0) {
        sprintf(path, "%s/%s", g_dst_dir, name);
        mkdir(path);
        g_dst_dir = path;
    } else {
        mkdir(g_dst_dir);
        sprintf(path, "%s/", g_dst_dir);
    }

    sprintf(entry_name, "%s/[meta]", g_dst_dir);
    bool written = write_meta(entry_name, inode);

    if (!g_verbose) {
        lf_pending = true;
        printf("%c", (written ? '+' : '.'));
    } else {
        printf("\n%s\n", path + g_dst_dir_prefix_len);
        printf("---------------------------------------------------------------------------------------------------------\n");
    }

    begin(&stream, inode);
    for (;; ++index) {
        if (stream.bytes_left == 0)
            break;

        ext2_direntry de;
        if (!read_bytes(&stream, &de, sizeof(de))) {
            fprintf(stderr, "%s: error reading header of entry %d.\n", name, index);
            exit(1);
        }

        if (de.entry_size < sizeof(de) + de.name_length) {
            fprintf(stderr, "%s: invalid entry %d size.\n", name, index);
            exit(1);
        }

        if (!read_bytes(&stream, entry_name, de.name_length)) {
            fprintf(stderr, "%s: error reading name of entry %d.\n", name, index);
            exit(1);
        }

        entry_name[de.name_length] = 0;

        if (!skip(&stream, de.entry_size - sizeof(de) - de.name_length)) {
            fprintf(stderr, "%s: error reading entry %d.\n", name, index);
            exit(1);
        }

        if (!strcmp(entry_name, ".") || !strcmp(entry_name, ".."))
            continue;

        const ext2_inode* inode;
        if (g_verbose)
            printf("%3d| %-25s", index, entry_name);

        if (de.inode != 0) {
            inode = get_inode(de.inode);
            switch (inode->type_and_perm & EXT2_TYPE_MASK) {
                case EXT2_TYPE_DIRECTORY: if (g_verbose) printf(" [dir ]"); break;
                case EXT2_TYPE_FIFO: if (g_verbose) printf(" [fifo]"); break;
                case EXT2_TYPE_CHAR_DEV: if (g_verbose) printf(" [char]"); break;
                case EXT2_TYPE_BLOCK_DEV: if (g_verbose) printf(" [blk ]"); break;
                case EXT2_TYPE_FILE: if (g_verbose) printf(" [file]"); break;
                case EXT2_TYPE_SYMLINK: if (g_verbose) printf(" [link]"); break;
                case EXT2_TYPE_SOCKET: if (g_verbose) printf(" [sock]"); break;
                default:
                    fprintf(stderr, "%s: invalid type for entry %d.\n", name, index);
                    exit(1);
            }

            if (g_verbose) {
                printf(" %04o", (inode->type_and_perm & EXT2_PERM_MASK));

                printf(" %5d", inode->user_id);
                printf(" %5d", inode->group_id);

                printf(" %-10lu", (unsigned long)inode->file_size);
            }
            }

        if (g_verbose) {
            printf(" inode=%lu entrysize=%lu namelen=%lu\n",
                (unsigned long)de.inode, (unsigned long)de.entry_size, (unsigned long)de.name_length);
        }

        if (inode) {
            char buf[1024];
            sprintf(buf, "%s/%s", g_dst_dir, entry_name);
            exclude_t* exclude;
            for (exclude = g_exclude; exclude; exclude = exclude->next) {
                if (!strcmp(buf + g_dst_dir_prefix_len, exclude->name))
                    break;
            }
            if (exclude)
                continue;

            if ((inode->type_and_perm & EXT2_TYPE_MASK) != EXT2_TYPE_DIRECTORY) {
                bool written = dump_file(entry_name, inode);
                if (!g_verbose) {
                    if (!written) {
                        lf_pending = true;
                        printf(".");
                    } else {
                        if (lf_pending) {
                            lf_pending = false;
                            printf("\n");
                        }
                        printf("%s\n", buf + g_dst_dir_prefix_len);
                    }
                }
            } else {
                if (!strcmp(entry_name, ".") || !strcmp(entry_name, ".."))
                    continue;

                pending_dir_t* dir = (pending_dir_t*)malloc(sizeof(pending_dir_t));
                dir->next = NULL;
                dir->entry_name = strdup(entry_name);
                dir->inode = get_inode(de.inode);
                if (!first)
                    first = dir;
                else
                    last->next = dir;
                last = dir;
            }
        }
    }

    if (!g_verbose)
        fflush(stdout);

    for (pending_dir_t* dir = first; dir; dir = dir->next)
        dump_directory(dir->entry_name, dir->inode);

    g_dst_dir = old_dst_dir;
}

static void load_img(const char* img)
{
    FILE* f = fopen(img, "rb");
    if (!f) {
        fprintf(stderr, "unable to open file %s: %s\n", img, strerror(errno));
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (ferror(f)) {
        fprintf(stderr, "unable to load file %s: %s\n", img, strerror(errno));
        fclose(f);
        exit(1);
    }

    uint8_t* ptr = (uint8_t*)malloc(size);
    g_disk = ptr;
    size_t bytesRead = fread(ptr, 1, size, f);
    if (ferror(f) || bytesRead != size) {
        fprintf(stderr, "unable to load file %s: %s\n", img, strerror(errno));
        fclose(f);
        exit(1);
    }

    fclose(f);
}

static uint8_t* g_vhd;
static uint32_t g_vhdBatSize;
static uint32_t* g_vhdBat;

static const uint8_t* vhd_get_sector(size_t index)
{
    size_t blockIndex = index / VHD_SECTORS_PER_BLOCK;
    size_t sectorIndex = index % VHD_SECTORS_PER_BLOCK;

    if (blockIndex >= g_vhdBatSize) {
        fprintf(stderr, "blockIndex (%lu) is out of range (%lu)\n",
            (unsigned long)blockIndex, (unsigned long)g_vhdBatSize);
        exit(1);
    }

    size_t offset = MSB32(g_vhdBat[blockIndex]) * VHD_SECTOR_SIZE + VHD_SECTOR_SIZE /* skip bitmap */;
    return g_vhd + offset + (sectorIndex * VHD_SECTOR_SIZE);
}

static void load_vhd(const char* vhd)
{
    FILE* f = fopen(vhd, "rb");
    if (!f) {
        fprintf(stderr, "unable to open file %s: %s\n", vhd, strerror(errno));
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (ferror(f)) {
        fprintf(stderr, "unable to load file %s: %s\n", vhd, strerror(errno));
        fclose(f);
        exit(1);
    }

    g_vhd = (uint8_t*)malloc(size);
    size_t bytesRead = fread(g_vhd, 1, size, f);
    if (ferror(f) || bytesRead != size) {
        fprintf(stderr, "unable to load file %s: %s\n", vhd, strerror(errno));
        fclose(f);
        exit(1);
    }

    fclose(f);

    const vhd_footer* footer = (const vhd_footer*)g_vhd;
    const vhd_dynhdr* dynhdr = (const vhd_dynhdr*)(footer + 1);

    uint64_t vhdSize = MSB64(footer->originalSize);
    g_vhdBatSize = MSB32(dynhdr->maxTableEntries);

    size_t batSize = sizeof(uint32_t) *
        ((g_vhdBatSize + BAT_ENTRIES_PER_SECTOR - 1) / BAT_ENTRIES_PER_SECTOR * BAT_ENTRIES_PER_SECTOR);
    g_vhdBat = (uint32_t*)(dynhdr + 1);

    const mbr* pMBR = (const mbr*)vhd_get_sector(0);
    size_t start = pMBR->entries[0].lba_start;
    size_t sectors = pMBR->entries[0].lba_size;

    uint8_t* dst = (uint8_t*)malloc(sectors * VHD_SECTOR_SIZE);
    g_disk = dst;
    for (size_t i = 0; i < sectors; i++) {
        memcpy(dst, vhd_get_sector(start + i), VHD_SECTOR_SIZE);
        dst += VHD_SECTOR_SIZE;
    }
}

int main(int argc, char** argv)
{
    const char* img = NULL, *vhd = NULL, *out = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-img"))
            img = argv[++i];
        else if (!strcmp(argv[i], "-vhd"))
            vhd = argv[++i];
        else if (!strcmp(argv[i], "-out")) {
            g_dst_dir = argv[++i];
            g_dst_dir_prefix_len = strlen(g_dst_dir);
        } else if (!strcmp(argv[i], "-exclude")) {
            exclude_t* e = (exclude_t*)malloc(sizeof(exclude_t));
            e->next = g_exclude;
            e->name = argv[++i];
            g_exclude = e;
        } else if (!strcmp(argv[i], "-v"))
            g_verbose = true;
    }

    if (img)
        load_img(img);
    else if (vhd)
        load_vhd(vhd);
    else {
        fprintf(stderr, "missing -img or -vhd on command line.\n");
        return 1;
    }

    if (!g_dst_dir) {
        fprintf(stderr, "missing -out on command line.\n");
        return 1;
    }

    memcpy(&ext2_superblock, g_disk + EXT2_SUPERBLOCK_START_OFFSET, sizeof(ext2_superblock_t));
    if (ext2_superblock.magic != EXT2_MAGIC) {
        fprintf(stderr, "not ext2!\n");
        return 1;
    }

    g_blocksize = (1024 << ext2_superblock.log2_blocksize);

    if (g_verbose) {
        printf("total inodes = %lu\n", (unsigned long)ext2_superblock.total_inodes);
        printf("total blocks = %lu\n", (unsigned long)ext2_superblock.total_blocks);
        printf("superuser blocks = %lu\n", (unsigned long)ext2_superblock.superuser_blocks);
        printf("free blocks = %lu\n", (unsigned long)ext2_superblock.free_blocks);
        printf("free inodes = %lu\n", (unsigned long)ext2_superblock.free_inodes);
        printf("starting block = %lu\n", (unsigned long)ext2_superblock.first_data_block);
        printf("block size = %lu (1024 << %lu)\n", (unsigned long)g_blocksize, (unsigned long)ext2_superblock.log2_blocksize);
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
    }

    uint32_t a = (ext2_superblock.total_blocks + ext2_superblock.blocks_per_group - 1) / ext2_superblock.blocks_per_group;
    uint32_t b = (ext2_superblock.total_inodes + ext2_superblock.inodes_per_group - 1) / ext2_superblock.inodes_per_group;
    if (a != b) {
        printf("block group count calculation mismatch! (%lu != %lu)\n", (unsigned long)a, (unsigned long)b);
        return 1;
    }

    g_block_group_count = a;
    if (g_verbose)
        printf("block group count = %lu\n", (unsigned long)g_block_group_count);

    size_t inode_table_blocks = (ext2_superblock.inodes_per_group * sizeof(ext2_inode) + EXT2_BLOCKSIZE - 1) / EXT2_BLOCKSIZE;

    g_blockgroups = (ext2_blockgroupdesc*)(g_disk + EXT2_GROUP_TABLE_START_OFFSET);

    if (g_verbose) {
        for (size_t i = 0; i < g_block_group_count; i++) {
            ext2_blockgroupdesc* p = &g_blockgroups[i];
            printf("block_use_bmp=%lu inode_use_bmp=%lu inode_table=%lu..%lu free_blocks=%u free_inodes=%u dirs=%u\n",
                (unsigned long)p->block_usage_bitmap_blockno,
                (unsigned long)p->inode_usage_bitmap_blockno,
                (unsigned long)p->inode_table_blockno,
                (unsigned long)(p->inode_table_blockno + inode_table_blocks - 1),
                (unsigned)p->free_blocks,
                (unsigned)p->free_inodes,
                (unsigned)p->num_directories
            );
            unsigned long used = 0, free = 0;
            for (size_t j = 0; j < ext2_superblock.blocks_per_group; j++) {
                uint8_t* block_usage_bitmap = (uint8_t*)(g_disk + p->block_usage_bitmap_blockno * g_blocksize);
                if ((block_usage_bitmap[j >> 3] & (1 << (j & 7))) != 0) {
                    //printf("%d ", (int)j);
                    ++used;
                } else {
                    ++free;
                }
            }
            printf("   bitmap: used=%lu free=%lu\n", used, free);
        }
        printf("--------------------------------------\n");
    }

    dump_directory("<root directory>", get_inode(EXT2_ROOT_DIR_INODE));
}
