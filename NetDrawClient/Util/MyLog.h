#pragma once

#include <stdio.h>

FILE* _GetLogFilePtr();

#define Log(msg, ...)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        FILE* pFile = _GetLogFilePtr();                                                                                \
        fwprintf(pFile, msg L"\n", __VA_ARGS__);                                                                       \
        fclose(pFile);                                                                                                 \
    } while (false)

