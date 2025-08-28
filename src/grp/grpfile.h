#ifndef GRP_GRPFILE_H
#define GRP_GRPFILE_H

#include <mkdisk/mkdisk.h>

void GrpFile_InitLua(lua_State* L);
void GrpFile_WriteAllForDisk(Disk* disk);

#endif
