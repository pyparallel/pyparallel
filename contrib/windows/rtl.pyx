
from rtl cimport *

cdef class Bitmap:
    cdef:
        PULONG buffer
        ULONG  size
        PRTL_BITMAP bitmap

    def __cinit__(self):
        pass

# vim: set ts=8 sw=4 sts=4 tw=80 et:
