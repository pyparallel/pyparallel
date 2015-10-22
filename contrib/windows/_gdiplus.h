
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

static CLSID ImageEncoderBmpClsid = {
    0x557cf400,
    0x1a04,
    0x11d3,
    {
        0x9a,
        0x73,
        0x00,
        0x00,
        0xf8,
        0x1e,
        0xf3,
        0x2e
    }
};

static CLSID ImageEncoderJpegClsid = {
    0x557cf401,
    0x1a04,
    0x11d3,
    {
        0x9a,
        0x73,
        0x00,
        0x00,
        0xf8,
        0x1e,
        0xf3,
        0x2e
    }
};

static CLSID ImageEncoderGifClsid = {
    0x557cf402,
    0x1a04,
    0x11d3,
    {
        0x9a,
        0x73,
        0x00,
        0x00,
        0xf8,
        0x1e,
        0xf3,
        0x2e
    }
};

static CLSID ImageEncoderTiffClsid = {
    0x557cf405,
    0x1a04,
    0x11d3,
    {
        0x9a,
        0x73,
        0x00,
        0x00,
        0xf8,
        0x1e,
        0xf3,
        0x2e
    }
};

static CLSID ImageEncoderPngClsid = {
    0x557cf406,
    0x1a04,
    0x11d3,
    {
        0x9a,
        0x73,
        0x00,
        0x00,
        0xf8,
        0x1e,
        0xf3,
        0x2e
    }
};

} /* extern "C" */

/* vim: set ts=8 sw=4 sts=4 tw=80 et:                                         */
