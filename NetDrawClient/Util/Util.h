#pragma once

#define Crash()                                                                                                        \
    do                                                                                                                 \
    {                                                                                                                  \
        wprintf(L"%s %d", __FILEW__, __LINE__);                                                                        \
        *(int*)0 = 0;                                                                                                  \
    } while (0)

#define CrashIf(cond)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (cond)                                                                                                      \
            *(int*)0 = 0;                                                                                              \
    } while (0)