# winerror.h

from types cimport *

cdef extern from "<windows.h>":
    DWORD ERROR_SUCCESS = 0
    DWORD ERROR_INSUFFICIENT_BUFFER = 122

cdef public DWORD ERROR_SUCCESS
