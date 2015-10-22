
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#include <Windows.h>
#pragma comment(lib, "Advapi32.lib")

typedef struct _OPERATION_PARAMS {
    OPERATION_START_PARAMETERS  start;
    OPERATION_END_PARAMETERS    end;
} OPERATION_PARAMS, *POPERATION_PARAMS;

/* vim: set ts=8 sw=4 sts=4 tw=80 et:                                         */
