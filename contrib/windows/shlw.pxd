from types cimport *
from objidl cimport *

cdef extern from "<Shlwapi.h>":

    cdef IStream* SHCreateMemStream(const BYTE *pInit, UINT cbInit)

# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:
