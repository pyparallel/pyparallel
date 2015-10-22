# WinBase.h
from types cimport *

cdef extern from *:

    ULONG OPERATION_API_VERSION

    ctypedef ULONG OPERATION_ID

    ctypedef struct OPERATION_START_PARAMETERS:
        ULONG           Version
        OPERATION_ID    OperationId
        ULONG           Flags
    ctypedef OPERATION_START_PARAMETERS* POPERATION_START_PARAMETERS
    # Flags:
    ULONG OPERATION_START_TRACE_CURRENT_THREAD

    ctypedef struct OPERATION_END_PARAMETERS:
        ULONG           Version
        OPERATION_ID    OperationId
        ULONG           Flags
    ctypedef OPERATION_END_PARAMETERS* POPERATION_END_PARAMETERS
    # Flags:
    ULONG OPERATION_END_DISCARD

    BOOL OperationStart(OPERATION_START_PARAMETERS *OperationParams)
    BOOL OperationEnd(OPERATION_END_PARAMETERS *OperationParams)

    ctypedef struct OPERATION_PARAMS:
        OPERATION_START_PARAMETERS  start
        OPERATION_END_PARAMETERS    end
    ctypedef OPERATION_PARAMS* POPERATION_PARAMETERS

# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:
