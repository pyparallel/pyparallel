
include "windows.pxi"

from gdiplus cimport *

cpdef ULONG_PTR startup():
    return gdiplus_startup()

cpdef void shutdown():
    gdiplus_shutdown()

# vim: set ts=8 sw=4 sts=4 tw=80 et:
