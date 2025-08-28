#ifndef MKDISK_MBR_H
#define MKDISK_MBR_H

#include <mkdisk/mkdisk.h>

#define MBR_DISK_START (disk_config->mbr_disk_start) // in sectors
#define MBR_DISK_SIZE (disk_config->mbr_disk_size) // in sectors

void MBR_Init(Disk* dsk, void* dst);

#endif
