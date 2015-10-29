
from types cimport *
from constants cimport *

include "windows.pxi"
cimport base
cimport processthreads as pt

cdef class ProcessorNumber:
    cdef readonly:
        PROCESSOR_NUMBER pn

    def __cinit__(self):
        base.GetCurrentProcessorNumberEx(&self.pn)

    property group:
        def __get__(self):
            return self.pn.Group

    property number:
        def __get__(self):
            return self.pn.Number

cdef class Context:
    cdef public:
        HANDLE handle
        CONTEXT ctx

    def __cinit__(self, HANDLE hThread = <HANDLE>NULL,
                        DWORD flags = CONTEXT_FULL):
        self.handle = hThread
        self.ctx.ContextFlags = flags
        self.refresh()

    cpdef void refresh(self):
        if not self.handle:
            return
        if not base.GetThreadContext(self.handle, &self.ctx):
            raise OSError("GetThreadContext")

cdef class IOCounters:
    cdef readonly:
        HANDLE handle
        IO_COUNTERS counters

    def __cinit__(self, HANDLE handle):
        self.handle = handle
        self.refresh()

    cpdef void refresh(self):
        if not base.GetProcessIoCounters(self.handle, &self.counters):
            raise OSError("GetProcessIoCounters")

#def GetCurrentProcess():
#    return pt.GetCurrentProcess()

#def GetCurrentThread():
#    return pt.GetCurrentThread()

#def GetActiveProcessorCount(WORD GroupNumber):
#    return base.GetActiveProcessorCount(GroupNumber)

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
