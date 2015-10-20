cdef extern from "<windows.h>":
    pass

cdef inline ULONGLONG FileTimeToUnsignedLongLong(PFILETIME filetime):
    cdef ULARGE_INTEGER ul
    ul.LowPart = filetime.dwLowDateTime
    ul.HighPart = filetime.dwHighDateTime
    return ul.QuadPart

