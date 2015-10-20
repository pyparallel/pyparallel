cdef extern from *:

    ctypedef bint BOOL
    ctypedef char CHAR
    ctypedef CHAR CCHAR
    ctypedef CHAR BOOLEAN
    ctypedef unsigned char UCHAR
    ctypedef UCHAR BYTE
    ctypedef short SHORT
    ctypedef unsigned short USHORT
    ctypedef unsigned short WORD
    ctypedef WORD ATOM
    ctypedef USHORT WCHAR
    ctypedef WCHAR* PWSTR
    ctypedef float FLOAT
    ctypedef FLOAT* PFLOAT
    ctypedef int INT32
    ctypedef long long INT64
    ctypedef Py_ssize_t INT_PTR
    ctypedef unsigned int UINT
    ctypedef unsigned int UINT32
    ctypedef unsigned long long UINT64
    ctypedef Py_ssize_t UINT_PTR
    ctypedef long LONG
    ctypedef long LONG32
    ctypedef long long LONGLONG
    ctypedef long long LONG64
    ctypedef LONGLONG USN
    ctypedef unsigned long ULONG
    ctypedef ULONG ACCESS_MASK
    ctypedef Py_ssize_t ULONG_PTR
    ctypedef ULONG_PTR KAFFINITY
    ctypedef ULONG* PULONG
    ctypedef unsigned long long ULONGLONG
    ctypedef unsigned long long ULONG64
    ctypedef ULONG64* PULONG64
    ctypedef ULONGLONG* PULONGLONG
    ctypedef unsigned long DWORD
    ctypedef DWORD* PDWORD
    ctypedef unsigned int DWORD32
    ctypedef unsigned long long DWORD64
    ctypedef long long __int64
    ctypedef Py_ssize_t PVOID
    ctypedef Py_ssize_t LPVOID

    ctypedef Py_ssize_t HANDLE
    ctypedef HANDLE HDC
    ctypedef HANDLE HWND
    ctypedef HANDLE HRGN
    ctypedef HANDLE HBITMAP
    ctypedef HANDLE HGDIOBJ

    ctypedef struct PROCESSOR_NUMBER:
        WORD Group
        BYTE Number
        BYTE Reserved
    ctypedef PROCESSOR_NUMBER* PPROCESSOR_NUMBER

    ctypedef struct PROCESS_INFORMATION:
        HANDLE hProcess
        HANDLE hThread
        DWORD dwProcessId
        DWORD dwThreadId
    ctypedef PROCESS_INFORMATION* PPROCESS_INFORMATION

    ctypedef enum PROCESS_INFORMATION_CLASS:
        ProcessBasicInformation             = 0
        ProcessQuotaLimits                  = 1
        ProcessIoCounters                   = 2
        ProcessVmCounters                   = 3
        ProcessTimes                        = 4
        ProcessBasePriority                 = 5
        ProcessRaisePriority                = 6
        ProcessDebugPort                    = 7
        ProcessExceptionPort                = 8
        ProcessAccessToken                  = 9
        ProcessLdtInformation               = 10
        ProcessLdtSize                      = 11
        ProcessDefaultHardErrorMode         = 12
        ProcessIoPortHandlers               = 13
        ProcessPooledUsageAndLimits         = 14
        ProcessWorkingSetWatch              = 15
        ProcessUserModeIOPL                 = 16
        ProcessEnableAlignmentFaultFixup    = 17
        ProcessPriorityClass                = 18
        ProcessWx86Information              = 19
        ProcessHandleCount                  = 20
        ProcessAffinityMask                 = 21
        ProcessPriorityBoost                = 22

        ProcessWow64Information             = 26
        ProcessImageFileName                = 27

    ctypedef struct SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION:
        DWORD64 CycleTime
    ctypedef SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION* PSYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION


    ctypedef struct MEMORY_PRIORITY_INFORMATION:
        ULONG MemoryPriority
    ctypedef MEMORY_PRIORITY_INFORMATION* PMEMORY_PRIORITY_INFORMATION

    ctypedef enum MEMORY_PRIORITY:
        MEMORY_PRIORITY_VERY_LOW        = 1
        MEMORY_PRIORITY_LOW             = 2
        MEMORY_PRIORITY_MEDIUM          = 3
        MEMORY_PRIORITY_BELOW_NORMAL    = 4
        MEMORY_PRIORITY_NORMAL          = 5

    ctypedef struct LIST_ENTRY:
        LIST_ENTRY *Flink
        LIST_ENTRY *Blink
    ctypedef LIST_ENTRY* PLIST_ENTRY

    # See http://www.hitmaroc.net/600697-3913-cython-nesting-union-within-struct.html
    # regarding the quirky nesting.
    ctypedef struct _ULARGE_INTEGER:
        DWORD LowPart
        DWORD HighPart

    ctypedef union ULARGE_INTEGER:
        DWORD LowPart
        DWORD HighPart
        _ULARGE_INTEGER u
        ULONGLONG QuadPart

    ctypedef struct _LARGE_INTEGER:
        DWORD LowPart
        LONG  HighPart

    ctypedef union LARGE_INTEGER:
        DWORD LowPart
        LONG  HighPart
        _LARGE_INTEGER u
        LONGLONG QuadPart

    ctypedef struct UNICODE_STRING:
        USHORT Length
        USHORT MaximumLength
        PWSTR  Buffer
    ctypedef UNICODE_STRING* PUNICODE_STRING

    ctypedef struct IO_COUNTERS:
        ULONGLONG ReadOperationCount
        ULONGLONG WriteOperationCount
        ULONGLONG OtherOperationCount
        ULONGLONG ReadTransferCount
        ULONGLONG WriteTransferCount
        ULONGLONG OtherTransferCount
    ctypedef IO_COUNTERS* PIO_COUNTERS

    ctypedef struct FILETIME:
        DWORD dwLowDateTime
        DWORD dwHighDateTime
    ctypedef FILETIME* PFILETIME
    ctypedef PFILETIME LPFILETIME

    ctypedef struct SYSTEMTIME:
        WORD wYear
        WORD wMonth
        WORD wDayOfWeek
        WORD wDay
        WORD wHour
        WORD wMinute
        WORD wSecond
        WORD wMilliseconds
    ctypedef SYSTEMTIME* PSYSTEMTIME
    ctypedef PSYSTEMTIME LPSYSTEMTIME

    ctypedef struct CRITICAL_SECTION:
        pass
    ctypedef CRITICAL_SECTION* PCRITICAL_SECTION
    ctypedef CRITICAL_SECTION* LPCRITICAL_SECTION

    ctypedef enum LOGICAL_PROCESSOR_RELATIONSHIP:
        RelationProcessorCore       = 0
        RelationNumaNode            = 1
        RelationCache               = 2
        RelationProcessorPackage    = 3
        RelationGroup               = 4
        RelationAll                 = 0xffff

    ctypedef struct GROUP_AFFINITY:
        KAFFINITY Mask
        WORD Group
        WORD Reserved[3]
    ctypedef GROUP_AFFINITY* PGROUP_AFFINITY

    ctypedef struct PROCESSOR_RELATIONSHIP:
        BYTE Flags
        BYTE EfficiencyClass
        BYTE Reserved[21]
        WORD GroupCount

    ctypedef enum CACHE_LEVEL:
        L1 = 1
        L2 = 2
        L3 = 3

    ctypedef enum PROCESSOR_CACHE_TYPE:
        CacheUnified
        CacheInstruction
        CacheData
        CacheTrace

    ctypedef struct CACHE_DESCRIPTOR:
        BYTE Level
        BYTE Associativity
        WORD LineSize
        DWORD Size
        PROCESSOR_CACHE_TYPE Type
    ctypedef CACHE_DESCRIPTOR* PCACHE_DESCRIPTOR

    ctypedef struct CACHE_RELATIONSHIP:
        BYTE Level
        BYTE Associativity
        WORD LineSize
        DWORD CacheSize
        PROCESSOR_CACHE_TYPE Type
        BYTE Reserved[20]
        GROUP_AFFINITY GroupMask
    ctypedef CACHE_RELATIONSHIP* PCACHE_RELATIONSHIP

    ctypedef struct NUMA_NODE_RELATIONSHIP:
        DWORD NodeNumber
        BYTE Reserved[20]
        GROUP_AFFINITY GroupMask
    ctypedef NUMA_NODE_RELATIONSHIP* PNUMA_NODE_RELATIONSHIP

    ctypedef struct PROCESSOR_GROUP_INFO:
        BYTE MaximumProcessorCount
        BYTE ActiveProcessorCount
        BYTE Reserved[38]
        KAFFINITY ActiveProcessorMask
    ctypedef PROCESSOR_GROUP_INFO* PPROCESSOR_GROUP_INFO

    ctypedef struct GROUP_RELATIONSHIP:
        WORD MaximumGroupCount
        WORD ActiveGroupCount
        BYTE Reserved[20]
        # 1 = ANYSIZE_ARRAY
        PROCESSOR_GROUP_INFO GroupInfo[1]

    ctypedef struct PROCESSOR_NUMBER:
        WORD Group
        BYTE Number
        BYTE Reserved
    ctypedef PROCESSOR_NUMBER* PPROCESSOR_NUMBER

    ctypedef struct SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX:
        LOGICAL_PROCESSOR_RELATIONSHIP Relationship
        DWORD Size
        # union:
        PROCESSOR_RELATIONSHIP Processor
        NUMA_NODE_RELATIONSHIP NumaNode
        CACHE_RELATIONSHIP Cache
        GROUP_RELATIONSHIP Group

    ctypedef enum CPU_SET_INFORMATION_TYPE:
        CpuSetInformation

    ctypedef struct CPU_SET:
        DWORD Id
        WORD Group
        BYTE LogicalProcessorIndex
        BYTE CoreIndex
        BYTE LastLevelCacheIndex
        BYTE NumaNodeIndex
        BYTE EfficiencyClass

        # Inner anonymous struct:
        BOOLEAN Parked
        BOOLEAN Allocated
        BOOLEAN AllocatedToTargetProcess
        BOOLEAN RealTime
        BYTE ReservedFlags

        DWORD Reserved
        DWORD64 AllocationTag

    ctypedef struct SYSTEM_LOGICAL_PROCESSOR_INFORMATION:
        DWORD Size
        CPU_SET_INFORMATION_TYPE Type
        CPU_SET CpuSet

        # Union/struct CpuSet info repeated:
        DWORD Id
        WORD Group
        BYTE LogicalProcessorIndex
        BYTE CoreIndex
        BYTE LastLevelCacheIndex
        BYTE NumaNodeIndex
        BYTE EfficiencyClass

        # Inner anonymous struct:
        BOOLEAN Parked
        BOOLEAN Allocated
        BOOLEAN AllocatedToTargetProcess
        BOOLEAN RealTime
        BYTE ReservedFlags

        DWORD Reserved
        DWORD64 AllocationTag
    ctypedef SYSTEM_LOGICAL_PROCESSOR_INFORMATION SYSTEM_CPU_SET_INFORMATION
    ctypedef SYSTEM_CPU_SET_INFORMATION* PSYSTEM_CPU_SET_INFORMATION

    ctypedef struct SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX:
        LOGICAL_PROCESSOR_RELATIONSHIP Relationship
        DWORD Size
        # Union:
        PROCESSOR_RELATIONSHIP Processor
        NUMA_NODE_RELATIONSHIP NumaNode
        CACHE_RELATIONSHIP Cache
        GROUP_RELATIONSHIP Group
    ctypedef SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX

    ctypedef struct HEAPENTRY32:
        Py_ssize_t dwSize
        HANDLE hHandle
        ULONG_PTR dwAddress
        Py_ssize_t dwBlockSize
        DWORD dwFlags
        DWORD dwLockCount
        DWORD dwResvd
        DWORD th32ProcessID
        ULONG_PTR th32HeapID
    ctypedef HEAPENTRY32* PHEAPENTRY32

    ctypedef enum HEAPENTRY32_FLAGS:
        LF32_FIXED      = 1
        LF32_FREE       = 2
        LF32_MOVEABLE   = 4

    ctypedef struct HEAPLIST32:
        Py_ssize_t dwSize
        DWORD th32ProcessID
        ULONG_PTR th32HeapID
        DWORD dwFlags
    ctypedef HEAPLIST32* PHEAPLIST32
    ctypedef HEAPLIST32* LPHEAPLIST32

    ctypedef struct THREADENTRY32:
        DWORD dwSize
        DWORD cntUsage
        DWORD th32ThreadID
        DWORD th32OwnerProcessID
        LONG tpBasePri
        LONG tpDeltaPri
        DWORD dwFlags
    ctypedef THREADENTRY32* PTHREADENTRY32

    ctypedef struct BITMAP:
        LONG bmType
        LONG bmWidth
        LONG bmHeight
        LONG bmWidthBytes
        LONG bmPlanes
        LONG bmBitsPixel
        LPVOID bmBits
    ctypedef BITMAP* PBITMAP
    ctypedef BITMAP* LPBITMAP

    ctypedef struct RTL_BITMAP:
        ULONG SizeOfBitMap
        PULONG Buffer
    ctypedef RTL_BITMAP* PRTL_BITMAP

    ctypedef struct RTL_BITMAP_RUN:
        ULONG StartingIndex
        ULONG NumberOfBits
    ctypedef RTL_BITMAP_RUN* PRTL_BITMAP_RUN

    ctypedef LARGE_INTEGER PHYSICAL_ADDRESS

    ctypedef struct MM_PHYSICAL_ADDRESS_LIST:
        PHYSICAL_ADDRESS PhysicalAddress
        Py_ssize_t NumberOfBytes
    ctypedef MM_PHYSICAL_ADDRESS_LIST* PMM_PHYSICAL_ADDRESS_LIST

# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:
