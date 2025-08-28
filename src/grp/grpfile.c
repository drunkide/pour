#include <common/common.h>
#include <common/console.h>
#include <mkdisk/mkdisk.h>
#include <string.h>

#pragma pack(push, 1)

#define SW_SHOWNORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3

typedef struct POINT {
    int16_t x;
    int16_t y;
} POINT;

typedef struct RECT {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
} RECT;

typedef struct GROUPHEADER {
    char cIdentifier[4];
    uint16_t wCheckSum;
    uint16_t cbGroup;
    uint16_t nCmdShow;
    RECT rcNormal;
    POINT ptMin;
    uint16_t pName;
    uint16_t wLogPixelsX;
    uint16_t wLogPixelsY;
    uint8_t bBitsPerPixel;
    uint8_t bPlanes;
    uint16_t reserved;
    uint16_t cItems;
} GROUPHEADER;

typedef struct ITEMDATA {
    POINT pt;
    uint16_t iIcon;
    uint16_t cbResource;
    uint16_t cbANDPlane;
    uint16_t cbXORPlane;
    uint16_t pHeader;
    uint16_t pANDPlane;
    uint16_t pXORPlane;
    uint16_t pName;
    uint16_t pCommand;
    uint16_t pIconPath;
} ITEMDATA;

// 0x8101 - path for the application
// 0x8102 - shortcut key
// 0x8103 - minimized

typedef struct TAGDATA {
    int16_t wID;
    int16_t wItem;
    uint16_t cb;
} TAGDATA;

typedef struct ICONHEADER {
    uint16_t xHotSpot;
    uint16_t yHotSpot;
    uint16_t width;
    uint16_t height;
    uint16_t unknown;
    uint8_t bPlanes;
    uint8_t bBitsPerPixel;
} ICONHEADER;

#pragma pack(pop)

typedef struct Item {
    struct Item* next;
    const char* name;
    const char* command;
    const char* workingDir;
    const char* iconPath;
    size_t nameLength;
    size_t commandLength;
    size_t workingDirLength;
    size_t iconPathLength;
    ITEMDATA header;
    ICONHEADER iconHeader;
    uint8_t andPlane[128];
    uint8_t xorPlane[128];
    uint16_t offset;
} Item;

STRUCT(GrpFile) {
    GrpFile* next;
    GROUPHEADER header;
    DiskDir* dir;
    Item* firstItem;
    Item* lastItem;
    const char* fileName;
    const char* title;
    int width;
    int height;
    int ref;
    bool written;
};

#define ICON_X 21
#define ICON_Y 0
#define ICON_W 75
#define ICON_H 72

#define WINDOW_RIGHT_PADDING -8
#define WINDOW_BOTTOM_PADDING 8

#define ICON_MINIMIZED_Y 253

#define CLASS_GRPFILE "GrpFile*"

#define USERVAL_GROUP_TITLE 1
#define USERVAL_GROUP_DIR 2
#define USERVAL_GROUP_FILE_NAME 3
#define USERVAL_GROUP_ITEM_TABLE 2

static GrpFile* g_groups;
static int g_groupFileCount;

static int grpfile_create(lua_State* L)
{
    const int titleIndex = 1;
    const char* title = luaL_checkstring(L, titleIndex);
    const int dirIndex = 2;
    DiskDir* dir = MkDisk_GetDirectory(L, dirIndex);
    const int fileNameIndex = 3;
    const char* fileName = luaL_checkstring(L, fileNameIndex);

    GrpFile* grp = (GrpFile*)lua_newuserdatauv(L, sizeof(GrpFile), 4);
    luaL_setmetatable(L, CLASS_GRPFILE);

    lua_pushvalue(L, titleIndex);
    lua_setiuservalue(L, -2, USERVAL_GROUP_TITLE);
    lua_pushvalue(L, dirIndex);
    lua_setiuservalue(L, -2, USERVAL_GROUP_DIR);
    lua_pushvalue(L, fileNameIndex);
    lua_setiuservalue(L, -2, USERVAL_GROUP_FILE_NAME);
    lua_newtable(L);

    lua_setiuservalue(L, -2, USERVAL_GROUP_ITEM_TABLE);

    memcpy(grp->header.cIdentifier, "PMCC", 4);
    grp->header.wCheckSum = 0;
    grp->header.nCmdShow = SW_SHOWNORMAL;
    grp->header.rcNormal.left = 100;
    grp->header.rcNormal.top = 100;
    grp->header.rcNormal.right = 200;
    grp->header.rcNormal.bottom = 200;
    grp->header.ptMin.x = ICON_X + g_groupFileCount++ * ICON_W;
    grp->header.ptMin.y = ICON_MINIMIZED_Y;
    grp->header.wLogPixelsX = 32;
    grp->header.wLogPixelsY = 32;
    grp->header.bPlanes = 1;
    grp->header.bBitsPerPixel = 1;
    grp->header.reserved = 0;
    grp->header.cItems = 0;

    grp->dir = dir;
    grp->firstItem = NULL;
    grp->lastItem = NULL;
    grp->fileName = fileName;
    grp->title = title;
    grp->width = 1;
    grp->height = 1;
    grp->written = false;

    lua_pushvalue(L, -1);
    grp->ref = luaL_ref(L, LUA_REGISTRYINDEX);

    grp->next = g_groups;
    g_groups = grp;

    return 1;
}

static int grpfile_get(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    for (GrpFile* grp = g_groups; grp; grp = grp->next) {
        if (!strcmp(grp->title, name)) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, grp->ref);
            return 1;
        }
    }

    return luaL_error(L, "GRP file \"%s\" does not exist.", name);
}

static int grpfile_minimize(lua_State* L)
{
    GrpFile* grp = (GrpFile*)luaL_checkudata(L, 1, CLASS_GRPFILE);

    if (grp->written)
        return luaL_error(L, "GRP file already written.");

    grp->header.nCmdShow = SW_SHOWMINIMIZED;
    return 0;
}

#define USERVAL_ITEM_NAME 1
#define USERVAL_ITEM_COMMAND 2
#define USERVAL_ITEM_WORKING_DIR 3
#define USERVAL_ITEM_ICON_PATH 4

static int grpfile_add(lua_State* L)
{
    size_t nameLength, commandLength, workingDirLength, iconPathLength;

    const int grpIndex = 1;
    GrpFile* grp = (GrpFile*)luaL_checkudata(L, grpIndex, CLASS_GRPFILE);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    const int nameIndex = 4;
    const char* name = luaL_checklstring(L, nameIndex, &nameLength);
    int commandIndex = 5;
    const char* command = luaL_checklstring(L, commandIndex, &commandLength);
    int workingDirIndex = 6;
    const char* workingDir = luaL_optlstring(L, workingDirIndex, NULL, &workingDirLength);
    int iconPathIndex = 7;
    const char* iconPath = luaL_optlstring(L, iconPathIndex, NULL, &iconPathLength);

    if (grp->written)
        return luaL_error(L, "GRP file already written.");

    if (!iconPath) {
        iconPath = command;
        iconPathLength = commandLength;
    }

    if (workingDir) {
        /* PROGMAN is stupid :facepalm:
           Put <WORKING_DIR>\<EXE> into COMMAND and then put <EXE_PATH> into a TAG :facepalm: */

        const char* slash = strrchr(command, '\\');
        const char* origCommand = command;

        int n = lua_gettop(L);
        lua_pushvalue(L, workingDirIndex);
        if ((workingDirLength == 0 || workingDir[workingDirLength - 1] != '\\'))
            lua_pushliteral(L, "\\");
        lua_pushstring(L, (slash ? slash + 1 : command));
        lua_concat(L, lua_gettop(L) - n);
        commandIndex = lua_absindex(L, -1);
        command = lua_tolstring(L, commandIndex, &commandLength);

        lua_pushlstring(L, origCommand, (slash ? slash - origCommand + 1 : 0));
        workingDirIndex = lua_absindex(L, -1);
        workingDir = lua_tolstring(L, workingDirIndex, &workingDirLength);
    }

    Item* item = (Item*)lua_newuserdatauv(L, sizeof(Item), 4);

    lua_pushvalue(L, nameIndex);
    lua_setiuservalue(L, -2, USERVAL_ITEM_NAME);
    lua_pushvalue(L, commandIndex);
    lua_setiuservalue(L, -2, USERVAL_ITEM_COMMAND);
    if (workingDir) {
        lua_pushvalue(L, workingDirIndex);
        lua_setiuservalue(L, -2, USERVAL_ITEM_WORKING_DIR);
    }
    if (iconPath) {
        lua_pushvalue(L, iconPathIndex);
        lua_setiuservalue(L, -2, USERVAL_ITEM_ICON_PATH);
    }

    item->next = NULL;
    item->name = name;
    item->command = command;
    item->workingDir = workingDir;
    item->iconPath = iconPath;
    item->nameLength = nameLength + 1;
    item->commandLength = commandLength + 1;
    item->workingDirLength = workingDirLength + 1;
    item->iconPathLength = iconPathLength + 1;
    item->header.pt.x = ICON_X + ICON_W * x;
    item->header.pt.y = ICON_Y + ICON_H * y;
    item->header.iIcon = 0;
    item->header.cbResource = sizeof(item->iconHeader);
    item->header.cbANDPlane = sizeof(item->andPlane);
    item->header.cbXORPlane = sizeof(item->xorPlane);
    item->iconHeader.xHotSpot = 16;
    item->iconHeader.yHotSpot = 16;
    item->iconHeader.width = 32;
    item->iconHeader.height = 32;
    item->iconHeader.unknown = 4;
    item->iconHeader.bBitsPerPixel = 1;
    item->iconHeader.bPlanes = 1;
    memset(item->andPlane, 0, sizeof(item->andPlane));
    memset(item->xorPlane, 0, sizeof(item->xorPlane));

    if (grp->width < x + 1)
        grp->width = x + 1;
    if (grp->height < y + 1)
        grp->height = y + 1;

    if (!grp->firstItem)
        grp->firstItem = item;
    else
        grp->lastItem->next = item;
    grp->lastItem = item;

    ++grp->header.cItems;
    return 0;
}

#define bytes(buf, block) bytes_((buf), &(block), sizeof(block))
static void bytes_(luaL_Buffer* buf, const void* data, size_t size)
{
    luaL_addlstring(buf, (const char*)data, size);
}

static void u16(luaL_Buffer* buf, uint16_t value)
{
    luaL_addlstring(buf, (const char*)&value, 2);
}

static void string(luaL_Buffer* buf, const char* str)
{
    luaL_addlstring(buf, str, strlen(str) + 1);
}

static void tag(luaL_Buffer* buf, uint16_t wID, int16_t item, const char* str)
{
    TAGDATA tag;
    tag.wID = wID;
    tag.wItem = item;
    tag.cb = (str ? strlen(str) + (!strcmp(str, "PMCC") ? 0 : 1) + sizeof(tag) : 0);

    bytes(buf, tag);
    if (str)
        luaL_addlstring(buf, str, tag.cb - sizeof(tag));
}

static int grpfile_write(lua_State* L)
{
    GrpFile* grp = (GrpFile*)luaL_checkudata(L, 1, CLASS_GRPFILE);
    uint16_t cbGroup, itemOffsets, pName;

    grp->written = true;

    grp->header.rcNormal.right = grp->header.rcNormal.left + ICON_X + ICON_W * grp->width + WINDOW_RIGHT_PADDING;
    grp->header.rcNormal.bottom = grp->header.rcNormal.top + ICON_Y + ICON_H * grp->height + WINDOW_BOTTOM_PADDING;

    if (grp->header.cItems == 0)
        grp->header.cItems = 1;

    luaL_Buffer buf;
    void* data;
    size_t dataSize;
    {
        luaL_buffinit(L, &buf);

        /* header */
        bytes(&buf, grp->header);

        /* item offsets */
        itemOffsets = (uint16_t)buf.n;
        for (size_t i = 0; i < grp->header.cItems; i++)
            u16(&buf, 0); /* patched later */

        /* group name */
        pName = (uint16_t)buf.n;
        string(&buf, grp->title);

        /* items */
        for (Item* p = grp->firstItem; p; p = p->next) {
            p->offset = (uint16_t)buf.n;
            bytes(&buf, p->header);
            p->header.pName = (uint16_t)buf.n;
            bytes_(&buf, p->name, p->nameLength);
            p->header.pCommand = (uint16_t)buf.n;
            bytes_(&buf, p->command, p->commandLength);
            p->header.pIconPath = (uint16_t)buf.n;
            bytes_(&buf, p->iconPath, p->iconPathLength);
        }

        /* icons */
        for (Item* p = grp->firstItem; p; p = p->next) {
            p->header.pHeader = (uint16_t)buf.n;
            bytes(&buf, p->iconHeader);
            p->header.pANDPlane = (uint16_t)buf.n;
            bytes(&buf, p->andPlane);
            p->header.pXORPlane = (uint16_t)buf.n;
            bytes(&buf, p->xorPlane);
        }

        /* tags */
        cbGroup = (uint16_t)buf.n;
        tag(&buf, 0x8000, -1, "PMCC");  /* header */
        int16_t index = 0;
        for (Item* p = grp->firstItem; p; p = p->next, ++index) {
            if (p->workingDir)
                tag(&buf, 0x8101, index, p->workingDir);
        }
        tag(&buf, -1, -1, NULL);        /* footer */

        luaL_pushresult(&buf);
        data = (void*)luaL_tolstring(L, -1, &dataSize);
    }

    GROUPHEADER* hdr = (GROUPHEADER*)data;
    hdr->pName = pName;
    hdr->cbGroup = cbGroup;

    uint16_t* pOffsets = (uint16_t*)((char*)data + itemOffsets);
    for (Item* p = grp->firstItem; p; p = p->next) {
        *pOffsets++ = p->offset;
        ITEMDATA* pHeader = (ITEMDATA*)((char*)data + p->offset);
        pHeader->pHeader = p->header.pHeader;
        pHeader->pANDPlane = p->header.pANDPlane;
        pHeader->pXORPlane = p->header.pXORPlane;
        pHeader->pName = p->header.pName;
        pHeader->pCommand = p->header.pCommand;
        pHeader->pIconPath = p->header.pIconPath;
    }

    const uint8_t* p = (const uint8_t*)data;
    const uint8_t* pEnd = p + dataSize;
    uint16_t sum = 0;
    while (p < pEnd) {
        uint16_t value = *p++;
        if (p < pEnd)
            value = (uint16_t)(value | (*p++ << 8));
        sum += value;
    }
    hdr->wCheckSum = 0 - sum;

    MkDisk_AddFileContent(grp->dir->disk, grp->dir, grp->fileName, data, dataSize);
    return 0;
}

static const luaL_Reg grpfile_funcs[] = {
    { "minimize", grpfile_minimize },
    { "add", grpfile_add },
    { "write", grpfile_write },
    { NULL, NULL }
};

static const luaL_Reg funcs[] = {
    { "create", grpfile_create },
    { "get", grpfile_get },
    { NULL, NULL }
};

static int grpfile_tostring(lua_State* L)
{
    const Disk* dsk = (Disk*)luaL_checkudata(L, 1, CLASS_GRPFILE);
    DONT_WARN_UNUSED(dsk);
    lua_pushstring(L, "<GrpFile*>");
    return 1;
}

int luaopen_grpfile(lua_State* L)
{
    luaL_newmetatable(L, CLASS_GRPFILE);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, grpfile_tostring);
    lua_setfield(L, -2, "__tostring");
    luaL_setfuncs(L, grpfile_funcs, 0);

    luaL_newlib(L, funcs);
    return 1;
}

void GrpFile_InitLua(lua_State* L)
{
    luaL_requiref(L, "wingrp", luaopen_grpfile, 1);
    lua_pop(L, 1);
}

void GrpFile_WriteAllForDisk(Disk* disk)
{
    lua_State* L = disk->L;
    for (GrpFile* grp = g_groups; grp; grp = grp->next) {
        if (grp->dir->disk == disk && !grp->written) {
            lua_pushcfunction(L, grpfile_write);
            lua_rawgeti(L, LUA_REGISTRYINDEX, grp->ref);
            lua_call(L, 1, 0);
        }
    }
}
