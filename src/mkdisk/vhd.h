#ifndef MKDISK_VHD_H
#define MKDISK_VHD_H

#include <mkdisk/mkdisk.h>

void VHD_Init(Disk* dsk);
uint8_t* VHD_Sector(Disk* dsk, size_t index);
void VHD_WriteSectors(Disk* dsk, size_t firstSector, const void* data, size_t size);
void VHD_Write(Disk* dsk, const char* file);
void VHD_WriteAsIMG(Disk* dsk, const char* file, bool includeMBR);

#endif
