#include <mkdisk/mkdisk.h>
#include <mkdisk/ext2_defs.h>
#include <mkdisk/mbr_defs.h>
#include <mkdisk/vhd_defs.h>
#include <common/console.h>
#include <common/byteswap.h>
#include <common/file.h>
#include <string.h>

typedef struct stream_t {
    lua_State* L;
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
    size_t blockgroup_no = EXT2_BLOCKGROUP_FOR_INODE(ext2_superblock, inode_no);
    ext2_blockgroupdesc* blockgroup = &g_blockgroups[blockgroup_no];

    const ext2_inode* inode_table = (const ext2_inode*)(g_disk + blockgroup->inode_table_blockno * g_blocksize);
    return &inode_table[EXT2_INODE_INDEX(ext2_superblock, inode_no)];
}

static void begin(lua_State* L, stream_t* stream, const ext2_inode* inode)
{
    stream->L = L;
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

        luaL_error(stream->L, "triply indirect blocks are not supported!");
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

static bool write_meta(lua_State* L, const char* path, const ext2_inode* inode)
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

    return File_MaybeOverwrite(L, path, buf, strlen(buf));
}

static bool dump_file(lua_State* L, const char* name, const ext2_inode* inode)
{
    bool written = false;

    char path[1025];
    sprintf(path, "%s/%s", g_dst_dir, name);

    int type = (inode->type_and_perm & EXT2_TYPE_MASK);
    if (type == EXT2_TYPE_SYMLINK && inode->file_size <= EXT2_SMALL_SYMLINK_LEN) {
        written = File_MaybeOverwrite(L, path, inode->block_pointers, inode->file_size);
    } else if (type == EXT2_TYPE_CHAR_DEV || type == EXT2_TYPE_BLOCK_DEV) {
        int major = (inode->block_pointers[0] >> 16) & 0xffff;
        int minor = inode->block_pointers[0] & 0xffff;
        char buf[32];
        sprintf(buf, "%d:%d\n", major, minor);
        written = File_MaybeOverwrite(L, path, buf, strlen(buf));
    } else {
        uint8_t* ptr = (uint8_t*)lua_newuserdatauv(L, inode->file_size, 0);
        uint8_t* dst = ptr;
        stream_t stream;
        begin(L, &stream, inode);
        while (read_byte(&stream, dst))
            ++dst;
        written = File_MaybeOverwrite(L, path, ptr, inode->file_size);
        lua_pop(L, 1);
    }

    strcat(path, "[meta]");
    written = write_meta(L, path, inode) || written;

    return written;
}

typedef struct pending_dir_t {
    struct pending_dir_t* next;
    const ext2_inode* inode;
    char* entry_name;
} pending_dir_t;

static bool lf_pending = false;

static void dump_directory(lua_State* L, const char* name, const ext2_inode* inode)
{
    const char* path;
    stream_t stream;
    char entry_name[1024];
    int index = 0;

    int n = lua_gettop(L);

    pending_dir_t* first = NULL;
    pending_dir_t* last = NULL;

    const char* old_dst_dir = g_dst_dir;
    if (strcmp(name, "<root directory>") != 0) {
        path = lua_pushfstring(L, "%s/%s", g_dst_dir, name);
        File_TryCreateDirectory(L, path);
        g_dst_dir = path;
    } else {
        File_TryCreateDirectory(L, g_dst_dir);
        path = lua_pushfstring(L, "%s/", g_dst_dir);
    }

    sprintf(entry_name, "%s/[meta]", g_dst_dir);
    bool written = write_meta(L, entry_name, inode);

    if (!g_verbose) {
        lf_pending = true;
        Con_PrintF(L, COLOR_PROGRESS, "%c", (written ? '+' : '.'));
    } else {
        Con_PrintF(L, COLOR_STATUS, "\n%s\n", path + g_dst_dir_prefix_len);
        Con_PrintSeparator(L);
    }

    begin(L, &stream, inode);
    for (;; ++index) {
        if (stream.bytes_left == 0)
            break;

        ext2_direntry de;
        if (!read_bytes(&stream, &de, sizeof(de)))
            luaL_error(L, "%s: error reading header of entry %d.", name, index);

        if (de.entry_size < sizeof(de) + de.name_length)
            luaL_error(L, "%s: invalid entry %d size.", name, index);

        if (!read_bytes(&stream, entry_name, de.name_length))
            luaL_error(L, "%s: error reading name of entry %d.", name, index);

        entry_name[de.name_length] = 0;

        if (!skip(&stream, de.entry_size - sizeof(de) - de.name_length))
            luaL_error(L, "%s: error reading entry %d.", name, index);

        if (!strcmp(entry_name, ".") || !strcmp(entry_name, ".."))
            continue;

        const ext2_inode* inode = NULL;
        if (g_verbose) {
            char buf[1036];
            sprintf(buf, "%3d| %-25s", index, entry_name);
            Con_Print(L, COLOR_STATUS, buf);
        }

        if (de.inode != 0) {
            inode = get_inode(de.inode);
            switch (inode->type_and_perm & EXT2_TYPE_MASK) {
                case EXT2_TYPE_DIRECTORY: if (g_verbose) Con_Print(L, COLOR_STATUS, " [dir ]"); break;
                case EXT2_TYPE_FIFO: if (g_verbose) Con_Print(L, COLOR_STATUS, " [fifo]"); break;
                case EXT2_TYPE_CHAR_DEV: if (g_verbose) Con_Print(L, COLOR_STATUS, " [char]"); break;
                case EXT2_TYPE_BLOCK_DEV: if (g_verbose) Con_Print(L, COLOR_STATUS, " [blk ]"); break;
                case EXT2_TYPE_FILE: if (g_verbose) Con_Print(L, COLOR_STATUS, " [file]"); break;
                case EXT2_TYPE_SYMLINK: if (g_verbose) Con_Print(L, COLOR_STATUS, " [link]"); break;
                case EXT2_TYPE_SOCKET: if (g_verbose) Con_Print(L, COLOR_STATUS, " [sock]"); break;
                default: luaL_error(L, "%s: invalid type for entry %d.", name, index);
            }

            if (g_verbose) {
                char buf[256];
                sprintf(buf, " %04o", (inode->type_and_perm & EXT2_PERM_MASK));
                Con_Print(L, COLOR_STATUS, buf);
                sprintf(buf, " %5d %5d", inode->user_id, inode->group_id);
                Con_Print(L, COLOR_STATUS, buf);
                sprintf(buf, " %-10lu", (unsigned long)inode->file_size);
                Con_Print(L, COLOR_STATUS, buf);
            }
        }

        if (g_verbose) {
            Con_PrintF(L, COLOR_STATUS, " inode=%I entrysize=%I namelen=%I\n",
                (lua_Integer)de.inode, (lua_Integer)de.entry_size, (lua_Integer)de.name_length);
        }

        if (inode) {
            char buf[1025];
            sprintf(buf, "%s/%s", g_dst_dir, entry_name);
            exclude_t* exclude;
            for (exclude = g_exclude; exclude; exclude = exclude->next) {
                if (!strcmp(buf + g_dst_dir_prefix_len, exclude->name))
                    break;
            }
            if (exclude)
                continue;

            if ((inode->type_and_perm & EXT2_TYPE_MASK) != EXT2_TYPE_DIRECTORY) {
                bool written = dump_file(L, entry_name, inode);
                if (!g_verbose) {
                    if (!written) {
                        lf_pending = true;
                        Con_Print(L, COLOR_PROGRESS, ".");
                    } else {
                        if (lf_pending) {
                            lf_pending = false;
                            Con_Print(L, COLOR_PROGRESS, "\n");
                        }
                        Con_PrintF(L, COLOR_STATUS, "%s\n", buf + g_dst_dir_prefix_len);
                    }
                }
            } else {
                if (!strcmp(entry_name, ".") || !strcmp(entry_name, ".."))
                    continue;

                pending_dir_t* dir = (pending_dir_t*)lua_newuserdatauv(L, sizeof(pending_dir_t), 0);
                luaL_ref(L, LUA_REGISTRYINDEX);
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
        Con_Flush(L);

    for (pending_dir_t* dir = first; dir; dir = dir->next)
        dump_directory(L, dir->entry_name, dir->inode);

    lua_settop(L, n);
    g_dst_dir = old_dst_dir;
}

static void load_img(lua_State* L, const char* img)
{
    g_disk = (uint8_t*)File_PushContents(L, img, NULL);
}

STRUCT(Vhd) {
    lua_State* L;
    uint8_t* data;
    uint32_t batSize;
    uint32_t* bat;
};

static const uint8_t* vhd_get_sector(const Vhd* vhd, size_t index)
{
    lua_State* L = vhd->L;

    size_t blockIndex = index / VHD_SECTORS_PER_BLOCK;
    size_t sectorIndex = index % VHD_SECTORS_PER_BLOCK;

    if (blockIndex >= vhd->batSize) {
        luaL_error(L, "blockIndex (%I) is out of range (%I)",
            (lua_Integer)blockIndex, (lua_Integer)vhd->batSize);
    }

    size_t offset = MSB32(vhd->bat[blockIndex]) * VHD_SECTOR_SIZE + VHD_SECTOR_SIZE /* skip bitmap */;
    return vhd->data + offset + (sectorIndex * VHD_SECTOR_SIZE);
}

static void load_vhd(lua_State* L, const char* file)
{
    Vhd vhd;
    vhd.L = L;
    vhd.data = (uint8_t*)File_PushContents(L, file, NULL);

    const vhd_footer* footer = (const vhd_footer*)vhd.data;
    const vhd_dynhdr* dynhdr = (const vhd_dynhdr*)(footer + 1);

    /*
    uint64_t vhdSize = MSB64(footer->originalSize);
    */
    vhd.batSize = MSB32(dynhdr->maxTableEntries);

    /*
    size_t batSize = sizeof(uint32_t) *
        ((vhd.batSize + BAT_ENTRIES_PER_SECTOR - 1) / BAT_ENTRIES_PER_SECTOR * BAT_ENTRIES_PER_SECTOR);
    */
    vhd.bat = (uint32_t*)(dynhdr + 1);

    const mbr* pMBR = (const mbr*)vhd_get_sector(&vhd, 0);
    size_t start = pMBR->entries[0].lba_start;
    size_t sectors = pMBR->entries[0].lba_size;

    uint8_t* dst = (uint8_t*)lua_newuserdatauv(L, sectors * VHD_SECTOR_SIZE, 0);
    g_disk = dst;
    for (size_t i = 0; i < sectors; i++) {
        memcpy(dst, vhd_get_sector(&vhd, start + i), VHD_SECTOR_SIZE);
        dst += VHD_SECTOR_SIZE;
    }

    lua_remove(L, -2);
}

static void ext2read_dump(lua_State* L, const char* dstDir, size_t dstDirLen)
{
    g_dst_dir = dstDir;
    g_dst_dir_prefix_len = dstDirLen;

    memcpy(&ext2_superblock, g_disk + EXT2_SUPERBLOCK_START_OFFSET, sizeof(ext2_superblock_t));
    if (ext2_superblock.magic != EXT2_MAGIC)
        luaL_error(L, "not ext2!");

    g_blocksize = (1024 << ext2_superblock.log2_blocksize);

    if (g_verbose) {
        Con_PrintF(L, COLOR_STATUS, "total inodes = %I\n", (lua_Integer)ext2_superblock.total_inodes);
        Con_PrintF(L, COLOR_STATUS, "total blocks = %I\n", (lua_Integer)ext2_superblock.total_blocks);
        Con_PrintF(L, COLOR_STATUS, "superuser blocks = %I\n", (lua_Integer)ext2_superblock.superuser_blocks);
        Con_PrintF(L, COLOR_STATUS, "free blocks = %I\n", (lua_Integer)ext2_superblock.free_blocks);
        Con_PrintF(L, COLOR_STATUS, "free inodes = %I\n", (lua_Integer)ext2_superblock.free_inodes);
        Con_PrintF(L, COLOR_STATUS, "starting block = %I\n", (lua_Integer)ext2_superblock.first_data_block);
        Con_PrintF(L, COLOR_STATUS, "block size = %I (1024 << %I)\n", (lua_Integer)g_blocksize, (lua_Integer)ext2_superblock.log2_blocksize);
        Con_PrintF(L, COLOR_STATUS, "fragment size = %I (1024 << %I)\n", (lua_Integer)(1024 << ext2_superblock.log2_fragmentsize), (lua_Integer)ext2_superblock.log2_fragmentsize);
        Con_PrintF(L, COLOR_STATUS, "blocks per group = %I\n", (lua_Integer)ext2_superblock.blocks_per_group);
        Con_PrintF(L, COLOR_STATUS, "fragments per group = %I\n", (lua_Integer)ext2_superblock.fragments_per_group);
        Con_PrintF(L, COLOR_STATUS, "inodes per group = %I\n", (lua_Integer)ext2_superblock.inodes_per_group);
        Con_PrintF(L, COLOR_STATUS, "last_mount_time = %I\n", (lua_Integer)ext2_superblock.last_mount_time);
        Con_PrintF(L, COLOR_STATUS, "last_write_time = %I\n", (lua_Integer)ext2_superblock.last_write_time);
        Con_PrintF(L, COLOR_STATUS, "times_mounted = %I\n", (lua_Integer)ext2_superblock.times_mounted);
        Con_PrintF(L, COLOR_STATUS, "mounts_allowed = %I\n", (lua_Integer)ext2_superblock.mounts_allowed);
        Con_PrintF(L, COLOR_STATUS, "state = %I\n", (lua_Integer)ext2_superblock.state);
        Con_PrintF(L, COLOR_STATUS, "error_action = %I\n", (lua_Integer)ext2_superblock.error_action);
        Con_PrintF(L, COLOR_STATUS, "version_minor = %I\n", (lua_Integer)ext2_superblock.version_minor);
        Con_PrintF(L, COLOR_STATUS, "last_check_time = %I\n", (lua_Integer)ext2_superblock.last_check_time);
        Con_PrintF(L, COLOR_STATUS, "interval_between_checks = %I\n", (lua_Integer)ext2_superblock.interval_between_checks);
        Con_PrintF(L, COLOR_STATUS, "creator_os = %I\n", (lua_Integer)ext2_superblock.creator_os);
        Con_PrintF(L, COLOR_STATUS, "version_major = %I\n", (lua_Integer)ext2_superblock.version_major);
        Con_PrintF(L, COLOR_STATUS, "root_user_id = %I\n", (lua_Integer)ext2_superblock.root_user_id);
        Con_PrintF(L, COLOR_STATUS, "root_group_id = %I\n", (lua_Integer)ext2_superblock.root_group_id);
        Con_PrintSeparator(L);
    }

    uint32_t a = (ext2_superblock.total_blocks + ext2_superblock.blocks_per_group - 1) / ext2_superblock.blocks_per_group;
    uint32_t b = (ext2_superblock.total_inodes + ext2_superblock.inodes_per_group - 1) / ext2_superblock.inodes_per_group;
    if (a != b)
        luaL_error(L, "block group count calculation mismatch! (%I != %I)", (lua_Integer)a, (lua_Integer)b);

    g_block_group_count = a;
    if (g_verbose)
        Con_PrintF(L, COLOR_STATUS, "block group count = %I\n", (lua_Integer)g_block_group_count);

    size_t inode_table_blocks = (ext2_superblock.inodes_per_group * sizeof(ext2_inode) + EXT2_BLOCKSIZE - 1) / EXT2_BLOCKSIZE;

    g_blockgroups = (ext2_blockgroupdesc*)(g_disk + EXT2_GROUP_TABLE_START_OFFSET);

    if (g_verbose) {
        for (size_t i = 0; i < g_block_group_count; i++) {
            ext2_blockgroupdesc* p = &g_blockgroups[i];
            Con_PrintF(L, COLOR_STATUS,
                "block_use_bmp=%I inode_use_bmp=%I inode_table=%I..%I free_blocks=%I free_inodes=%I dirs=%I\n",
                (lua_Integer)p->block_usage_bitmap_blockno,
                (lua_Integer)p->inode_usage_bitmap_blockno,
                (lua_Integer)p->inode_table_blockno,
                (lua_Integer)(p->inode_table_blockno + inode_table_blocks - 1),
                (lua_Integer)p->free_blocks,
                (lua_Integer)p->free_inodes,
                (lua_Integer)p->num_directories
            );
            lua_Integer used = 0, free = 0;
            for (size_t j = 0; j < ext2_superblock.blocks_per_group; j++) {
                uint8_t* block_usage_bitmap = (uint8_t*)(g_disk + p->block_usage_bitmap_blockno * g_blocksize);
                if ((block_usage_bitmap[j >> 3] & (1 << (j & 7))) != 0) {
                    //printf("%d ", (int)j);
                    ++used;
                } else {
                    ++free;
                }
            }
            Con_PrintF(L, COLOR_STATUS, "   bitmap: used=%I free=%I\n", used, free);
        }
        Con_PrintSeparator(L);
    }

    luaL_checkstack(L, 8192, NULL);
    dump_directory(L, "<root directory>", get_inode(EXT2_ROOT_DIR_INODE));
}

static int ext2read_exclude(lua_State* L)
{
    int top = lua_gettop(L);
    for (int i = 1; i <= top; i++) {
        const char* file = luaL_checkstring(L, i);

        exclude_t* e = (exclude_t*)lua_newuserdatauv(L, sizeof(exclude_t), 0);
        luaL_ref(L, LUA_REGISTRYINDEX);
        e->next = g_exclude;
        e->name = file;
        g_exclude = e;
    }

    return 0;
}

static int ext2read_set_verbose(lua_State* L)
{
    DONT_WARN_UNUSED(L);
    g_verbose = true;
    return 0;
}

static int ext2read_dump_img(lua_State* L)
{
    size_t dstDirLen;
    const char* file = luaL_checkstring(L, 1);
    const char* dstDir = luaL_checklstring(L, 2, &dstDirLen);

    load_img(L, file);
    ext2read_dump(L, dstDir, dstDirLen);

    return 0;
}

static int ext2read_dump_vhd(lua_State* L)
{
    size_t dstDirLen;
    const char* file = luaL_checkstring(L, 1);
    const char* dstDir = luaL_checklstring(L, 2, &dstDirLen);

    load_vhd(L, file);
    ext2read_dump(L, dstDir, dstDirLen);

    return 0;
}

static const luaL_Reg funcs[] = {
    { "exclude", ext2read_exclude },
    { "set_verbose", ext2read_set_verbose },
    { "dump_img", ext2read_dump_img },
    { "dump_vhd", ext2read_dump_vhd },
    { NULL, NULL }
};

static int luaopen_ext2read(lua_State* L)
{
    luaL_newlib(L, funcs);
    return 1;
}

void Ext2Read_InitLua(lua_State* L)
{
    luaL_requiref(L, "ext2read", luaopen_ext2read, 1);
    lua_pop(L, 1);
}
