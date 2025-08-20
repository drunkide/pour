#include <common/file.h>
#include <sys/types.h>
#include <sys/stat.h>

bool File_Exists(const char* path)
{
    struct stat st;
    return stat(path, &st) == 0;
}
