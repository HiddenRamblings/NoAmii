#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "logger.h"
#include "ifile.h"

#ifndef PATH_MAX
#define PATH_MAX 255
#define _MAX_LFN 255
#endif

int logger_is_initd = 0;

static IFile file;

void openLogger()
{
    if (logger_is_initd)
        return;

    Result r = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, "/luma/titles/logx2.txt"), FS_OPEN_CREATE | FS_OPEN_WRITE | FS_OPEN_READ);

    if (R_FAILED(r)) {
        logger_is_initd = -1;
        return;
    }

    //IFile_Truncate(&file);

    logger_is_initd = 1;
}

void
logstr(const char *str)
{
    if (logger_is_initd == -1)
        return; // Errored during init. Don't bother.

    u32 len = strlen(str);
	u64 total;
    IFile_Write(&file, &total, str, len, FS_WRITE_FLUSH);
}

void
logu64(u64 progId)
{
    char str[] = "Title: 0000000000000000\n";
    u32 i = 22;
    while (progId) {
        static const char hexDigits[] = "0123456789ABCDEF";
        str[i--] = hexDigits[(u32)(progId & 0xF)];
        progId >>= 4;
    }

    logstr(str);
}

void closeLogger()
{
    if (logger_is_initd)
        return;

    IFile_Close(&file);
    logger_is_initd = 0;
}

void panicstr(const char *str)
{
    logstr(str);
    closeLogger();
    svcBreak(USERBREAK_ASSERT);
}

