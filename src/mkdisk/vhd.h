#ifndef MKDISK_VHD_H
#define MKDISK_VHD_H

#include <common/common.h>

void vhd_init(void);
uint8_t* vhd_sector(size_t index);
void vhd_write_sectors(size_t firstSector, const void* data, size_t size);
void vhd_write(const char* file);
void vhd_write_as_img(const char* file, bool includeMBR);

#endif
