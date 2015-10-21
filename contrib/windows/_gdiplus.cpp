
#include "_gdiplus.h"

static GDIPLUS _gdiplus = { 0, };

BOOL
CALLBACK
gdiplus_startup_callback(
    PINIT_ONCE init_once,
    PVOID param,
    PVOID *context
)
{
    Gdiplus::GdiplusStartup(&_gdiplus.token, &_gdiplus.input, &_gdiplus.output);
    return TRUE;
}

ULONG_PTR
_gdiplus_startup(void)
{
    InitOnceExecuteOnce(&_gdiplus.startup_init_once,
                        gdiplus_startup_callback,
                        NULL,
                        NULL);
    return _gdiplus.token;
}

BOOL
CALLBACK
gdiplus_shutdown_callback(
    PINIT_ONCE init_once,
    PVOID param,
    PVOID *context
)
{
    ULONG_PTR token = _gdiplus_startup();
    Gdiplus::GdiplusShutdown(token);
    return TRUE;
}

void
_gdiplus_shutdown(void)
{
    InitOnceExecuteOnce(&_gdiplus.shutdown_init_once,
                        gdiplus_shutdown_callback,
                        NULL,
                        NULL);
}


extern "C" {
ULONG_PTR
gdiplus_startup(void)
{
    return _gdiplus_startup();
}

void
gdiplus_shutdown(void)
{
    _gdiplus_shutdown();
}

PyObject *
_load_gdiplus(void)
{
    return PyInit_gdiplus();
}
}

/* vim: set ts=8 sw=4 sts=4 tw=80 et:                                         */
