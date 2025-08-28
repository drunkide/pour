#include <common/common.h>
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x502
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>
#include <lua.h>
#include <lauxlib.h>
#include <mkdisk/mkdisk.h>
#include <mkdisk/vhd.h>
#include <mkdisk/mbr.h>
#include <mkdisk/fat.h>
#include <mkdisk/ext2.h>
#include <mkdisk/ext2_defs.h>
#include <mkdisk/bootcode.h>
#include <mkdisk/disk_config.h>
#include <patch/patch.h>

#define CLASS_DIRECTORY "mkdisk.directory"
#define CLASS_DIRECTORY_TABLE "mkdisk.directory.table"

static const char lua_DIR;
static bool initialized = false;
static bool written = false;

bool g_skip_mkdisk;
bool g_use_ext2;
bool g_mbr_fat;

static void read_meta_file(uint16_t default_perm, const char* file, ext2_meta* out)
{
    out->type_and_perm = default_perm;
    out->uid = 0;
    out->gid = 0;

    FILE* f = fopen(file, "rb");
    if (f) {
        unsigned t, u, g;
        char type;
        if (fscanf(f, "%c %o %u:%u", &type, &t, &u, &g) != 4) {
            fprintf(stderr, "invalid meta file: %s\n", file);
            exit(1);
        }
        fclose(f);
        switch (type) {
            case 'D': out->type_and_perm = EXT2_TYPE_DIRECTORY; break;
            case 'Q': out->type_and_perm = EXT2_TYPE_FIFO; break;
            case 'C': out->type_and_perm = EXT2_TYPE_CHAR_DEV; break;
            case 'B': out->type_and_perm = EXT2_TYPE_BLOCK_DEV; break;
            case 'F': out->type_and_perm = EXT2_TYPE_FILE; break;
            case 'L': out->type_and_perm = EXT2_TYPE_SYMLINK; break;
            case 'S': out->type_and_perm = EXT2_TYPE_SOCKET; break;
            default:
                fprintf(stderr, "invalid meta file: %s\n", file);
                exit(1);
        }
        if (t > 0xfff || u > 0xffff || g > 0xffff) {
            fprintf(stderr, "invalid meta file: %s\n", file);
            exit(1);
        }
        out->type_and_perm |= t;
        out->uid = u;
        out->gid = g;
    }
}

lua_dir* get_directory(lua_State* L, int index)
{
    if (!lua_istable(L, index))
        return NULL;

    lua_rawgetp(L, index, &lua_DIR);
    if (lua_isnoneornil(L, -1)) {
        lua_pop(L, 1);
        return NULL;
    }

    lua_dir* ld = (lua_dir*)luaL_testudata(L, -1, CLASS_DIRECTORY);
    if (!ld) {
        lua_pop(L, 1);
        return NULL;
    }

    lua_pop(L, 1);
    return ld;
}

static lua_dir* push_directory(lua_State* L, dir* dir, int parentIndex,
    const char* fatShortName, const char* origName, const char* path)
{
    size_t pathlen = strlen(path) + 1;

    lua_newtable(L);
    luaL_getmetatable(L, CLASS_DIRECTORY_TABLE);
    lua_setmetatable(L, -2);

    lua_dir* ld = (lua_dir*)lua_newuserdatauv(L, sizeof(lua_dir) + pathlen + 1, 0);
    char* dstPath = (char*)(ld + 1);
    *dstPath = '/';
    if (*path == '/')
        ++path;
    memcpy(dstPath + 1, path, pathlen);
    ld->path = dstPath;
    ld->dir = dir;
    luaL_getmetatable(L, CLASS_DIRECTORY);
    lua_setmetatable(L, -2);

    lua_rawsetp(L, -2, &lua_DIR);

    if (!g_use_ext2) {
        lua_pushvalue(L, -1);
        lua_setfield(L, parentIndex, fatShortName);
    }

    if (g_use_ext2 || fat_enable_lfn) {
        lua_pushvalue(L, -1);
        lua_setfield(L, parentIndex, origName);
    }

    return ld;
}

static const lua_dir* make_dir(lua_State* L, const lua_dir* parentDir,
    int parentIndex, const char* dirName, const ext2_meta* meta)
{
    const lua_dir* subdir;
    char fatShortName[13];

    if (g_use_ext2 || fat_enable_lfn)
        lua_getfield(L, parentIndex, dirName);
    else {
        fat_normalize_name(fatShortName, dirName);
        lua_getfield(L, parentIndex, fatShortName);
    }

    if (!lua_isnoneornil(L, -1))
        subdir = get_directory(L, -1);
    else {
        lua_pop(L, 1); // pop nil

        lua_pushstring(L, parentDir->path);
        lua_pushstring(L, dirName);
        lua_pushliteral(L, "/");
        lua_concat(L, 3);
        const char* newPath = lua_tostring(L, -1);

        dir* dir;
        if (g_use_ext2)
            dir = ext2_create_directory(parentDir->dir, dirName, meta);
        else
            dir = fat_create_directory(parentDir->dir, dirName);

        subdir = push_directory(L, dir, parentIndex, fatShortName, dirName, newPath);

        lua_remove(L, -2);
    }

    return subdir;
}

static void add_file(lua_State* L, const lua_dir* dst_dir,
    const char* name, const char* fileName, const unsigned long long* pFileSize)
{
    char fatShortName[13];

    FILE* f = fopen(fileName, "rb");
    if (!f) {
        fprintf(stderr, "can't open \"%s\": %s\n", fileName, strerror(errno));
        exit(1);
    }

    const char* fsName;
    if (g_use_ext2)
        fsName = name;
    else {
        fat_normalize_name(fatShortName, name);
        fsName = fatShortName;
    }

    if (!pFileSize)
        printf("\n=> %s%s\n", dst_dir->path, fsName);

    PATCH* patch = patch_find(L, fsName);

    unsigned long long file_size;
    if (pFileSize)
        file_size = *pFileSize;
    else {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (ferror(f) || size < 0) {
            fprintf(stderr, "can't determine size of file \"%s\": %s\n", fileName, strerror(errno));
            fclose(f);
            exit(1);
        }
        file_size = (unsigned long long)size;
    }

    void* ptr = malloc(file_size + (patch ? patch->extraBytes : 0));
    if (!ptr) {
        fprintf(stderr, "file \"%s\" is too large.\n", fileName);
        fclose(f);
        exit(1);
    }

    errno = 0;
    size_t bytesRead = fread(ptr, 1, file_size, f);
    if (ferror(f) || bytesRead != file_size) {
        fprintf(stderr, "error reading file \"%s\": %s\n", fileName, strerror(errno));
        free(ptr);
        fclose(f);
        exit(1);
    }

    fclose(f);

    if (patch)
        patch_apply(L, name, patch, &ptr, &file_size);

    if (!g_use_ext2)
        fat_add_file(dst_dir->dir, name, ptr, file_size);
    else {
        char buf[1024];
        sprintf(buf, "%s[meta]", fileName);

        ext2_meta meta;
        read_meta_file(EXT2_TYPE_FILE | 0644, buf, &meta);
        if ((meta.type_and_perm & EXT2_TYPE_MASK) == EXT2_TYPE_DIRECTORY) {
            fprintf(stderr, "invalid meta file: %s\n", buf);
            exit(1);
        }

        ext2_add_file(dst_dir->dir, name, ptr, file_size, &meta);
    }

    free(ptr);
}

static void add_file_content(lua_State* L, const lua_dir* dst_dir,
    const char* name, const char* data, size_t dataLen)
{
    char fatShortName[13];

    (void)L;

    const char* fsName;
    if (g_use_ext2)
        fsName = name;
    else {
        fat_normalize_name(fatShortName, name);
        fsName = fatShortName;
    }

    printf("\n=> %s%s (generated)\n", dst_dir->path, fsName);

    if (!g_use_ext2)
        fat_add_file(dst_dir->dir, name, data, dataLen);
    else {
        ext2_meta meta;
        meta.type_and_perm = EXT2_TYPE_FILE | 0644;
        meta.uid = 0;
        meta.gid = 0;

        ext2_add_file(dst_dir->dir, name, data, dataLen, &meta);
    }
}

typedef enum recursive_t {
    RECURSIVE,
    RECURSIVE_FLAT,
    RECURSIVE_FLAT_SKIP_CMAKE,
    NON_RECURSIVE,
} recursive_t;

static void scan_dir(lua_State* L,
    const char* prefix, const lua_dir* d, int dirIndex, const char* path, recursive_t recursive)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s%s", prefix, path);
    DIR* it = opendir(buf);
    if (!it) {
        fprintf(stderr, "can't open dir \"%s\": %s\n", buf, strerror(errno));
        exit(1);
    }

    for (;;) {
        errno = 0;
        struct dirent* e = readdir(it);
        if (!e) {
            if (errno) {
                fprintf(stderr, "can't read dir \"%s\": %s\n", buf, strerror(errno));
                closedir(it);
                exit(1);
            }
            break;
        }

        size_t d_name_len = strlen(e->d_name);
        if (d_name_len == 1 && e->d_name[0] == '.')
            continue;
        if (d_name_len == 2 && e->d_name[0] == '.' && e->d_name[1] == '.')
            continue;

        if (recursive == RECURSIVE_FLAT_SKIP_CMAKE) {
            if (d_name_len == 14 && !memcmp(e->d_name, "CMakeLists.txt", 14))
                continue;
            if (d_name_len >= 6 && !memcmp(e->d_name + d_name_len - 6, ".cmake", 6))
                continue;
        }

        char buf[1024];
        snprintf(buf, sizeof(buf), "%s%s%s", prefix, path, e->d_name);

        unsigned long long file_size;
        bool is_dir;

      #ifndef _WIN32
        struct stat st;
        if (stat(buf, &st) < 0) {
            fprintf(stderr, "can't stat \"%s\": %s\n", buf, strerror(errno));
            closedir(it);
            exit(1);
        }
        is_dir = S_ISDIR(st.st_mode);
        file_size = st.st_size;
      #else
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (!GetFileAttributesEx(buf, GetFileExInfoStandard, &data)) {
            fprintf(stderr, "can't stat \"%s\": %s\n", buf, strerror(errno));
            closedir(it);
            exit(1);
        }
        is_dir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        file_size = ((uint64_t)data.nFileSizeHigh << 32) | data.nFileSizeLow;
      #endif

        if (is_dir) {
            if (recursive != NON_RECURSIVE) {
                const lua_dir* subdir;
                if (recursive == RECURSIVE_FLAT || recursive == RECURSIVE_FLAT_SKIP_CMAKE)
                    subdir = d;
                else {
                    char metafile[1032];
                    sprintf(metafile, "%s/[meta]", buf);

                    ext2_meta meta;
                    read_meta_file(EXT2_TYPE_DIRECTORY | 0755, metafile, &meta);
                    if ((meta.type_and_perm & EXT2_TYPE_MASK) != EXT2_TYPE_DIRECTORY) {
                        fprintf(stderr, "invalid meta file for directory: %s\n", metafile);
                        exit(1);
                    }

                    subdir = make_dir(L, d, dirIndex, e->d_name, &meta);
                }
                snprintf(buf, sizeof(buf), "%s%s/", path, e->d_name);
                scan_dir(L, prefix, subdir, lua_gettop(L), buf, recursive);
            }
        } else if (!g_skip_mkdisk) {
            if (g_use_ext2) {
                // ignore files ending with '[meta]'
                size_t len = strlen(e->d_name);
                if (len >= 5 && !memcmp(e->d_name + len - 6, "[meta]", 6))
                    continue;
            }

            fputc('+', stdout);
            fflush(stdout);
            //printf("%s", buf);

            add_file(L, d, e->d_name, buf, &file_size);
        }
    }

    closedir(it);
}

/****************************************************************************/

static int mkdisk_init(lua_State* L)
{
    const char* size = luaL_checkstring(L, 1);
    const char* boot = luaL_checkstring(L, 2);

    if (written) {
        initialized = false;
        written = false;
    } else if (initialized)
        return luaL_error(L, "init(): already initialized.");

    if (!strcmp(size, "3m"))
        disk_config = &disk_3M;
    else if (!strcmp(size, "20m"))
        disk_config = &disk_20M;
    else if (!strcmp(size, "100m"))
        disk_config = &disk_100M;
    else if (!strcmp(size, "400m"))
        disk_config = &disk_400M;
    else if (!strcmp(size, "500m"))
        disk_config = &disk_500M;
    else if (!strcmp(size, "510m"))
        disk_config = &disk_510M;
    else if (!strcmp(size, "520m"))
        disk_config = &disk_520M;
    else if (!strcmp(size, "1g"))
        disk_config = &disk_1G;
    else
        return luaL_error(L, "init(): invalid disk size.");

    g_use_ext2 = !strcmp(boot, "ext2") || !strcmp(boot, "ext2;mbr=fat"); // done here for mbr_init()
    g_mbr_fat = !strcmp(boot, "ext2;mbr=fat");

    vhd_init();
    mbr_init(vhd_sector(0));

    if (!strcmp(boot, "fat16"))
        fat_init(bootCodeNone);
    else if (!strcmp(boot, "fat16-win95"))
        fat_init(bootCode95);
    else if (!strcmp(boot, "fat16-nt3.1"))
        fat_init(bootCodeNT);
    else if (!strcmp(boot, "ext2"))
        ext2_init();
    else if (!strcmp(boot, "ext2;mbr=fat"))
        ext2_init();
    else
        return luaL_error(L, "init(): invalid filesystem ID.");

    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &lua_DIR);

    if (g_use_ext2)
        push_directory(L, ext2_root_directory(), lua_absindex(L, -1), "/", "/", "");
    else
        push_directory(L, fat_root_directory(), lua_absindex(L, -1), "/", "/", "");
    lua_pop(L, 1);

    lua_setglobal(L, "DIR");

    initialized = true;
    return 0;
}

static int mkdisk_enable_lfn(lua_State* L)
{
    (void)L;
    fat_enable_lfn = true;
    return 0;
}

static int mkdisk_add_directory(lua_State* L)
{
    size_t src_dir_len;
    const lua_dir* dst_dir = get_directory(L, 1);
    const char* src_dir = luaL_checklstring(L, 2, &src_dir_len);
    const char* recurse = luaL_optstring(L, 3, "recursive");

    if (!initialized)
        return luaL_error(L, "add_directory(): not initialized.");
    if (written)
        return luaL_error(L, "add_directory(): already finished.");

    luaL_argcheck(L, dst_dir != NULL, 1, "directory expected");

    recursive_t recursive;
    if (!strcmp(recurse, "recursive"))
        recursive = RECURSIVE;
    else if (!strcmp(recurse, "flat"))
        recursive = RECURSIVE_FLAT;
    else if (!strcmp(recurse, "flat,skip-cmake"))
        recursive = RECURSIVE_FLAT_SKIP_CMAKE;
    else if (!strcmp(recurse, "non-recursive"))
        recursive = NON_RECURSIVE;
    else
        return luaL_error(L, "add_directory(): invalid recursive mode.");

    if (!g_skip_mkdisk)
        printf("\n%s => %s\n[", src_dir, dst_dir->path);

    // append trailing '/'
    if (src_dir[src_dir_len] != '/') {
        lua_pushvalue(L, 2);
        lua_pushliteral(L, "/");
        lua_concat(L, 2);
        src_dir = lua_tolstring(L, -1, &src_dir_len);
    }

    luaL_checkstack(L, 1000, "scan_dir");
    scan_dir(L, src_dir, dst_dir, 1, "", recursive);

    if (!g_skip_mkdisk)
        printf("]\n");

    return 0;
}

static int mkdisk_make_directory(lua_State* L)
{
    const lua_dir* dst_dir = get_directory(L, 1);
    const char* name = luaL_checkstring(L, 2);

    if (!initialized)
        return luaL_error(L, "make_directory(): not initialized.");
    if (written)
        return luaL_error(L, "make_directory(): already finished.");

    luaL_argcheck(L, dst_dir != NULL, 1, "directory expected");

    ext2_meta meta;
    meta.type_and_perm = EXT2_TYPE_DIRECTORY | 0755;
    meta.uid = 0;
    meta.gid = 0;

    make_dir(L, dst_dir, 1, name, &meta);
    return 1;
}

static int mkdisk_add_file(lua_State* L)
{
    const lua_dir* dst_dir = get_directory(L, 1);
    const char* name = luaL_checkstring(L, 2);
    const char* srcPath = luaL_checkstring(L, 3);

    if (!initialized)
        return luaL_error(L, "add_file(): not initialized.");
    if (written)
        return luaL_error(L, "add_file(): already finished.");

    luaL_argcheck(L, dst_dir != NULL, 1, "directory expected");

    if (!g_skip_mkdisk)
        add_file(L, dst_dir, name, srcPath, NULL);

    return 1;
}

static int mkdisk_add_file_content(lua_State* L)
{
    size_t contentLen = 0;
    const lua_dir* dst_dir = get_directory(L, 1);
    const char* name = luaL_checkstring(L, 2);
    const char* content = luaL_checklstring(L, 3, &contentLen);

    if (!initialized)
        return luaL_error(L, "add_file_content(): not initialized.");
    if (written)
        return luaL_error(L, "add_file_content(): already finished.");

    luaL_argcheck(L, dst_dir != NULL, 1, "directory expected");

    if (!g_skip_mkdisk)
        add_file_content(L, dst_dir, name, content, contentLen);

    return 1;
}

static int mkdisk_write_vhd(lua_State* L)
{
    const char* vhd_name = luaL_checkstring(L, 1);

    if (!initialized)
        return luaL_error(L, "write_vhd(): not initialized.");
    if (written)
        return luaL_error(L, "write_vhd(): already finished.");

    if (!g_skip_mkdisk) {
        if (g_use_ext2)
            ext2_write();
        else
            fat_write();
        vhd_write(vhd_name);
    }

    written = true;
    return 0;
}

static int mkdisk_write_img(lua_State* L)
{
    const char* img_name = luaL_checkstring(L, 1);
    const char* mode = luaL_optstring(L, 2, "mbr");

    if (!initialized)
        return luaL_error(L, "write_img(): not initialized.");
    if (written)
        return luaL_error(L, "write_img(): already finished.");

    bool mbr;
    if (!strcmp(mode, "mbr"))
        mbr = true;
    else if (!strcmp(mode, "nombr"))
        mbr = false;
    else
        return luaL_error(L, "invalid img write mode: %s", mode);

    if (!g_skip_mkdisk) {
        if (g_use_ext2)
            ext2_write();
        else
            fat_write();
        vhd_write_as_img(img_name, mbr);
    }

    written = true;
    return 0;
}

static int dir_tostring(lua_State* L)
{
    const lua_dir* d = get_directory(L, 1);
    luaL_argcheck(L, d != NULL, 1, "directory expected");
    lua_pushstring(L, d->path);
    return 1;
}

static const luaL_Reg funcs[] = {
    { "init", mkdisk_init },
    { "enable_lfn", mkdisk_enable_lfn },
    { "add_directory", mkdisk_add_directory },
    { "make_directory", mkdisk_make_directory },
    { "add_file", mkdisk_add_file },
    { "add_file_content", mkdisk_add_file_content },
    { "write_vhd", mkdisk_write_vhd },
    { "write_img", mkdisk_write_img },
    { NULL, NULL }
};

static int luaopen_mkdisk(lua_State* L)
{
    luaL_newmetatable(L, CLASS_DIRECTORY);

    luaL_newmetatable(L, CLASS_DIRECTORY_TABLE);
    lua_pushcfunction(L, dir_tostring);
    lua_setfield(L, -2, "__tostring");

    lua_pushboolean(L, g_skip_mkdisk);
    lua_setglobal(L, "skip_boot");

    luaL_newlib(L, funcs);
    return 1;
}

void MkDisk_InitLua(lua_State* L)
{
    luaL_requiref(L, "mkdisk", luaopen_mkdisk, 1);
    lua_pop(L, 1);
}
