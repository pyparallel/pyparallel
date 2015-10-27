#processthreadsapi.h
from types cimport *

cdef extern from "<ntddk.h>":

    void __stdcall RtlGetCallersAddress(
        PVOID *CallersAddress,
        PVOID *CallersCaller
    )

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
