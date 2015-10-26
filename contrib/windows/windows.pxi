cdef extern from "<windows.h>":
    pass

cdef inline ULONGLONG FileTimeToUnsignedLongLong(PFILETIME filetime):
    cdef ULARGE_INTEGER ul
    ul.LowPart = filetime.dwLowDateTime
    ul.HighPart = filetime.dwHighDateTime
    return ul.QuadPart

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell syntax=cython:                      #
