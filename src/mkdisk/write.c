#include <common/common.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
 #ifndef __MINGW32__
  #define fileno _fileno
 #endif
 #define ftruncate _chsize
#endif
#include <mkdisk/write.h>

static const char* fileName;

static char* new_buffer;
static char* dst;
static const char* dstEnd;
static size_t new_size;

static char* old_buffer;
static size_t old_size;

/****************************************************************************/

static void error_reading_file(FILE* f)
{
    fprintf(stderr, "Error reading file \"%s\": %s\n", fileName, strerror(errno));
    fclose(f);
    exit(1);
}

static void error_writing_file(FILE* f)
{
    fprintf(stderr, "Error writing file \"%s\": %s\n", fileName, strerror(errno));
    fclose(f);
    remove(fileName);
    exit(1);
}

/****************************************************************************/

#define INDICATOR_STEP (1048576/2)

static size_t indicatorTotal;
static size_t indicatorTotalWritten;
static size_t indicatorTotalSkipped;
static size_t totalSkipped;
static size_t totalWritten;

static void indicator_begin(void)
{
    totalSkipped = 0;
    totalWritten = 0;
    indicatorTotal = 0;
    indicatorTotalWritten = 0;
    indicatorTotalSkipped = 0;
}

static void indicator_write_char(void)
{
    static const size_t indicLen = 6;
    static const char indic[] = ".~*xX#";
    char ch;

    if (indicatorTotalWritten == 0)
        ch = indic[0];
    else {
        double percent = (indicatorTotalWritten * 1.0 / indicatorTotal);
        size_t index = 1 + (size_t)(percent * (indicLen - 1));
        if (index >= indicLen)
            index = indicLen - 1;
        ch = indic[index];
    }

    fputc(ch, stdout);

    #define DECREASE(x) (x = (x > INDICATOR_STEP ? x - INDICATOR_STEP : 0))
    DECREASE(indicatorTotal);
    DECREASE(indicatorTotalWritten);
    DECREASE(indicatorTotalSkipped);
}

static void indicator_add(size_t skipped, size_t written)
{
    totalSkipped += skipped;
    totalWritten += written;

    indicatorTotal += skipped + written;
    indicatorTotalSkipped += skipped;
    indicatorTotalWritten += written;

    if (indicatorTotal >= INDICATOR_STEP) {
        do {
            indicator_write_char();
        } while (indicatorTotal >= INDICATOR_STEP);
        fflush(stdout);
    }
}

static void indicator_end(void)
{
    if (indicatorTotal != 0)
        indicator_write_char();
}

/****************************************************************************/

static bool try_load_existing_file(void)
{
    old_size = 0;
    old_buffer = NULL;

    FILE* f = fopen(fileName, "rb");
    if (!f)
        return false;

    fseek(f, 0, SEEK_END);
    long fSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (ferror(f) || fSize < 0)
        error_reading_file(f);

    old_size = (size_t)fSize;
    old_buffer = (char*)malloc(old_size);
    if (!old_buffer) {
        fprintf(stderr, "Memory allocation failed.\n");
        fclose(f);
        exit(1);
    }

    size_t bytesRead = fread(old_buffer, 1, old_size, f);
    if (ferror(f) || bytesRead != old_size)
        error_reading_file(f);

    fclose(f);

    return true;
}

static void full_write_file(void)
{
    printf("\n------------------------------------------------------\n");
    printf("%s disk file: ", (old_buffer ? "Overwriting" : "Writing"));
    fflush(stdout);

    FILE* f = fopen(fileName, "wb");
    if (!f) {
        fprintf(stderr, "Can't create \"%s\": %s\n", fileName, strerror(errno));
        exit(1);
    }

    size_t bytesWritten = fwrite(new_buffer, 1, new_size, f);
    if (ferror(f) || bytesWritten != new_size)
        error_writing_file(f);

    fclose(f);

    printf("Done.\n");
    printf("------------------------------------------------------\n");
}

#define MIN_OVERWRITE 512 /* to avoid thrashing */

static void partial_update_file(FILE* f)
{
    printf("\n------------------------------------------------------\n");
    printf("Writing disk file: [");
    fflush(stdout);

    const char* pOrig = old_buffer;
    const char* pOrigEnd = old_buffer + old_size;
    const char* pNew = new_buffer;
    const char* pNewEnd = new_buffer + new_size;

    size_t skipped = 0;
    indicator_begin();

    while (pOrig < pOrigEnd) {
        /* skip while matching */
        if (*pOrig == *pNew) {
            ++pOrig;
            ++pNew;
            ++skipped;
            continue;
        }

        indicator_add(skipped, 0);
        skipped = 0;

        /* calculate amount of bytes to overwrite */
        const char* start = pNew;
        do {
            ++pOrig;
            ++pNew;
        } while (pOrig < pOrigEnd && (*pOrig != *pNew || (pNew - start) < MIN_OVERWRITE));

        size_t offset = start - new_buffer;
        fseek(f, offset, SEEK_SET);
        if (ferror(f))
            error_writing_file(f);

        size_t bytesToWrite = pNew - start;
        size_t bytesWritten = fwrite(new_buffer + offset, 1, bytesToWrite, f);
        if (ferror(f) || bytesWritten != bytesToWrite)
            error_writing_file(f);

        indicator_add(0, bytesWritten);
    }

    indicator_add(skipped, 0);

    if (pNew < pNewEnd) {
        size_t offset = pNew - new_buffer;
        fseek(f, offset, SEEK_SET);
        if (ferror(f))
            error_writing_file(f);

        size_t bytesToWrite = pNewEnd - pNew;
        size_t bytesWritten = fwrite(new_buffer + offset, 1, bytesToWrite, f);
        if (ferror(f) || bytesWritten != bytesToWrite)
            error_writing_file(f);

        indicator_add(0, bytesWritten);
    }

    indicator_end();

    printf("]\nModified %.1f%% of disk (", (totalWritten * 100.0 / new_size));
    if (totalWritten >= 1024*1024*1024)
        printf("%.2f Gbytes", totalWritten / (1024.0*1024.0*1024.0));
    else if (totalWritten >= 1024*1024)
        printf("%.2f Mbytes", totalWritten / (1024.0*1024.0));
    else
        printf("%.2f Kbytes", totalWritten / (1024.0));
    printf(")\n------------------------------------------------------\n");
}

static void validate_written_file(void)
{
    old_buffer = realloc(old_buffer, new_size);
    if (!old_buffer) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }

    FILE* f = fopen(fileName, "rb");
    if (!f) {
        fprintf(stderr, "Can't open \"%s\": %s\n", fileName, strerror(errno));
        remove(fileName);
        exit(1);
    }

    size_t bytesRead = fread(old_buffer, 1, new_size, f);
    if (ferror(f) || bytesRead != new_size) {
        fprintf(stderr, "Error reading file \"%s\": %s\n", fileName, strerror(errno));
        fclose(f);
        remove(fileName);
        exit(1);
    }

    fclose(f);

    if (memcmp(old_buffer, new_buffer, new_size) != 0) {
        fprintf(stderr, "\n**** VALIDATION FAILED -- WRITTEN FILE MISMATCH! ****\n\n");
        remove(fileName);
        exit(1);
    }
}

/****************************************************************************/

void write_begin(const char* file, size_t fileSize)
{
    new_buffer = (char*)malloc(fileSize);
    if (!new_buffer) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }

    dst = new_buffer;
    dstEnd = new_buffer + fileSize;
    new_size = fileSize;

    fileName = file;
}

void write_append(const void* data, size_t size)
{
    assert(dst + size <= dstEnd);
    memcpy(dst, data, size);
    dst += size;
}

size_t write_get_current_offset(void)
{
    return dst - new_buffer;
}

void write_commit(void)
{
    try_load_existing_file();

    if (!old_buffer) {
        full_write_file();
        return;
    }

    if (old_size == new_size && !memcmp(old_buffer, new_buffer, new_size)) {
        printf("\n------------------------------------------------------\n");
        printf("Existing disk file is identical, not writing.\n");
        printf("------------------------------------------------------\n");
        return;
    }

    FILE* f = fopen(fileName, "r+b");
    if (!f) {
        fprintf(stderr, "Can't overwrite \"%s\": %s\n", fileName, strerror(errno));
        exit(1);
    }

    if (new_size < old_size) {
        int fd = fileno(f);
        if (fd < 0 || ftruncate(fd, new_size) < 0) {
            fprintf(stderr, "WARNING: truncate of file \"%s\" failed: %s\n", fileName, strerror(errno));
            fclose(f);
            full_write_file();
            return;
        }

        fseek(f, 0, SEEK_END);
        long newSize = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (ferror(f) || (size_t)newSize != new_size) {
            fprintf(stderr, "WARNING: truncate of file \"%s\" failed: %s\n", fileName, strerror(errno));
            fclose(f);
            full_write_file();
            return;
        }

        old_size = new_size;
    }

    partial_update_file(f);
    fclose(f);

    validate_written_file();
}
