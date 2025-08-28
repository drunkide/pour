#include <common/common.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <mkdisk/disk_config.h>
#include <mkdisk/vhd.h>
#include <mkdisk/vhd_defs.h>
#include <mkdisk/write.h>
#include <common/byteswap.h>

static vhd_footer footer;
static vhd_dynhdr dynhdr;
static uint32_t* bat;
static size_t batSize;
static uint32_t nextOffset;
static vhd_blockchain** blocks;
static vhd_blockchain* first;
static vhd_blockchain* last;

static uint32_t checksum(const void* data, size_t size)
{
    const uint8_t* p = (const uint8_t*)data;
    uint32_t checksum = 0;
    while (size-- > 0)
        checksum += *p++;
    return MSB32(~checksum);
}

void vhd_init()
{
    nextOffset = disk_config->vhd_next_offset;

    first = NULL;
    last = NULL;

    batSize = sizeof(uint32_t) *
        ((disk_config->vhd_bat_size + BAT_ENTRIES_PER_SECTOR - 1) / BAT_ENTRIES_PER_SECTOR * BAT_ENTRIES_PER_SECTOR);
    bat = malloc(batSize);
    blocks = calloc(sizeof(vhd_blockchain*), disk_config->vhd_bat_size);
    if (!bat || !blocks) {
        fprintf(stderr, "memory allocation failed.\n");
        exit(1);
    }

    memcpy(footer.cookie, "conectix", 8);
    footer.features = MSB32(0x00000002); // reserved: this bit must always be set to 1
    footer.version = MSB32(0x00010000);
    footer.dataOffset = MSB64(0x200);
    footer.timestamp = MSB32(0x2FFE800E);
    memcpy(footer.creator, "DBox", 4);
    footer.creatorVersion = MSB32(0x00010000);
    memcpy(footer.creatorHostOS, "Wi2k", 4);
    footer.originalSize = MSB64(disk_config->vhd_size);
    footer.currentSize = MSB64(disk_config->vhd_size);
    footer.numCylinders = MSB16(disk_config->vhd_cylinders);
    footer.numHeads = disk_config->vhd_heads;
    footer.sectorsPerTrack = disk_config->vhd_sectors_per_track;
    footer.diskType = MSB32(3); // Dynamic Hard Disk
    footer.checksum = 0;
    memcpy(footer.uniqueId, "\x57\x0D\x19\x2A\xF7\x7F\xDF\x4D\xCE\x39\x7E\x68\xA5\x6A\x7A\x48", 16);
    footer.savedState = 0;

    memcpy(dynhdr.cookie, "cxsparse", 8);
    dynhdr.dataOffset = MSB64(0xFFFFFFFFFFFFFFFF);
    dynhdr.tableOffset = MSB64(0x600);
    dynhdr.headerVersion = MSB32(0x00010000);
    dynhdr.maxTableEntries = MSB32(disk_config->vhd_bat_size);
    dynhdr.blockSize = MSB32(VHD_BLOCK_SIZE);
    dynhdr.checksum = 0;

    footer.checksum = checksum(&footer, sizeof(footer));
    dynhdr.checksum = checksum(&dynhdr, sizeof(dynhdr));

    memset(bat, 0xFF, batSize);
}

uint8_t* vhd_sector(size_t index)
{
    size_t blockIndex = index / VHD_SECTORS_PER_BLOCK;
    size_t sectorIndex = index % VHD_SECTORS_PER_BLOCK;

    if (blockIndex >= (size_t)(disk_config->vhd_bat_size)) {
        fprintf(stderr, "blockIndex (%lu) is out of range (%lu)\n",
            (unsigned long)blockIndex, (unsigned long)disk_config->vhd_bat_size);
        exit(1);
    }

    vhd_blockchain* block = blocks[blockIndex];
    if (!block) {
        block = (vhd_blockchain*)calloc(1, sizeof(vhd_blockchain));
        if (!block) {
            fprintf(stderr, "memory allocation failed.\n");
            exit(1);
        }

        //memset(block->data.bitmap, 0, sizeof(block->data.bitmap));
        blocks[blockIndex] = block;
        bat[blockIndex] = MSB32(nextOffset);
        block->offset = nextOffset * VHD_SECTOR_SIZE;
        nextOffset += sizeof(vhd_block) / VHD_SECTOR_SIZE;

        if (!first)
            first = block;
        else
            last->next = block;
        last = block;
    }

    size_t byteIndex = sectorIndex >> 3;
    size_t bitIndex = sectorIndex & 7;
    static const uint8_t mask[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
    block->data.bitmap[byteIndex] |= mask[bitIndex];

    return &block->data.data[sectorIndex * VHD_SECTOR_SIZE];
}

static const uint8_t zeros[VHD_SECTOR_SIZE];

const uint8_t* vhd_read_sector(size_t index)
{
    size_t blockIndex = index / VHD_SECTORS_PER_BLOCK;
    size_t sectorIndex = index % VHD_SECTORS_PER_BLOCK;

    vhd_blockchain* block = blocks[blockIndex];
    if (!block)
        return zeros;

    return &block->data.data[sectorIndex * VHD_SECTOR_SIZE];
}

void vhd_write_sectors(size_t firstSector, const void* data, size_t size)
{
    const uint8_t* src = (const uint8_t*)data;

    while (size != 0) {
        size_t srcSize = (size > VHD_SECTOR_SIZE ? VHD_SECTOR_SIZE : size);
        if (memcmp(src, zeros, srcSize) != 0) {
            uint8_t* dst = vhd_sector(firstSector);
            memcpy(dst, src, srcSize);
        }

        ++firstSector;
        src += VHD_SECTOR_SIZE;
        size -= srcSize;
    }
}

void VHD_Write(lua_State* L, const char* file)
{
    size_t fileSize = sizeof(footer) + sizeof(dynhdr) + batSize;
    for (vhd_blockchain* block = first; block; block = block->next)
        fileSize += sizeof(vhd_block);
    fileSize += sizeof(footer);

    Write* wr = Write_Begin(L, file, fileSize);
    Write_Append(wr, &footer, sizeof(footer));
    Write_Append(wr, &dynhdr, sizeof(dynhdr));
    Write_Append(wr, bat, batSize);
    for (vhd_blockchain* block = first; block; block = block->next) {
        if (block->offset != Write_GetCurrentOffset(wr))
            luaL_error(L, "VHD: block offset mismatch!");
        Write_Append(wr, &block->data, sizeof(vhd_block));
    }
    Write_Append(wr, &footer, sizeof(footer));

    if (Write_GetCurrentOffset(wr) != fileSize)
        luaL_error(L, "VHD: calculated file size mismatch!");

    Write_Commit(wr);
}

void VHD_WriteAsIMG(lua_State* L, const char* file, bool includeMBR)
{
    size_t fileSize = (includeMBR ? disk_config->vhd_size : disk_config->mbr_disk_size * VHD_SECTOR_SIZE);

    Write* wr = Write_Begin(L, file, fileSize);
    for (size_t i = 0; i < fileSize / VHD_SECTOR_SIZE; i++) {
        const uint8_t* p = vhd_read_sector(i + (includeMBR ? 0 : disk_config->mbr_disk_start));
        Write_Append(wr, p, VHD_SECTOR_SIZE);
    }

    if (Write_GetCurrentOffset(wr) != fileSize)
        luaL_error(L, "VHD: calculated file size mismatch!");

    Write_Commit(wr);
}
