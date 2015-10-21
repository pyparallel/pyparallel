
#include "Python.h"
#include <Windows.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

typedef struct _GDIPLUS {
    GdiplusStartupInput  input;
    GdiplusStartupOutput output;
    ULONG_PTR            token;
    INIT_ONCE            startup_init_once;
    INIT_ONCE            shutdown_init_once;
} GDIPLUS, *PGDIPLUS;

extern PyObject* PyInit_gdiplus(void);

extern "C" {
PyAPI_FUNC(ULONG_PTR) gdiplus_startup(void);
PyAPI_FUNC(void) gdiplus_shutdown(void);
PyAPI_FUNC(PyObject *) _load_gdiplus(void);
}

/* vim: set ts=8 sw=4 sts=4 tw=80 et:                                         */
