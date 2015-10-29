
include "windows.pxi"
from base cimport *
from file cimport *
from string cimport *
from threadpool cimport *

cdef class Threadpool:

    cdef PTP_IO create_threadpool_io(self, HANDLE fl,
                                     PTP_WIN32_IO_CALLBACK pfnio,
                                     PVOID pv):
        return <PTP_IO>NULL;


# vim: set ts=8 sw=4 sts=4 tw=80 et nospell:                                   #
