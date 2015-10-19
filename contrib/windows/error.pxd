# winerror.h

from types cimport *

cdef extern from "<windows.h>":
    pass

cdef public DWORD ERROR_SUCCESS = 0
cdef public DWORD ERROR_INSUFFICIENT_BUFFER = 122
