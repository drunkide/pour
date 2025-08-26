#include <common/common.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <lua.h>
#include <mkdisk/disk_config.h>
#include <mkdisk/mkdisk.h>
#include <mkdisk/fat.h>
#include <mkdisk/vhd.h>
#include <mkdisk/mbr.h>
#include <mkdisk/fat_defs.h>
#include <mkdisk/mbr_defs.h>
#include <mkdisk/vhd_defs.h>

#define SECTORS_PER_CLUSTER (disk_config->fat_sectors_per_cluster)
#define FAT_SIZE (MBR_DISK_SIZE / SECTORS_PER_CLUSTER)
#define SECTORS_PER_FAT (FAT_SIZE * sizeof(uint16_t) + SECTOR_SIZE - 1) / SECTOR_SIZE

#define CLUSTER_SIZE (SECTOR_SIZE * SECTORS_PER_CLUSTER)

#define MAX_DIR_ENTRIES 16384

struct dir {
    struct dir* next;
    struct dir* parent;
    size_t parentIndex;
    size_t entryCount;
    uint16_t cluster;
    fat_direntry entries[MAX_DIR_ENTRIES];
};

bool fat_enable_lfn;

static size_t fatSize;
static uint16_t* fat;
static dir root_dir;
static dir* last_dir;

void fat_init(const uint8_t* bootCode)
{
    if (sizeof(fat_bootsector) != VHD_SECTOR_SIZE) {
        fprintf(stderr, "invalid boot sector size (%lu) - expected %lu.\n",
            (unsigned long)sizeof(fat_bootsector), (unsigned long)VHD_SECTOR_SIZE);
        exit(1);
    }

    fatSize = FAT_SIZE;
    fat = calloc(sizeof(uint16_t), fatSize);
    if (!fat) {
        fprintf(stderr, "memory allocation failed.\n");
        exit(1);
    }

    fat_bootsector* p = (fat_bootsector*)vhd_sector(MBR_DISK_START);
    if (bootCode) {
        p->jump[0] = bootCode[0];
        p->jump[1] = bootCode[1];
        p->jump[2] = bootCode[2];
    }
    memcpy(p->oem, "MSDOS5.0", 8);
    p->bytesPerSector = SECTOR_SIZE;
    p->sectorsPerCluster = SECTORS_PER_CLUSTER;
    p->reservedSectors = 1;
    p->fatCount = 2;
    p->maxRootDirEntries = MAX_ROOT_DIR_ENTRIES;
    p->totalSectorsOld = 0;
    p->mediaDescriptor = 0xF8; // fixed disk
    p->sectorsPerFat = SECTORS_PER_FAT;
    p->sectorsPerTrack = 0x3F;
    p->headCount = disk_config->fat_head_count;
    p->hiddenSectors = MBR_DISK_START;
    p->totalSectors = MBR_DISK_SIZE;
    p->physicalDriveNumber = 0x80; // first HDD
    p->extendedBootSignature = 0x29;
    memcpy(p->serial, "\x9E\xE9\x11\x00", 4);
    memcpy(p->volumeLabel, "NO NAME    ", 11);
    memcpy(p->fileSystem, "FAT16   ", 8);
    memcpy(p->code, bootCode + 3, BOOT_CODE_SIZE);
    p->signature = SIGNATURE;

    fat[0] = 0xFFF8; // FAT ID
    fat[1] = 0xFFFF;

    memset(&root_dir, 0, sizeof(root_dir));
    last_dir = &root_dir;
}

dir* fat_root_directory()
{
    return &root_dir;
}

#define IS_LFN(ch) \
    ((ch) == ' ' || \
     (ch) == '+' || \
     (ch) == ',' || \
     (ch) == ';' || \
    0)

static void set_str(char* dst, size_t maxLen, const char* src, size_t srcLen)
{
    if (srcLen >= maxLen)
        srcLen = maxLen;

    size_t i;
    for (i = 0; i < srcLen; i++) {
        char ch = *src++;
        if (IS_LFN(ch) || ch == '.')
            *dst++ = '~';
        else
            *dst++ = toupper(ch);
    }
    for (; i < maxLen; i++)
        *dst++ = ' ';
}

/*
static bool is_lfn(const char* name)
{
    const char* ext = strchr(name, '.');

    size_t nameLen = (ext ? (size_t)(ext - name) : strlen(name));
    if (nameLen > 8)
        return true;

    size_t extLen = (ext ? strlen(++ext) : 0);
    if (extLen > 3)
        return true;

    for (const char* p = name; *p; p++) {
        if (IS_LFN(*p) || (*p == '.' && p + 1 != ext))
            return true;
    }

    return false;
}
*/

static void set_name(fat_direntry* d, const char* name)
{
    const char* ext = strchr(name, '.');

    size_t nameLen = (ext ? (size_t)(ext - name) : strlen(name));
    set_str(d->name, 8, name, nameLen);

    size_t extLen = (ext ? strlen(++ext) : 0);
    set_str(d->ext, 3, ext, extLen);
}

void fat_normalize_name(char* dst, const char* name)
{
    fat_direntry d;
    set_name(&d, name);

    const char* p = d.name;
    for (int i = 0; i < 8; i++) {
        if (*p == ' ')
            break;
        *dst++ = *p++;
    }

    if (d.ext[0] != ' ') {
        p = d.ext;
        *dst++ = '.';
        *dst++ = *p++;
        for (int i = 1; i < 3; i++) {
            if (*p == ' ')
                break;
            *dst++ = *p++;
        }
    }

    *dst = 0;
}

static bool fat_write_lfn(dir* parent, const char* name)
{
    if (!fat_enable_lfn)// || !is_lfn(name))
        return false;

    fat_direntry shrt;
    set_name(&shrt, name);
    uint8_t sum = 0;
    for (int i = 0; i < 8; i++)
        sum = (((sum & 1) ? 0x80 : 0) | (sum >> 1)) + shrt.name[i];
    for (int i = 0; i < 3; i++)
        sum = (((sum & 1) ? 0x80 : 0) | (sum >> 1)) + shrt.ext[i];

    size_t nameLen = strlen(name) + 1;
    size_t numEntries = (nameLen + 12) / 13;

    if (parent->entryCount + numEntries >= MAX_DIR_ENTRIES) {
        fprintf(stderr, "too many directory entries!\n");
        exit(1);
    }

    size_t startIndex = parent->entryCount;
    size_t index = startIndex + numEntries;
    parent->entryCount = index;

    const char* p = name;
    const char* pEnd = name + nameLen;
    uint8_t order = 1;
    while (p < pEnd) {
        if (--index == startIndex)
            order |= 0x40;
        parent->entries[index].name[0] = order++;
        parent->entries[index].name[1] = (p < pEnd ?     *p : 0xff);
        parent->entries[index].name[2] = (p < pEnd ? ++p, 0 : 0xff);
        parent->entries[index].name[3] = (p < pEnd ?     *p : 0xff);
        parent->entries[index].name[4] = (p < pEnd ? ++p, 0 : 0xff);
        parent->entries[index].name[5] = (p < pEnd ?     *p : 0xff);
        parent->entries[index].name[6] = (p < pEnd ? ++p, 0 : 0xff);
        parent->entries[index].name[7] = (p < pEnd ?     *p : 0xff);
        parent->entries[index].ext[0] =  (p < pEnd ? ++p, 0 : 0xff);
        parent->entries[index].ext[1] =  (p < pEnd ?     *p : 0xff);
        parent->entries[index].ext[2] =  (p < pEnd ? ++p, 0 : 0xff);
        parent->entries[index].attrib = 0x0F;
        parent->entries[index].reserved[0] = 0;
        parent->entries[index].reserved[1] = sum;
        parent->entries[index].reserved[2] = (p < pEnd ?     *p : 0xff);
        parent->entries[index].reserved[3] = (p < pEnd ? ++p, 0 : 0xff);
        parent->entries[index].reserved[4] = (p < pEnd ?     *p : 0xff);
        parent->entries[index].reserved[5] = (p < pEnd ? ++p, 0 : 0xff);
        parent->entries[index].reserved[6] = (p < pEnd ?     *p : 0xff);
        parent->entries[index].reserved[7] = (p < pEnd ? ++p, 0 : 0xff);
        parent->entries[index].reserved[8] = (p < pEnd ?     *p : 0xff);
        parent->entries[index].reserved[9] = (p < pEnd ? ++p, 0 : 0xff);
        uint32_t ch1 = (p < pEnd ?     *p : 0xff);
        uint32_t ch2 = (p < pEnd ? ++p, 0 : 0xff);
        uint32_t ch3 = (p < pEnd ?     *p : 0xff);
        uint32_t ch4 = (p < pEnd ? ++p, 0 : 0xff);
        parent->entries[index].lastModified = ch1 | (ch2 << 8) | (ch3 << 16) | (ch4 << 24);
        parent->entries[index].firstCluster = 0;
        ch1 = (p < pEnd ?     *p : 0xff);
        ch2 = (p < pEnd ? ++p, 0 : 0xff);
        ch3 = (p < pEnd ?     *p : 0xff);
        ch4 = (p < pEnd ? ++p, 0 : 0xff);
        parent->entries[index].size = ch1 | (ch2 << 8) | (ch3 << 16) | (ch4 << 24);
    }

    assert(order == (numEntries | 0x40) + 1);

    return true;
}

dir* fat_create_directory(dir* parent, const char* name)
{
    dir* d = (dir*)calloc(1, sizeof(dir));
    if (!d) {
        fprintf(stderr, "memory allocation failed.\n");
        exit(1);
    }

    if (parent->entryCount >= MAX_DIR_ENTRIES) {
        fprintf(stderr, "too many directory entries!\n");
        exit(1);
    }

    d->entryCount = 2;
    memcpy(d->entries[0].name, ".       ", 8);
    memcpy(d->entries[0].ext, "   ", 3);
    d->entries[0].attrib = ATTR_DIR;
    memcpy(d->entries[1].name, "..      ", 8);
    memcpy(d->entries[1].ext, "   ", 3);
    d->entries[1].attrib = ATTR_DIR;

    fat_write_lfn(parent, name);

    d->parent = parent;
    d->parentIndex = parent->entryCount++;
    set_name(&parent->entries[d->parentIndex], name);
    parent->entries[d->parentIndex].attrib = ATTR_DIR;

    last_dir->next = d;
    last_dir = d;

    return d;
}

static uint16_t alloc_cluster(uint16_t start)
{
    for (size_t i = start; i < FAT_SIZE; i++) {
        if (fat[i] == 0)
            return (uint16_t)i;
    }

    fprintf(stderr, "Output full!\n");
    exit(1);
}

static void alloc_file(uint16_t cluster, size_t size)
{
    for (;;) {
        size_t srcSize = (size > (size_t)CLUSTER_SIZE ? (size_t)CLUSTER_SIZE : size);

        fat[cluster] = 0xffff;

        size -= srcSize;
        if (size == 0)
            break;

        uint16_t nextCluster = alloc_cluster(cluster);
        fat[cluster] = nextCluster;
        cluster = nextCluster;
    }
}

static void write_file(uint16_t cluster, const void* data, size_t size)
{
    const uint8_t* src = (const uint8_t*)data;

    if (size > 0) {
        for (;;) {
            size_t srcSize = (size > (size_t)CLUSTER_SIZE ? (size_t)CLUSTER_SIZE : size);

            vhd_write_sectors(MBR_DISK_START + 1 + SECTORS_PER_FAT * 2 + ROOT_DIR_SECTORS +
                (cluster - 2) * SECTORS_PER_CLUSTER, src, srcSize);

            size -= srcSize;
            src += CLUSTER_SIZE;

            if (size == 0)
                break;

            cluster = fat[cluster];
        }
    }

    if (fat[cluster] != 0xffff) {
        fprintf(stderr, "sanity check failed: invalid calculation of FAT chain.\n");
        exit(1);
    }
}

void fat_add_file(dir* parent, const char* name, const void* data, size_t size)
{
    if (parent->entryCount >= MAX_DIR_ENTRIES) {
        fprintf(stderr, "too many directory entries!\n");
        exit(1);
    }

    uint16_t cluster;
    if (size == 0)
        cluster = 0;
    else {
        cluster = alloc_cluster(2);
        alloc_file(cluster, size);
        write_file(cluster, data, size);
    }

    fat_write_lfn(parent, name);

    size_t index = parent->entryCount++;
    set_name(&parent->entries[index], name);
    parent->entries[index].attrib = ATTR_ARCHIVE;
    parent->entries[index].firstCluster = cluster;
    parent->entries[index].size = size;
}

void fat_write()
{
    for (dir* p = root_dir.next; p; p = p->next) {
        p->cluster = alloc_cluster(2);
        p->entries[0].firstCluster = p->cluster;
        p->parent->entries[p->parentIndex].firstCluster = p->cluster;
        alloc_file(p->cluster, p->entryCount * sizeof(fat_direntry));
    }

    for (dir* p = root_dir.next; p; p = p->next) {
        p->entries[1].firstCluster = p->parent->cluster;
        write_file(p->cluster, p->entries, p->entryCount * sizeof(fat_direntry));
    }

    vhd_write_sectors(MBR_DISK_START + 1, fat, fatSize * sizeof(uint16_t));
    vhd_write_sectors(MBR_DISK_START + 1 + SECTORS_PER_FAT, fat, fatSize * sizeof(uint16_t));
    vhd_write_sectors(MBR_DISK_START + 1 + SECTORS_PER_FAT * 2, root_dir.entries, ROOT_DIR_SIZE);
}
