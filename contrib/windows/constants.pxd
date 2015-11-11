
from types cimport *

cdef extern from *:
    # Anonymous enum for constants
    cpdef enum:
        CREATE_ALWAYS
        CREATE_NEW
        OPEN_ALWAYS
        OPEN_EXISTING
        TRUNCATE_EXISTING
        GENERIC_WRITE
        FILE_ATTRIBUTE_NORMAL

    cpdef enum PROCESSOR_ARCHITECTURE:
        PROCESSOR_ARCHITECTURE_AMD64    = 9
        PROCESSOR_ARCHITECTURE_ARM      = 5
        PROCESSOR_ARCHITECTURE_IA64     = 6
        PROCESSOR_ARCHITECTURE_INTEL    = 0
        PROCESSOR_ARCHITECTURE_UNKNOWN  = 0xffff

    cpdef enum MEMORY_ALLOCATION_TYPE:
        MEM_COMMIT          = 0x00001000
        MEM_RESERVE         = 0x00002000
        MEM_RESET           = 0x00080000
        #MEM_RESET_UNDO      = 0x1000000
        MEM_LARGE_PAGES     = 0x20000000
        MEM_PHYSICAL        = 0x00400000
        MEM_TOP_DOWN        = 0x00100000
        MEM_WRITE_WATCH     = 0x00200000

    cpdef enum MEMORY_FREE_TYPE:
        MEM_DECOMMIT        = 0x4000
        MEM_RELEASE         = 0x8000

#    cpdef enum MEMORY_PRIORITY:
#        MEMORY_PRIORITY_VERY_LOW        = 1
#        MEMORY_PRIORITY_LOW             = 2
#        MEMORY_PRIORITY_MEDIUM          = 3
#        MEMORY_PRIORITY_BELOW_NORMAL    = 4
#        MEMORY_PRIORITY_NORMAL          = 5

    cpdef enum CONTEXT_FLAGS:
        CONTEXT_AMD64
        CONTEXT_CONTROL
        CONTEXT_INTEGER
        CONTEXT_SEGMENTS
        CONTEXT_FLOATING_POINT
        CONTEXT_FULL
        CONTEXT_ALL
        CONTEXT_XSTATE

    #cpdef enum _RIO_NOTIFICATION_COMPLETION_TYPE:
    #    RIO_EVENT_COMPLETION = 1
    #    RIO_IOCP_COMPLETION  = 2

#    cpdef enum PROCESS_INFORMATION_CLASS:
#        ProcessBasicInformation             = 0
#        ProcessQuotaLimits                  = 1
#        ProcessIoCounters                   = 2
#        ProcessVmCounters                   = 3
#        ProcessTimes                        = 4
#        ProcessBasePriority                 = 5
#        ProcessRaisePriority                = 6
#        ProcessDebugPort                    = 7
#        ProcessExceptionPort                = 8
#        ProcessAccessToken                  = 9
#        ProcessLdtInformation               = 10
#        ProcessLdtSize                      = 11
#        ProcessDefaultHardErrorMode         = 12
#        ProcessIoPortHandlers               = 13
#        ProcessPooledUsageAndLimits         = 14
#        ProcessWorkingSetWatch              = 15
#        ProcessUserModeIOPL                 = 16
#        ProcessEnableAlignmentFaultFixup    = 17
#        ProcessPriorityClass                = 18
#        ProcessWx86Information              = 19
#        ProcessHandleCount                  = 20
#        ProcessAffinityMask                 = 21
#        ProcessPriorityBoost                = 22
#        ProcessWow64Information             = 26
#        ProcessImageFileName                = 27

    cpdef enum LOGICAL_PROCESSOR_RELATIONSHIP:
        RelationProcessorCore       = 0
        RelationNumaNode            = 1
        RelationCache               = 2
        RelationProcessorPackage    = 3
        RelationGroup               = 4
        RelationAll                 = 0xffff

    cpdef enum IO_REPARSE_TAG:
        IO_REPARSE_TAG_CSV = 0x80000009
        #IO_REPARSE_TAG_DEDUP = 0x80000013
        IO_REPARSE_TAG_DFS = 0x8000000A
        IO_REPARSE_TAG_DFSR = 0x80000012
        IO_REPARSE_TAG_HSM = 0xC0000004
        IO_REPARSE_TAG_HSM2 = 0x80000006
        IO_REPARSE_TAG_MOUNT_POINT = 0xA0000003
        #IO_REPARSE_TAG_NFS = 0x80000014
        IO_REPARSE_TAG_SIS = 0x80000007
        IO_REPARSE_TAG_SYMLINK = 0xA000000C
        IO_REPARSE_TAG_WIM = 0x80000008

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
