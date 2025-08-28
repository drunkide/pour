#include <common/common.h>
#include <common/file.h>
#include <common/console.h>
#include <common/dirs.h>
#include <common/script.h>
#include <mkdisk/mkdisk.h>
#include <mkdisk/vhd.h>
#include <mkdisk/mbr.h>
#include <mkdisk/fat.h>
#include <mkdisk/ext2.h>
#include <mkdisk/ext2_defs.h>
#include <mkdisk/bootcode.h>
#include <mkdisk/disk_config.h>
#include <grp/grpfile.h>
#include <patch/patch.h>
#include <string.h>

#define CLASS_DIRECTORY "mkdisk.directory"
#define CLASS_DIRECTORY_TABLE "mkdisk.directory.table"
#define CLASS_DISK "mkdisk.disk"

STRUCT(DiskList) {
    Disk* first;
    Disk* last;
};

static char marker_DIR;
static char marker_LIST;

/********************************************************************************************************************/

static void MkDisk_ReadMetaFile(Disk* dsk, uint16_t default_perm, const char* file, ext2_meta* out)
{
    lua_State* L = dsk->L;

    out->type_and_perm = default_perm;
    out->uid = 0;
    out->gid = 0;

    if (!File_Exists(L, file))
        return;

    const char* str = File_PushContents(L, file, NULL);

    unsigned t, u, g;
    char type;
    if (sscanf(str, "%c %o %u:%u", &type, &t, &u, &g) != 4)
        luaL_error(L, "invalid meta file: %s", file);

    lua_pop(L, 1);

    switch (type) {
        case 'D': out->type_and_perm = EXT2_TYPE_DIRECTORY; break;
        case 'Q': out->type_and_perm = EXT2_TYPE_FIFO; break;
        case 'C': out->type_and_perm = EXT2_TYPE_CHAR_DEV; break;
        case 'B': out->type_and_perm = EXT2_TYPE_BLOCK_DEV; break;
        case 'F': out->type_and_perm = EXT2_TYPE_FILE; break;
        case 'L': out->type_and_perm = EXT2_TYPE_SYMLINK; break;
        case 'S': out->type_and_perm = EXT2_TYPE_SOCKET; break;
        default: luaL_error(L, "invalid meta file: %s", file);
    }

    if (t > 0xfff || u > 0xffff || g > 0xffff)
        luaL_error(L, "invalid meta file: %s", file);

    out->type_and_perm |= t;
    out->uid = u;
    out->gid = g;
}

static void MkDisk_ReadMetaFileForDirectory(Disk* dsk, const char* dirName, ext2_meta* out)
{
    lua_State* L = dsk->L;
    const char* buf = lua_pushfstring(L, "%s/[meta]", dirName);

    MkDisk_ReadMetaFile(dsk, EXT2_TYPE_DIRECTORY | 0755, buf, out);
    if ((out->type_and_perm & EXT2_TYPE_MASK) != EXT2_TYPE_DIRECTORY)
        luaL_error(L, "invalid meta file for directory: %s", buf);

    lua_pop(L, 1);
}

static void MkDisk_ReadMetaFileForFile(Disk* dsk, const char* fileName, ext2_meta* out)
{
    lua_State* L = dsk->L;
    const char* buf = lua_pushfstring(L, "%s[meta]", fileName);

    MkDisk_ReadMetaFile(dsk, EXT2_TYPE_FILE | 0644, buf, out);
    if ((out->type_and_perm & EXT2_TYPE_MASK) == EXT2_TYPE_DIRECTORY)
        luaL_error(L, "invalid meta file: %s", buf);

    lua_pop(L, 1);
}

/********************************************************************************************************************/

DiskDir* MkDisk_GetDirectory(lua_State* L, int index)
{
    if (!lua_istable(L, index))
        luaL_typeerror(L, index, "directory");

    lua_rawgetp(L, index, &marker_DIR);
    if (lua_isnoneornil(L, -1))
        luaL_typeerror(L, index, "directory");

    DiskDir* ld = (DiskDir*)luaL_testudata(L, -1, CLASS_DIRECTORY);
    if (!ld)
        luaL_typeerror(L, index, "directory");

    lua_pop(L, 1);
    return ld;
}

#define USERVAL_DISK 1

static DiskDir* MkDisk_PushDirectory(Disk* dsk, int dskIndex, FSDir* dir, int parentIndex,
    const char* fatShortName, const char* origName, const char* path)
{
    lua_State* L = dsk->L;

    size_t pathlen = strlen(path) + 1;

    lua_newtable(L);
    luaL_setmetatable(L, CLASS_DIRECTORY_TABLE);

    DiskDir* ld = (DiskDir*)lua_newuserdatauv(L, sizeof(DiskDir) + pathlen + 1, 1);
    char* dstPath = (char*)(ld + 1);
    *dstPath = '/';
    if (*path == '/')
        ++path;
    memcpy(dstPath + 1, path, pathlen);
    ld->disk = dsk;
    ld->path = dstPath;
    ld->dir = dir;
    luaL_setmetatable(L, CLASS_DIRECTORY);

    lua_pushvalue(L, dskIndex);
    lua_setiuservalue(L, -2, USERVAL_DISK);

    lua_rawsetp(L, -2, &marker_DIR);

    if (dsk->fs == FS_FAT) {
        lua_pushvalue(L, -1);
        lua_setfield(L, parentIndex, fatShortName);
    }

    if (dsk->fs != FS_FAT || dsk->fatEnableLFN) {
        lua_pushvalue(L, -1);
        lua_setfield(L, parentIndex, origName);
    }

    return ld;
}

static const DiskDir* MkDisk_PushMakeDir(Disk* dsk, int dskIndex, const DiskDir* parentDir,
    int parentIndex, const char* dirName, const ext2_meta* meta)
{
    lua_State* L = dsk->L;

    const DiskDir* subdir;
    char fatShortName[13];

    if (parentDir->disk != dsk)
        luaL_error(L, "parentDir disk mismatch!");

    if (dsk->fs != FS_FAT || dsk->fatEnableLFN)
        lua_getfield(L, parentIndex, dirName);
    else {
        fat_normalize_name(fatShortName, dirName);
        lua_getfield(L, parentIndex, fatShortName);
    }

    if (!lua_isnoneornil(L, -1))
        subdir = MkDisk_GetDirectory(L, -1);
    else {
        lua_pop(L, 1); /* pop nil */

        lua_pushstring(L, parentDir->path);
        lua_pushstring(L, dirName);
        lua_pushliteral(L, "/");
        lua_concat(L, 3);
        const char* newPath = lua_tostring(L, -1);

        FSDir* dir = NULL;
        switch (dsk->fs) {
            case FS_FAT: dir = fat_create_directory(parentDir->dir, dirName); break;
            case FS_EXT2: dir = ext2_create_directory(parentDir->dir, dirName, meta); break;
        }

        subdir = MkDisk_PushDirectory(dsk, dskIndex, dir, parentIndex, fatShortName, dirName, newPath);

        lua_remove(L, -2);
    }

    return subdir;
}

/********************************************************************************************************************/

static void MkDisk_AddFile(Disk* dsk, const DiskDir* dstDir,
    const char* name, const char* fileName, const uint64_t* pFileSize)
{
    lua_State* L = dsk->L;

    int n = lua_gettop(L);
    char fatShortName[13];

    if (dstDir->disk != dsk)
        luaL_error(L, "dstDir disk mismatch!");

    File* f = File_PushOpen(L, fileName, FILE_OPEN_SEQUENTIAL_READ);

    const char* fsName;
    if (dsk->fs != FS_FAT || dsk->fatEnableLFN)
        fsName = name;
    else {
        fat_normalize_name(fatShortName, name);
        fsName = fatShortName;
    }

    if (!pFileSize)
        Con_PrintF(L, COLOR_STATUS, "\n=> %s%s\n", dstDir->path, fsName);

    PATCH* patch = patch_find(L, fsName);

    size_t fileSize;
    if (!pFileSize)
        fileSize = File_GetSize(f);
    else {
        if (*pFileSize > MAX_FILE_SIZE)
            luaL_error(L, "file too large: %s", name);
        fileSize = (size_t)*pFileSize;
    }

    void* ptr = lua_newuserdatauv(L, fileSize + (patch ? patch->extraBytes : 0), 0);

    File_Read(f, ptr, fileSize);
    File_Close(f);

    if (patch)
        patch_apply(L, name, patch, &ptr, &fileSize);

    switch (dsk->fs) {
        case FS_FAT:
            fat_add_file(dstDir->dir, name, ptr, fileSize);
            break;
        case FS_EXT2: {
            ext2_meta meta;
            MkDisk_ReadMetaFileForFile(dsk, fileName, &meta);
            ext2_add_file(dstDir->dir, name, ptr, fileSize, &meta);
            break;
        }
    }

    lua_settop(L, n);
}

void MkDisk_AddFileContent(Disk* dsk, const DiskDir* dstDir,
    const char* name, const char* data, size_t dataLen)
{
    lua_State* L = dsk->L;

    int n = lua_gettop(L);
    char fatShortName[13];

    if (dstDir->disk != dsk)
        luaL_error(L, "dstDir disk mismatch!");

    const char* fsName;
    if (dsk->fs != FS_FAT || dsk->fatEnableLFN)
        fsName = name;
    else {
        fat_normalize_name(fatShortName, name);
        fsName = fatShortName;
    }

    Con_PrintF(L, COLOR_STATUS, "\n=> %s%s (generated)\n", dstDir->path, fsName);

    switch (dsk->fs) {
        case FS_FAT:
            fat_add_file(dstDir->dir, name, data, dataLen);
            break;
        case FS_EXT2: {
            ext2_meta meta;
            meta.type_and_perm = EXT2_TYPE_FILE | 0644;
            meta.uid = 0;
            meta.gid = 0;
            ext2_add_file(dstDir->dir, name, data, dataLen, &meta);
            break;
        }
    }

    lua_settop(L, n);
}

/********************************************************************************************************************/

typedef enum recursive_t {
    RECURSIVE,
    RECURSIVE_FLAT,
    RECURSIVE_FLAT_SKIP_CMAKE,
    NON_RECURSIVE,
} recursive_t;

static void MkDisk_ScanDir(Disk* dsk, int dskIndex,
    const char* prefix, const DiskDir* d, int dirIndex, const char* path, recursive_t recursive)
{
    lua_State* L = dsk->L;
    int top = lua_gettop(L);

    if (d->disk != dsk)
        luaL_error(L, "dir disk mismatch!");

    Dir* it = File_PushOpenDir(L, lua_pushfstring(L, "%s%s", prefix, path));

    for (;;) {
        const char* d_name = File_ReadDir(it);
        if (!d_name)
            break;

        size_t d_name_len = strlen(d_name);
        if (d_name_len == 1 && d_name[0] == '.')
            continue;
        if (d_name_len == 2 && d_name[0] == '.' && d_name[1] == '.')
            continue;

        if (recursive == RECURSIVE_FLAT_SKIP_CMAKE) {
            if (d_name_len == 14 && !memcmp(d_name, "CMakeLists.txt", 14))
                continue;
            if (d_name_len >= 6 && !memcmp(d_name + d_name_len - 6, ".cmake", 6))
                continue;
        }

        const char* buf = lua_pushfstring(L, "%s%s%s", prefix, path, d_name);

        uint64_t fileSize;
        bool isDir;
        File_QueryInfo(L, buf, &isDir, &fileSize);

        if (isDir) {
            if (recursive != NON_RECURSIVE) {
                int top2 = lua_gettop(L);

                const DiskDir* subdir;
                int subdirIndex;

                if (recursive == RECURSIVE_FLAT || recursive == RECURSIVE_FLAT_SKIP_CMAKE) {
                    subdir = d;
                    subdirIndex = dirIndex;
                } else {
                    ext2_meta meta;
                    MkDisk_ReadMetaFileForDirectory(dsk, buf, &meta);
                    subdir = MkDisk_PushMakeDir(dsk, dskIndex, d, dirIndex, d_name, &meta);
                    subdirIndex = lua_gettop(L);
                }

                const char* subname = lua_pushfstring(L, "%s%s/", path, d_name);
                MkDisk_ScanDir(dsk, dskIndex, prefix, subdir, subdirIndex, subname, recursive);

                lua_settop(L, top2);
            }
        } else {
            /* ignore files ending with '[meta]' */
            size_t len = strlen(d_name);
            if (len >= 5 && !memcmp(d_name + len - 6, "[meta]", 6))
                continue;

            Con_Print(L, COLOR_PROGRESS, "+");
            Con_Flush(L);

            MkDisk_AddFile(dsk, d, d_name, buf, &fileSize);
        }

        lua_pop(L, 1);
    }

    File_CloseDir(it);
    lua_settop(L, top);
}

/****************************************************************************/

static int mkdisk_enable_lfn(lua_State* L)
{
    Disk* dsk = (Disk*)luaL_checkudata(L, 1, CLASS_DISK);
    dsk->fatEnableLFN = true;
    return 0;
}

static int mkdisk_add_directory(lua_State* L)
{
    size_t srcDirLen;
    const int dskIndex = 1;
    Disk* dsk = (Disk*)luaL_checkudata(L, dskIndex, CLASS_DISK);
    const int dstDirIndex = 2;
    const DiskDir* dstDir = MkDisk_GetDirectory(L, dstDirIndex);
    const int srcDirIndex = 3;
    const char* srcDir = luaL_checklstring(L, srcDirIndex, &srcDirLen);
    const char* recurse = luaL_optstring(L, 4, "recursive");

    if (dstDir->disk != dsk)
        luaL_error(L, "dstDir disk mismatch!");
    if (dsk->built)
        return luaL_error(L, "add_directory(): already finished.");

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

    Con_PrintF(L, COLOR_STATUS, "\n%s => %s\n", srcDir, dstDir->path);
    Con_Print(L, COLOR_PROGRESS_SIDE, "[");

    /* append trailing '/' */
    if (srcDir[srcDirLen] != '/') {
        lua_pushvalue(L, srcDirIndex);
        lua_pushliteral(L, "/");
        lua_concat(L, 2);
        srcDir = lua_tolstring(L, -1, &srcDirLen);
    }

    luaL_checkstack(L, 1000, "MkDisk_ScanDir");
    MkDisk_ScanDir(dsk, dskIndex, srcDir, dstDir, dstDirIndex, "", recursive);

    Con_Print(L, COLOR_PROGRESS_SIDE, "]\n");

    return 0;
}

static int mkdisk_make_directory(lua_State* L)
{
    const int dskIndex = 1;
    Disk* dsk = (Disk*)luaL_checkudata(L, dskIndex, CLASS_DISK);
    const int dstDirIndex = 2;
    const DiskDir* dstDir = MkDisk_GetDirectory(L, dstDirIndex);
    const char* name = luaL_checkstring(L, 3);

    if (dstDir->disk != dsk)
        luaL_error(L, "dstDir disk mismatch!");
    if (dsk->built)
        return luaL_error(L, "make_directory(): already finished.");

    ext2_meta meta;
    meta.type_and_perm = EXT2_TYPE_DIRECTORY | 0755;
    meta.uid = 0;
    meta.gid = 0;
    MkDisk_PushMakeDir(dsk, dskIndex, dstDir, dstDirIndex, name, &meta);

    return 1;
}

static int mkdisk_add_file(lua_State* L)
{
    Disk* dsk = (Disk*)luaL_checkudata(L, 1, CLASS_DISK);
    const DiskDir* dstDir = MkDisk_GetDirectory(L, 2);
    const char* name = luaL_checkstring(L, 3);
    const char* srcPath = luaL_checkstring(L, 4);

    if (dstDir->disk != dsk)
        luaL_error(L, "dstDir disk mismatch!");
    if (dsk->built)
        return luaL_error(L, "add_file(): already finished.");

    MkDisk_AddFile(dsk, dstDir, name, srcPath, NULL);
    return 0;
}

static int mkdisk_add_file_content(lua_State* L)
{
    size_t contentLen = 0;
    Disk* dsk = (Disk*)luaL_checkudata(L, 1, CLASS_DISK);
    const DiskDir* dstDir = MkDisk_GetDirectory(L, 2);
    const char* name = luaL_checkstring(L, 3);
    const char* content = luaL_checklstring(L, 4, &contentLen);

    if (dstDir->disk != dsk)
        luaL_error(L, "dstDir disk mismatch!");
    if (dsk->built)
        return luaL_error(L, "add_file_content(): already finished.");

    MkDisk_AddFileContent(dsk, dstDir, name, content, contentLen);
    return 0;
}

static void MkDisk_EnsureDiskBuilt(Disk* dsk)
{
    if (!dsk->built) {
        GrpFile_WriteAllForDisk(dsk);
        switch (dsk->fs) {
            case FS_FAT: Fat_Write(dsk); break;
            case FS_EXT2: Ext2_Write(dsk); break;
        }
        dsk->built = true;
    }
}

static void MkDisk_WriteDefault(Disk* dsk)
{
    MkDisk_EnsureDiskBuilt(dsk);

    const char* ext = strrchr(dsk->outFile, '.');
    if (ext && !strcmp(ext, ".vhd"))
        VHD_Write(dsk, dsk->outFile);
    else
        VHD_WriteAsIMG(dsk, dsk->outFile, true);
}

static int mkdisk_write_default(lua_State* L)
{
    Disk* dsk = (Disk*)luaL_checkudata(L, 1, CLASS_DISK);
    MkDisk_WriteDefault(dsk);
    return 0;
}

static int mkdisk_write_vhd(lua_State* L)
{
    Disk* dsk = (Disk*)luaL_checkudata(L, 1, CLASS_DISK);
    const char* vhd_name = luaL_checkstring(L, 2);

    MkDisk_EnsureDiskBuilt(dsk);
    VHD_Write(dsk, vhd_name);

    return 0;
}

static int mkdisk_write_img(lua_State* L)
{
    Disk* dsk = (Disk*)luaL_checkudata(L, 1, CLASS_DISK);
    const char* img_name = luaL_checkstring(L, 2);
    const char* mode = luaL_optstring(L, 3, "mbr");

    bool mbr;
    if (!strcmp(mode, "mbr"))
        mbr = true;
    else if (!strcmp(mode, "nombr"))
        mbr = false;
    else
        return luaL_error(L, "invalid img write mode: %s", mode);

    MkDisk_EnsureDiskBuilt(dsk);
    VHD_WriteAsIMG(dsk, img_name, mbr);

    return 0;
}

static void MkDisk_RemoveDiskFromList(Disk* dsk)
{
    lua_State* L = dsk->L;

    lua_rawgetp(L, LUA_REGISTRYINDEX, &marker_LIST);
    DiskList* list = (DiskList*)lua_touserdata(L, -1);

    if (dsk->next)
        dsk->next->prev = dsk->prev;
    else {
        assert(list->last == dsk);
        list->last = dsk->prev;
    }

    if (dsk->prev)
        dsk->prev->next = dsk->next;
    else {
        assert(list->first == dsk);
        list->first = dsk->next;
    }

    dsk->next = NULL;
    dsk->prev = NULL;
    dsk->inList = false;
}

static int mkdisk_destructor(lua_State* L)
{
    Disk* dsk = (Disk*)luaL_checkudata(L, 1, CLASS_DISK);
    if (!dsk->inList)
        return 0;

    MkDisk_RemoveDiskFromList(dsk);

    if (!dsk->built && !Script_IsAbnormalTermination(L))
        MkDisk_WriteDefault(dsk);

    return 0;
}

/****************************************************************************/

static const luaL_Reg disk_funcs[] = {
    { "enable_lfn", mkdisk_enable_lfn },
    { "add_directory", mkdisk_add_directory },
    { "make_directory", mkdisk_make_directory },
    { "add_file", mkdisk_add_file },
    { "add_file_content", mkdisk_add_file_content },
    { "write", mkdisk_write_default },
    { "write_vhd", mkdisk_write_vhd },
    { "write_img", mkdisk_write_img },
    { "__gc", mkdisk_destructor },
    { NULL, NULL }
};

#define USERVAL_FILE_NAME 1
#define USERVAL_ROOT_DIRECTORY 2

static int mkdisk_create(lua_State* L)
{
    int nameIndex = 1;
    luaL_checkstring(L, nameIndex);
    const char* size = luaL_checkstring(L, 2);
    const char* boot = luaL_checkstring(L, 3);

    Disk* dsk = (Disk*)lua_newuserdatauv(L, sizeof(Disk), 2);
    int resultIdx = lua_gettop(L);

    dsk->L = L;
    dsk->name = NULL;
    dsk->mbrFAT = false;
    dsk->fatEnableLFN = false;
    dsk->inList = false;
    dsk->built = false;

    luaL_setmetatable(L, CLASS_DISK);

    char fileName[DIR_MAX];
    Script_GetString(L, nameIndex, fileName, sizeof(fileName), "disk file name is too long");
    dsk->outFile = Dir_PushAbsolutePath(L, fileName);
    lua_setiuservalue(L, resultIdx, USERVAL_FILE_NAME);

    if (!strcmp(size, "3m"))
        dsk->config = &disk_3M;
    else if (!strcmp(size, "20m"))
        dsk->config = &disk_20M;
    else if (!strcmp(size, "100m"))
        dsk->config = &disk_100M;
    else if (!strcmp(size, "400m"))
        dsk->config = &disk_400M;
    else if (!strcmp(size, "500m"))
        dsk->config = &disk_500M;
    else if (!strcmp(size, "510m"))
        dsk->config = &disk_510M;
    else if (!strcmp(size, "520m"))
        dsk->config = &disk_520M;
    else if (!strcmp(size, "1g"))
        dsk->config = &disk_1G;
    else
        return luaL_error(L, "disk:create(): invalid disk size.");

    const uint8_t* bootCode = NULL;
    if (!strcmp(boot, "fat16"))
        dsk->fs = FS_FAT, bootCode = bootCodeNone;
    else if (!strcmp(boot, "fat16-win95"))
        dsk->fs = FS_FAT, bootCode = bootCode95;
    else if (!strcmp(boot, "fat16-nt3.1"))
        dsk->fs = FS_FAT, bootCode = bootCodeNT;
    else if (!strcmp(boot, "ext2"))
        dsk->fs = FS_EXT2;
    else if (!strcmp(boot, "ext2;mbr=fat"))
        dsk->fs = FS_EXT2, dsk->mbrFAT = true;
    else
        return luaL_error(L, "init(): invalid filesystem ID.");

    VHD_Init(dsk);
    MBR_Init(dsk, VHD_Sector(dsk, 0));

    lua_settop(L, resultIdx);

    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setiuservalue(L, resultIdx, USERVAL_ROOT_DIRECTORY);

    FSDir* root = NULL;
    switch (dsk->fs) {
        case FS_FAT: Fat_Init(dsk, bootCode, &root); break;
        case FS_EXT2: Ext2_Init(dsk, &root); break;
    }
    MkDisk_PushDirectory(dsk, resultIdx, root, lua_absindex(L, -1), "/", "/", "");

    DiskList* list;
    lua_rawgetp(L, LUA_REGISTRYINDEX, &marker_LIST);
    if (!lua_isnoneornil(L, -1))
        list = (DiskList*)lua_touserdata(L, -1);
    else {
        list = (DiskList*)lua_newuserdatauv(L, sizeof(DiskList), 0);
        list->first = NULL;
        list->last = NULL;
        lua_rawsetp(L, LUA_REGISTRYINDEX, &marker_LIST);
    }

    lua_pop(L, 2);

    dsk->inList = true;
    dsk->next = NULL;
    dsk->prev = list->last;

    if (!list->first)
        list->first = dsk;
    else
        list->last->next = dsk;
    list->last = dsk;

    return 2;
}

static int mkdisk_create_named(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    lua_pushvalue(L, 1);
    luaL_ref(L, LUA_REGISTRYINDEX);
    lua_remove(L, 1);

    int ret = mkdisk_create(L);
    DONT_WARN_UNUSED(ret);
    assert(ret == 2);

    Disk* dsk = (Disk*)luaL_checkudata(L, -2, CLASS_DISK);
    dsk->name = name;
    lua_pushvalue(L, -2);
    dsk->ref = luaL_ref(L, LUA_REGISTRYINDEX);

    return 2;
}

static int mkdisk_get_disk(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    DiskList* list;
    lua_rawgetp(L, LUA_REGISTRYINDEX, &marker_LIST);
    if (lua_isnoneornil(L, -1))
        goto notfound;
    list = (DiskList*)lua_touserdata(L, -1);
    lua_pop(L, 1);

    for (Disk* dsk = list->first; dsk; dsk = dsk->next) {
        if (dsk->name && !strcmp(dsk->name, name)) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, dsk->ref);
            lua_getiuservalue(L, -1, USERVAL_ROOT_DIRECTORY);
            return 2;
        }
    }

  notfound:
    return luaL_error(L, "disk named \"%s\" was not found.", name);
}

/****************************************************************************/

static const luaL_Reg funcs[] = {
    { "create", mkdisk_create },
    { "create_named", mkdisk_create_named },
    { "get_disk", mkdisk_get_disk },
    { NULL, NULL }
};

static int dir_tostring(lua_State* L)
{
    const DiskDir* d = MkDisk_GetDirectory(L, 1);
    lua_pushstring(L, d->path);
    return 1;
}

static int disk_tostring(lua_State* L)
{
    const Disk* dsk = (Disk*)luaL_checkudata(L, 1, CLASS_DISK);
    DONT_WARN_UNUSED(dsk);
    lua_pushstring(L, "<Disk*>");
    return 1;
}

static int luaopen_mkdisk(lua_State* L)
{
    luaL_newmetatable(L, CLASS_DIRECTORY);

    luaL_newmetatable(L, CLASS_DIRECTORY_TABLE);
    lua_pushcfunction(L, dir_tostring);
    lua_setfield(L, -2, "__tostring");

    luaL_newmetatable(L, CLASS_DISK);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, disk_tostring);
    lua_setfield(L, -2, "__tostring");
    luaL_setfuncs(L, disk_funcs, 0);

    luaL_newlib(L, funcs);
    return 1;
}

void MkDisk_InitLua(lua_State* L)
{
    luaL_requiref(L, "mkdisk", luaopen_mkdisk, 1);
    lua_pop(L, 1);
}

void MkDisk_WriteAllDisks(lua_State* L)
{
    DiskList* list;
    lua_rawgetp(L, LUA_REGISTRYINDEX, &marker_LIST);
    if (lua_isnoneornil(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    list = (DiskList*)lua_touserdata(L, -1);
    lua_pop(L, 1);

    while (list->first) {
        Disk* dsk = list->first;
        MkDisk_RemoveDiskFromList(dsk);
        MkDisk_WriteDefault(dsk);
    }
}
