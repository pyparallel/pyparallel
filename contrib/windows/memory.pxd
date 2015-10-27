#processthreadsapi.h
from types cimport *

cdef extern from *:

    BOOL __stdcall SetProcessWorkingSetSize(
        HANDLE hProcess,
        SIZE_T dwMinimumWorkingSetSize,
        SIZE_T dwMaximumWorkingSetSize
    )

    BOOL __stdcall SetProcessWorkingSetSizeEx(
        HANDLE hProcess,
        SIZE_T dwMinimumWorkingSetSize,
        SIZE_T dwMaximumWorkingSetSize,
        DWORD Flags
    )
    ctypedef enum WORKING_SET_SIZE_FLAGS:
        QUOTA_LIMITS_HARDWS_MIN_DISABLE     = 0x00000002
        QUOTA_LIMITS_HARDWS_MIN_ENABLE      = 0x00000001
        QUOTA_LIMITS_HARDWS_MAX_DISABLE     = 0x00000008
        QUOTA_LIMITS_HARDWS_MAX_ENABLE      = 0x00000004

    BOOL __stdcall GetProcessWorkingSetSize(
        HANDLE hProcess,
        PSIZE_T lpMinimumWorkingSetSize,
        PSIZE_T lpMaximumWorkingSetSize
    )

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
