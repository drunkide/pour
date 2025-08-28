#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include "mkdisk.h"
#include "fat.h"
#include "ext2.h"
#include "ext2_defs.h"
#include "mkdisk.h"

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

static int g_groupFileCount;
static GROUPHEADER g_header;
static Item* g_firstItem;
static Item* g_lastItem;
static int g_width;
static int g_height;
static const char* g_title;
static bool g_wasCreated;

#define ICON_X 21
#define ICON_Y 0
#define ICON_W 75
#define ICON_H 72

#define WINDOW_RIGHT_PADDING -8
#define WINDOW_BOTTOM_PADDING 8

#define ICON_MINIMIZED_Y 253

const char grpfile_extra;
static bool extra_executed;

void grpfile_exec_extra(lua_State* L)
{
    if (extra_executed)
        return;
    extra_executed = true;

    lua_rawgetp(L, LUA_REGISTRYINDEX, &grpfile_extra);
    if (luaL_loadstring(L, lua_tostring(L, -1)) != 0)
        luaL_error(L, "syntax error in expression: %s", lua_tostring(L, -1));
    if (lua_pcall(L, 0, 0, 0))
        luaL_error(L, "error evaluating expression: %s", lua_tostring(L, -1));
}

static int grpfile_create(lua_State* L)
{
    const char* title = luaL_checkstring(L, 1);

    memcpy(g_header.cIdentifier, "PMCC", 4);
    g_header.wCheckSum = 0;
    g_header.nCmdShow = SW_SHOWNORMAL;
    g_header.rcNormal.left = 100;
    g_header.rcNormal.top = 100;
    g_header.rcNormal.right = 200;
    g_header.rcNormal.bottom = 200;
    g_header.ptMin.x = ICON_X + g_groupFileCount++ * ICON_W;
    g_header.ptMin.y = ICON_MINIMIZED_Y;
    g_header.wLogPixelsX = 32;
    g_header.wLogPixelsY = 32;
    g_header.bPlanes = 1;
    g_header.bBitsPerPixel = 1;
    g_header.reserved = 0;
    g_header.cItems = 0;

    g_title = title;
    lua_pushvalue(L, 1);
    lua_rawsetp(L, LUA_REGISTRYINDEX, g_title);

    g_width = 1;
    g_height = 1;
    g_firstItem = NULL;
    g_lastItem = NULL;

    return 1;
}

static int grpfile_minimized(lua_State* L)
{
    g_header.nCmdShow = SW_SHOWMINIMIZED;
    return 1;
}

static int grpfile_add(lua_State* L)
{
    size_t nameLength, commandLength, workingDirLength, iconPathLength;

    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    const char* name = luaL_checklstring(L, 3, &nameLength);
    const char* command = luaL_checklstring(L, 4, &commandLength);
    const char* workingDir = luaL_optlstring(L, 5, NULL, &workingDirLength);
    const char* iconPath = luaL_optlstring(L, 6, NULL, &iconPathLength);

    if (!iconPath) {
        iconPath = command;
        iconPathLength = commandLength;
    }

    int commandIndex = 4;
    int workingDirIndex = 5;
    if (workingDir) {
        // PROGMAN is stupid :facepalm:
        // Put <WORKING_DIR>\<EXE> into COMMAND and then put <EXE_PATH> into a TAG :facepalm:

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

    Item* item = (Item*)lua_newuserdata(L, sizeof(Item));
    lua_rawsetp(L, LUA_REGISTRYINDEX, item);

    lua_pushvalue(L, 3); lua_rawsetp(L, LUA_REGISTRYINDEX, name);
    lua_pushvalue(L, commandIndex); lua_rawsetp(L, LUA_REGISTRYINDEX, command);
    if (workingDir)
        lua_pushvalue(L, workingDirIndex), lua_rawsetp(L, LUA_REGISTRYINDEX, workingDir);
    if (iconPath)
        lua_pushvalue(L, 6), lua_rawsetp(L, LUA_REGISTRYINDEX, iconPath);

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

    if (g_width < x + 1)
        g_width = x + 1;
    if (g_height < y + 1)
        g_height = y + 1;

    if (!g_firstItem)
        g_firstItem = item;
    else
        g_lastItem->next = item;
    g_lastItem = item;

    ++g_header.cItems;
    return 0;
}

static luaL_Buffer buf;

#define bytes(block) bytes_(&(block), sizeof(block))
static void bytes_(const void* data, size_t size)
{
    luaL_addlstring(&buf, (const char*)data, size);
}

static void u16(uint16_t value)
{
    luaL_addlstring(&buf, (const char*)&value, 2);
}

static void string(const char* str)
{
    luaL_addlstring(&buf, str, strlen(str) + 1);
}

static void tag(uint16_t wID, int16_t item, const char* str)
{
    TAGDATA tag;
    tag.wID = wID;
    tag.wItem = item;
    tag.cb = (str ? strlen(str) + (!strcmp(str, "PMCC") ? 0 : 1) + sizeof(tag) : 0);

    bytes(tag);
    if (str)
        luaL_addlstring(&buf, str, tag.cb - sizeof(tag));
}

static int grpfile_write(lua_State* L)
{
    lua_dir* parent = get_directory(L, 1);
    const char* name = luaL_checkstring(L, 2);
    uint16_t cbGroup, itemOffsets, pName;

    g_header.rcNormal.right = g_header.rcNormal.left + ICON_X + ICON_W * g_width + WINDOW_RIGHT_PADDING;
    g_header.rcNormal.bottom = g_header.rcNormal.top + ICON_Y + ICON_H * g_height + WINDOW_BOTTOM_PADDING;

    if (g_header.cItems == 0)
        g_header.cItems = 1;

    void* data;
    size_t dataSize;
    {
        luaL_buffinit(L, &buf);

        // header
        bytes(g_header);

        // item offsets
        itemOffsets = (uint16_t)buf.n;
        for (size_t i = 0; i < g_header.cItems; i++)
            u16(0); // patched later

        // group name
        pName = (uint16_t)buf.n;
        string(g_title);

        // items
        for (Item* p = g_firstItem; p; p = p->next) {
            p->offset = (uint16_t)buf.n;
            bytes(p->header);
            p->header.pName = (uint16_t)buf.n;
            bytes_(p->name, p->nameLength);
            p->header.pCommand = (uint16_t)buf.n;
            bytes_(p->command, p->commandLength);
            p->header.pIconPath = (uint16_t)buf.n;
            bytes_(p->iconPath, p->iconPathLength);
        }

        // icons
        for (Item* p = g_firstItem; p; p = p->next) {
            p->header.pHeader = (uint16_t)buf.n;
            bytes(p->iconHeader);
            p->header.pANDPlane = (uint16_t)buf.n;
            bytes(p->andPlane);
            p->header.pXORPlane = (uint16_t)buf.n;
            bytes(p->xorPlane);
        }

        // tags
        cbGroup = (uint16_t)buf.n;
        tag(0x8000, -1, "PMCC");    // header
        int16_t index = 0;
        for (Item* p = g_firstItem; p; p = p->next, ++index) {
            if (p->workingDir)
                tag(0x8101, index, p->workingDir);
        }
        tag(-1, -1, NULL);          // footer

        luaL_pushresult(&buf);
        data = (void*)luaL_tolstring(L, -1, &dataSize);
    }

    GROUPHEADER* hdr = (GROUPHEADER*)data;
    hdr->pName = pName;
    hdr->cbGroup = cbGroup;

    uint16_t* pOffsets = (uint16_t*)((char*)data + itemOffsets);
    for (Item* p = g_firstItem; p; p = p->next) {
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

    if (!g_use_ext2)
        fat_add_file(parent->dir, name, data, dataSize);
    else {
        ext2_meta meta;
        meta.type_and_perm = EXT2_TYPE_FILE | 0644;
        meta.uid = 0;
        meta.gid = 0;

        ext2_add_file(parent->dir, name, data, dataSize, &meta);
    }

    printf("\n=> %s%s\n", parent->path, name);
    g_wasCreated = true;

    return 0;
}

static int grpfile_was_created(lua_State* L)
{
    grpfile_exec_extra(L);
    lua_pushboolean(L, g_wasCreated);
    return 1;
}

static const luaL_Reg funcs[] = {
    { "create", grpfile_create },
    { "minimized", grpfile_minimized },
    { "add", grpfile_add },
    { "write", grpfile_write },
    { "was_created", grpfile_was_created },
    { NULL, NULL }
};

// called from linit.c
int luaopen_grpfile(lua_State* L)
{
    luaL_newlib(L, funcs);
    return 1;
}
