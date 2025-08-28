#include <common/common.h>
#include <common/console.h>
#include <common/file.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mkdisk/write.h>

struct Write
{
    lua_State* L;
    const char* fileName;
    char* dst;
    const char* dstEnd;
    char* oldBuffer;
    char* newBuffer;
    size_t oldSize;
    size_t newSize;
    size_t indicatorTotal;
    size_t indicatorTotalWritten;
    size_t indicatorTotalSkipped;
    size_t totalSkipped;
    size_t totalWritten;
};

/****************************************************************************/

#define INDICATOR_STEP (1048576/2)

static void indicator_begin(Write* wr)
{
    wr->totalSkipped = 0;
    wr->totalWritten = 0;
    wr->indicatorTotal = 0;
    wr->indicatorTotalWritten = 0;
    wr->indicatorTotalSkipped = 0;
}

static void indicator_write_char(Write* wr)
{
    lua_State* L = wr->L;
    static const size_t indicLen = 6;
    static const char indic[] = ".~*xX#";
    char ch;

    if (wr->indicatorTotalWritten == 0)
        ch = indic[0];
    else {
        double percent = (wr->indicatorTotalWritten * 1.0 / wr->indicatorTotal);
        size_t index = 1 + (size_t)(percent * (indicLen - 1));
        if (index >= indicLen)
            index = indicLen - 1;
        ch = indic[index];
    }

    Con_PrintF(L, COLOR_PROGRESS, "%c", ch);

    #define DECREASE(x) (x = (x > INDICATOR_STEP ? x - INDICATOR_STEP : 0))
    DECREASE(wr->indicatorTotal);
    DECREASE(wr->indicatorTotalWritten);
    DECREASE(wr->indicatorTotalSkipped);
}

static void indicator_add(Write* wr, size_t skipped, size_t written)
{
    lua_State* L = wr->L;

    wr->totalSkipped += skipped;
    wr->totalWritten += written;

    wr->indicatorTotal += skipped + written;
    wr->indicatorTotalSkipped += skipped;
    wr->indicatorTotalWritten += written;

    if (wr->indicatorTotal >= INDICATOR_STEP) {
        do {
            indicator_write_char(wr);
        } while (wr->indicatorTotal >= INDICATOR_STEP);
        Con_Flush(L);
    }
}

static void indicator_end(Write* wr)
{
    if (wr->indicatorTotal != 0)
        indicator_write_char(wr);
}

/****************************************************************************/

static bool try_load_existing_file(Write* wr)
{
    lua_State* L = wr->L;

    wr->oldSize = 0;
    wr->oldBuffer = NULL;

    if (!File_Exists(L, wr->fileName))
        return false;

    wr->oldBuffer = File_PushContents(L, wr->fileName, &wr->oldSize);
    return true;
}

static void full_write_file(Write* wr)
{
    lua_State* L = wr->L;

    Con_Print(L, COLOR_SEPARATOR, "\n");
    Con_PrintSeparator(L);
    Con_PrintF(L, COLOR_STATUS, "%s disk file: ", (wr->oldBuffer ? "Overwriting" : "Writing"));
    Con_Flush(L);

    File_Overwrite(L, wr->fileName, wr->newBuffer, wr->newSize);

    Con_Print(L, COLOR_SUCCESS, "Done.\n");
    Con_PrintSeparator(L);
}

#define MIN_OVERWRITE 512 /* to avoid thrashing */

static void partial_update_file(Write* wr, File* file)
{
    lua_State* L = wr->L;

    Con_PrintSeparator(L);
    Con_Print(L, COLOR_STATUS, "Writing disk file: ");
    Con_Print(L, COLOR_PROGRESS_SIDE, "[");
    Con_Flush(L);

    const char* pOrig = wr->oldBuffer;
    const char* pOrigEnd = wr->oldBuffer + wr->oldSize;
    const char* pNew = wr->newBuffer;
    const char* pNewEnd = wr->newBuffer + wr->newSize;

    size_t skipped = 0;
    indicator_begin(wr);

    while (pOrig < pOrigEnd) {
        /* skip while matching */
        if (*pOrig == *pNew) {
            ++pOrig;
            ++pNew;
            ++skipped;
            continue;
        }

        indicator_add(wr, skipped, 0);
        skipped = 0;

        /* calculate amount of bytes to overwrite */
        const char* start = pNew;
        do {
            ++pOrig;
            ++pNew;
        } while (pOrig < pOrigEnd && (*pOrig != *pNew || (pNew - start) < MIN_OVERWRITE));

        size_t offset = start - wr->newBuffer;
        size_t count = pNew - start;
        File_SetPosition(file, offset);
        File_Write(file, wr->newBuffer + offset, count);
        indicator_add(wr, 0, count);
    }

    indicator_add(wr, skipped, 0);

    if (pNew < pNewEnd) {
        size_t offset = pNew - wr->newBuffer;
        size_t count = pNewEnd - pNew;
        File_SetPosition(file, offset);
        File_Write(file, wr->newBuffer + offset, count);
        indicator_add(wr, 0, count);
    }

    indicator_end(wr);

    char buf1[256];
    sprintf(buf1, "Modified %.1f%% of disk (", (wr->totalWritten * 100.0 / wr->newSize));

    char buf2[256];
    if (wr->totalWritten >= 1024*1024*1024)
        sprintf(buf2, "%.2f G", wr->totalWritten / (1024.0*1024.0*1024.0));
    else if (wr->totalWritten >= 1024*1024)
        sprintf(buf2, "%.2f M", wr->totalWritten / (1024.0*1024.0));
    else
        sprintf(buf2, "%.2f K", wr->totalWritten / (1024.0));

    Con_Print(L, COLOR_PROGRESS_SIDE, "]\n");
    Con_Print(L, COLOR_WARNING, buf1);
    Con_Print(L, COLOR_WARNING, buf2);
    Con_Print(L, COLOR_WARNING, "bytes)\n");
    Con_PrintSeparator(L);
}

static void validate_written_file(Write* wr)
{
    lua_State* L = wr->L;

    if (wr->newSize > wr->oldSize)
        wr->oldBuffer = (char*)lua_newuserdatauv(L, wr->newSize, 0);

    File* file = File_PushOpen(L, wr->fileName, FILE_OPEN_SEQUENTIAL_READ);
    if (File_GetSize(file) != wr->newSize) {
        File_Close(file);
        goto failed;
    }

    File_Read(file, wr->oldBuffer, wr->newSize);
    File_Close(file);

    if (memcmp(wr->oldBuffer, wr->newBuffer, wr->newSize) != 0) {
      failed:
        File_TryDelete(L, wr->fileName);
        luaL_error(L, "**** VALIDATION FAILED -- WRITTEN FILE MISMATCH! ****");
    }
}

/****************************************************************************/

Write* Write_Begin(lua_State* L, const char* file, size_t fileSize)
{
    Write* wr = (Write*)lua_newuserdatauv(L, sizeof(Write) + fileSize, 0);
    char* newBuffer = (char*)(wr + 1);

    memset(wr, 0, sizeof(Write));
    wr->L = L;
    wr->fileName = file;
    wr->dst = newBuffer;
    wr->dstEnd = newBuffer + fileSize;
    wr->oldBuffer = NULL;
    wr->newBuffer = newBuffer;
    wr->oldSize = 0;
    wr->newSize = fileSize;

    return wr;
}

void Write_Append(Write* wr, const void* data, size_t size)
{
    char* dst = wr->dst;
    if (dst + size > wr->dstEnd)
        luaL_error(wr->L, "write: overflow.");
    memcpy(dst, data, size);
    wr->dst = dst + size;
}

size_t Write_GetCurrentOffset(Write* wr)
{
    return wr->dst - wr->newBuffer;
}

void Write_Commit(Write* wr)
{
    lua_State* L = wr->L;

    if (!try_load_existing_file(wr)) {
        full_write_file(wr);
        return;
    }

    if (wr->oldSize == wr->newSize && !memcmp(wr->oldBuffer, wr->newBuffer, wr->newSize)) {
        Con_Print(L, COLOR_SEPARATOR, "\n===============================================\n");
        Con_Print(L, COLOR_SUCCESS,     " Existing disk file is identical, not writing.\n");
        Con_Print(L, COLOR_SEPARATOR,   "===============================================\n\n");
        return;
    }

    File* file = File_PushOpen(L, wr->fileName, FILE_OPEN_MODIFY);

    if (wr->newSize < wr->oldSize) {
        if (!File_TrySetSize(file, wr->newSize)) {
            full_write_file(wr);
            return;
        }

        wr->oldSize = wr->newSize;
    }

    partial_update_file(wr, file);
    File_Close(file);

    validate_written_file(wr);
}
