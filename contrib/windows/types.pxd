cdef extern from *:

    ctypedef bint BOOL
    ctypedef char CHAR
    ctypedef CHAR CCHAR
    ctypedef CHAR* PCHAR
    ctypedef CHAR* CSTR
    ctypedef CHAR* LPCSTR
    ctypedef CHAR BOOLEAN
    ctypedef unsigned char UCHAR
    ctypedef UCHAR BYTE
    ctypedef UCHAR* PUCHAR
    ctypedef short SHORT
    ctypedef unsigned short USHORT
    ctypedef unsigned short WORD
    ctypedef WORD *PWORD
    ctypedef WORD ATOM
    ctypedef USHORT WCHAR
    ctypedef WCHAR* PWSTR
    ctypedef WCHAR* PTSTR
    ctypedef WCHAR* LPTSTR
    ctypedef WCHAR* LPSTR
    ctypedef WCHAR* LPWSTR
    ctypedef WCHAR* POLESTR
    ctypedef WCHAR* LPOLESTR
    ctypedef const WCHAR* LPCTSTR
    ctypedef const WCHAR* LPCWSTR
    ctypedef float FLOAT
    ctypedef FLOAT* PFLOAT
    ctypedef int INT
    ctypedef int INT32
    ctypedef long long INT64
    ctypedef Py_ssize_t INT_PTR
    ctypedef unsigned int UINT
    ctypedef unsigned int UINT32
    ctypedef unsigned long long UINT64
    ctypedef unsigned long long SIZE_T
    ctypedef SIZE_T *PSIZE_T
    ctypedef Py_ssize_t UINT_PTR
    ctypedef long LONG
    ctypedef LONG *PLONG
    ctypedef long LONG32
    ctypedef long long LONGLONG
    ctypedef long long LONG64
    ctypedef LONGLONG USN
    ctypedef unsigned long ULONG
    ctypedef ULONG ACCESS_MASK
    ctypedef Py_ssize_t ULONG_PTR
    ctypedef Py_ssize_t PULONG_PTR
    ctypedef ULONG_PTR KAFFINITY
    ctypedef ULONG* PULONG
    ctypedef unsigned long long ULONGLONG
    ctypedef unsigned long long ULONG64
    ctypedef ULONGLONG DWORDLONG
    ctypedef ULONGLONG* PDWORDLONG
    ctypedef ULONG64* PULONG64
    ctypedef ULONGLONG* PULONGLONG
    ctypedef unsigned long DWORD
    ctypedef DWORD* PDWORD
    ctypedef DWORD* LPDWORD
    ctypedef DWORD* DWORD_PTR
    ctypedef unsigned int DWORD32
    ctypedef unsigned long long DWORD64
    ctypedef long long WORD64
    ctypedef WORD64 *PWORD64
    ctypedef long long __int64
    ctypedef Py_ssize_t PVOID
    ctypedef Py_ssize_t LPVOID
    ctypedef unsigned long long PVOID64
    ctypedef const void * LPCVOID
    ctypedef PVOID INIT_ONCE
    ctypedef PVOID* PINIT_ONCE

    ctypedef Py_ssize_t HANDLE
    ctypedef HANDLE HDC
    ctypedef HANDLE HWND
    ctypedef HANDLE HRGN
    ctypedef HANDLE HGDIOBJ
    ctypedef HANDLE HMODULE
    ctypedef HANDLE *PHANDLE

    ctypedef void* _HANDLE
    ctypedef _HANDLE _HBITMAP

    ctypedef struct IID:
        unsigned long x
        unsigned short s1
        unsigned short s2
        unsigned char c[8]
    ctypedef IID CLSID
    ctypedef CLSID* PCLSID
    ctypedef IID* REFIID

    ctypedef struct GUID:
        ULONG   Data1
        USHORT  Data2
        USHORT  Data3
        UCHAR   Data4[8]
    ctypedef GUID* PGUID

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

    ctypedef struct SYSTEM_INFO:
        DWORD dwOemId
        WORD wProcessorArchitecture
        WORD wReserved
        DWORD dwPageSize
        LPVOID lpMinimumApplicationAddress
        LPVOID lpMaximumApplicationAddress
        DWORD_PTR dwActiveProcessorMask
        DWORD dwNumberOfProcessors
        DWORD dwProcessorType
        DWORD dwAllocationGranularity
        WORD wProcessorLevel
        WORD wProcessorRevision
    ctypedef SYSTEM_INFO *PSYSTEM_INFO
    ctypedef SYSTEM_INFO *LPSYSTEM_INFO

    ctypedef enum PROCESSOR_ARCHITECTURE:
        PROCESSOR_ARCHITECTURE_AMD64    = 9
        PROCESSOR_ARCHITECTURE_ARM      = 5
        PROCESSOR_ARCHITECTURE_IA64     = 6
        PROCESSOR_ARCHITECTURE_INTEL    = 0
        PROCESSOR_ARCHITECTURE_UNKNOWN  = 0xffff

    ctypedef struct PERFORMANCE_INFORMATION:
        DWORD  cb
        SIZE_T CommitTotal
        SIZE_T CommitLimit
        SIZE_T CommitPeak
        SIZE_T PhysicalTotal
        SIZE_T PhysicalAvailable
        SIZE_T SystemCache
        SIZE_T KernelTotal
        SIZE_T KernelPaged
        SIZE_T KernelNonpaged
        SIZE_T PageSize
        DWORD  HandleCount
        DWORD  ProcessCount
        DWORD  ThreadCount
    ctypedef PERFORMANCE_INFORMATION *PPERFORMANCE_INFORMATION

    ctypedef struct PROCESS_MEMORY_COUNTERS:
        DWORD  cb
        DWORD  PageFaultCount
        SIZE_T PeakWorkingSetSize
        SIZE_T WorkingSetSize
        SIZE_T QuotaPeakPagedPoolUsage
        SIZE_T QuotaPagedPoolUsage
        SIZE_T QuotaPeakNonPagedPoolUsage
        SIZE_T QuotaNonPagedPoolUsage
        SIZE_T PagefileUsage
        SIZE_T PeakPagefileUsage
    ctypedef PROCESS_MEMORY_COUNTERS *PPROCESS_MEMORY_COUNTERS

    ctypedef struct PROCESS_MEMORY_COUNTERS_EX:
        DWORD  cb
        DWORD  PageFaultCount
        SIZE_T PeakWorkingSetSize
        SIZE_T WorkingSetSize
        SIZE_T QuotaPeakPagedPoolUsage
        SIZE_T QuotaPagedPoolUsage
        SIZE_T QuotaPeakNonPagedPoolUsage
        SIZE_T QuotaNonPagedPoolUsage
        SIZE_T PagefileUsage
        SIZE_T PeakPagefileUsage
        SIZE_T PrivateUsage
    ctypedef PROCESS_MEMORY_COUNTERS_EX *PPROCESS_MEMORY_COUNTERS_EX

    ctypedef struct ENUM_PAGE_FILE_INFORMATION:
        DWORD  cb
        DWORD  Reserved
        SIZE_T TotalSize
        SIZE_T TotalInUse
        SIZE_T PeakUsage
    ctypedef ENUM_PAGE_FILE_INFORMATION *PENUM_PAGE_FILE_INFORMATION

    ctypedef struct MODULEINFO:
        LPVOID lpBaseOfDll
        DWORD SizeOfImage
        LPVOID EntryPoint
    ctypedef MODULEINFO *PMODULEINFO

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
    ctypedef ULARGE_INTEGER *PULARGE_INTEGER
    ctypedef ULARGE_INTEGER *LPULARGE_INTEGER

    ctypedef struct _LARGE_INTEGER:
        DWORD LowPart
        LONG  HighPart

    ctypedef union LARGE_INTEGER:
        DWORD LowPart
        LONG  HighPart
        _LARGE_INTEGER u
        LONGLONG QuadPart
    ctypedef LARGE_INTEGER *PLARGE_INTEGER
    ctypedef LARGE_INTEGER *LPLARGE_INTEGER

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

    ctypedef BOOL (__stdcall *PINIT_ONCE_FN)(
        PINIT_ONCE InitOnce,
        PVOID Parameter,
        PVOID *Context
    )

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

    ctypedef struct SECURITY_ATTRIBUTES:
        DWORD   nLength
        LPVOID  lpSecurityDescriptor
        BOOL    bInheritHandle
    ctypedef SECURITY_ATTRIBUTES* PSECURITY_ATTRIBUTES
    ctypedef SECURITY_ATTRIBUTES* LPSECURITY_ATTRIBUTES

    ctypedef struct OVERLAPPED:
        ULONG_PTR Internal
        ULONG_PTR InternalHigh
        DWORD Offset
        DWORD OffsetHigh
        LPVOID Pointer
        HANDLE hEvent
    ctypedef OVERLAPPED* LPOVERLAPPED

    ctypedef struct OVERLAPPED_ENTRY:
        ULONG_PTR lpCompletionKey
        LPOVERLAPPED lpOverlapped
        ULONG_PTR Internal
        DWORD dwNumberOfBytesTransferred
    ctypedef OVERLAPPED_ENTRY *POVERLAPPED_ENTRY
    ctypedef OVERLAPPED_ENTRY *LPOVERLAPPED_ENTRY

    ctypedef void (*LPOVERLAPPED_COMPLETION_ROUTINE)(
        DWORD dwErrorCode,
        DWORD dwNumberOfBytesTransferred,
        LPVOID lpOverlapped
    )

    ctypedef DWORD (__stdcall *LPTHREAD_START_ROUTINE)(
        LPVOID lpThreadParameter
    )

    ctypedef struct SIZE:
        LONG cx
        LONG cy
    ctypedef SIZE* PSIZE

    ctypedef struct RGBQUAD:
        BYTE rgbBlue
        BYTE rgbGreen
        BYTE rgbRed
        BYTE rgbReserved

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

    ctypedef struct BITMAPFILEHEADER:
        WORD  bfType
        DWORD bfSize
        WORD  bfReserved1
        WORD  bfReserved2
        DWORD bfOffBits
    ctypedef BITMAPFILEHEADER* PBITMAPFILEHEADER

    ctypedef struct BITMAPINFOHEADER:
        DWORD   biSize
        LONG    biWidth
        LONG    biHeight
        WORD    biPlanes
        WORD    biBitCount
        DWORD   biCompression
        DWORD   biSizeImage
        LONG    biXPelsPerMeter
        LONG    biYPelsPerMeter
        DWORD   biClrUsed
        DWORD   biClrImportant
    ctypedef BITMAPINFOHEADER* PBITMAPINFOHEADER

    ctypedef struct BITMAPINFO:
        BITMAPINFOHEADER bmiHeader
        RGBQUAD          bmiColors[1]
    ctypedef BITMAPINFO* PBITMAPINFO
    ctypedef BITMAPINFO* LPBITMAPINFO

    ctypedef long FXPT2DOT30
    ctypedef struct CIEXYZ:
        FXPT2DOT30 ciexyzX
        FXPT2DOT30 ciexyzY
        FXPT2DOT30 ciexyzZ

    ctypedef struct CIEXYZTRIPLE:
        CIEXYZ ciexyzRed
        CIEXYZ ciexyzGreen
        CIEXYZ ciexyzBlue

    ctypedef struct BITMAPV4HEADER:
        DWORD        bV4Size
        LONG         bV4Width
        LONG         bV4Height
        WORD         bV4Planes
        WORD         bV4BitCount
        DWORD        bV4V4Compression
        DWORD        bV4SizeImage
        LONG         bV4XPelsPerMeter
        LONG         bV4YPelsPerMeter
        DWORD        bV4ClrUsed
        DWORD        bV4ClrImportant
        DWORD        bV4RedMask
        DWORD        bV4GreenMask
        DWORD        bV4BlueMask
        DWORD        bV4AlphaMask
        DWORD        bV4CSType
        CIEXYZTRIPLE bV4Endpoints
        DWORD        bV4GammaRed
        DWORD        bV4GammaGreen
        DWORD        bV4GammaBlue
    ctypedef BITMAPV4HEADER* PBITMAPV4HEADER

    ctypedef struct BITMAPV5HEADER:
        DWORD        bV5Size
        LONG         bV5Width
        LONG         bV5Height
        WORD         bV5Planes
        WORD         bV5BitCount
        DWORD        bV5Compression
        DWORD        bV5SizeImage
        LONG         bV5XPelsPerMeter
        LONG         bV5YPelsPerMeter
        DWORD        bV5ClrUsed
        DWORD        bV5ClrImportant
        DWORD        bV5RedMask
        DWORD        bV5GreenMask
        DWORD        bV5BlueMask
        DWORD        bV5AlphaMask
        DWORD        bV5CSType
        CIEXYZTRIPLE bV5Endpoints
        DWORD        bV5GammaRed
        DWORD        bV5GammaGreen
        DWORD        bV5GammaBlue
        DWORD        bV5Intent
        DWORD        bV5ProfileData
        DWORD        bV5ProfileSize
        DWORD        bV5Reserved
    ctypedef BITMAPV5HEADER* PBITMAPV5HEADER

    ctypedef struct RECT:
        LONG left
        LONG top
        LONG right
        LONG bottom
    ctypedef RECT* PRECT
    ctypedef RECT* LPRECT
    ctypedef RECT RECTL
    ctypedef RECTL* PRECTL
    ctypedef RECTL* LPRECTL

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

    ctypedef union FILE_SEGMENT_ELEMENT:
        PVOID64 Buffer
        ULONGLONG Alignment
    ctypedef FILE_SEGMENT_ELEMENT* PFILE_SEGMENT_ELEMENT

    ctypedef enum FILE_MAPPING:
        FILE_MAP_ALL_ACCESS
        FILE_MAP_COPY
        FILE_MAP_READ
        FILE_MAP_WRITE
    DWORD FILE_MAP_EXECUTE

    ctypedef enum PAGE_PROTECTION:
        PAGE_EXECUTE_READ           = 0x20
        PAGE_EXECUTE_READWRITE      = 0x40
        PAGE_EXECUTE_WRITECOPY      = 0x80
        PAGE_READONLY               = 0x02
        PAGE_READWRITE              = 0x04
        PAGE_WRITECOPY              = 0x08

    ctypedef enum SECURITY_ATTRIBUTES:
        SEC_COMMIT                  = 0x8000000
        SEC_IMAGE                   = 0x1000000
        SEC_IMAGE_NO_EXECUTE        = 0x11000000
        SEC_LARGE_PAGES             = 0x80000000
        SEC_NOCACHE                 = 0x10000000
        SEC_RESERVE                 = 0x4000000
        SEC_WRITECOMBINE            = 0x40000000

    ctypedef enum NUMA_NODE_PREFERRED:
        NUMA_NO_PREFERRED_NODE = 0xffffffff

    ctypedef enum FILE_INFO_BY_HANDLE_CLASS:
        FileBasicInfo                   = 0
        FileStandardInfo                = 1
        FileNameInfo                    = 2
        FileRenameInfo                  = 3
        FileDispositionInfo             = 4
        FileAllocationInfo              = 5
        FileEndOfFileInfo               = 6
        FileStreamInfo                  = 7
        FileCompressionInfo             = 8
        FileAttributeTagInfo            = 9
        FileIdBothDirectoryInfo         = 10
        FileIdBothDirectoryRestartInfo  = 11
        FileIoPriorityHintInfo          = 12
        FileRemoteProtocolInfo          = 13
        FileFullDirectoryInfo           = 14
        FileFullDirectoryRestartInfo    = 15
        FileStorageInfo                 = 16
        FileAlignmentInfo               = 17
        FileIdInfo                      = 18
        FileIdExtdDirectoryInfo         = 19
        FileIdExtdDirectoryRestartInfo  = 20
        MaximumFileInfoByHandlesClass
    ctypedef FILE_INFO_BY_HANDLE_CLASS *PFILE_INFO_BY_HANDLE_CLASS

    ctypedef struct FILE_BASIC_INFO:
        LARGE_INTEGER CreationTime
        LARGE_INTEGER LastAccessTime
        LARGE_INTEGER LastWriteTime
        LARGE_INTEGER ChangeTime
        DWORD         FileAttributes
    ctypedef FILE_BASIC_INFO *PFILE_BASIC_INFO

    ctypedef struct FILE_ALIGNMENT_INFO:
        ULONG AlignmentRequirement
    ctypedef FILE_ALIGNMENT_INFO *PFILE_ALIGNMENT_INFO

    ctypedef struct BY_HANDLE_FILE_INFORMATION:
        DWORD    dwFileAttributes
        FILETIME ftCreationTime
        FILETIME ftLastAccessTime
        FILETIME ftLastWriteTime
        DWORD    dwVolumeSerialNumber
        DWORD    nFileSizeHigh
        DWORD    nFileSizeLow
        DWORD    nNumberOfLinks
        DWORD    nFileIndexHigh
        DWORD    nFileIndexLow
    ctypedef BY_HANDLE_FILE_INFORMATION *PBY_HANDLE_FILE_INFORMATION
    ctypedef BY_HANDLE_FILE_INFORMATION *LPBY_HANDLE_FILE_INFORMATION

    ctypedef struct MEMORY_BASIC_INFORMATION:
        PVOID  BaseAddress
        PVOID  AllocationBase
        DWORD  AllocationProtect
        SIZE_T RegionSize
        DWORD  State
        DWORD  Protect
        DWORD  Type
    ctypedef MEMORY_BASIC_INFORMATION *PMEMORY_BASIC_INFORMATION

    ctypedef enum MEMORY_ALLOCATION_TYPE:
        MEM_COMMIT          = 0x00001000
        MEM_RESERVE         = 0x00002000
        MEM_RESET           = 0x00080000
        MEM_RESET_UNDO      = 0x1000000
        MEM_LARGE_PAGES     = 0x20000000
        MEM_PHYSICAL        = 0x00400000
        MEM_TOP_DOWN        = 0x00100000
        MEM_WRITE_WATCH     = 0x00200000

    ctypedef enum MEMORY_FREE_TYPE:
        MEM_DECOMMIT        = 0x4000
        MEM_RELEASE         = 0x8000

    ctypedef struct MEMORYSTATUSEX:
        DWORD     dwLength
        DWORD     dwMemoryLoad
        DWORDLONG ullTotalPhys
        DWORDLONG ullAvailPhys
        DWORDLONG ullTotalPageFile
        DWORDLONG ullAvailPageFile
        DWORDLONG ullTotalVirtual
        DWORDLONG ullAvailVirtual
        DWORDLONG ullAvailExtendedVirtual
    ctypedef MEMORYSTATUSEX *LPMEMORYSTATUSEX

    ctypedef struct HEAP_OPTIMIZE_RESOURCES_INFORMATION:
        ULONG Version;
        ULONG Flags;
    ctypedef HEAP_OPTIMIZE_RESOURCES_INFORMATION *PHEAP_OPTIMIZE_RESOURCES_INFORMATION

    ctypedef struct M128A:
        ULONGLONG Low
        LONGLONG High
    ctypedef M128A *PM128A

    ctypedef struct XSAVE_FORMAT:
        USHORT ControlWord
        USHORT StatusWord
        UCHAR TagWord
        UCHAR Reserved1
        USHORT ErrorOpcode
        ULONG ErrorOffset
        USHORT ErrorSelector
        USHORT Reserved2
        ULONG DataOffset
        USHORT DataSelector
        USHORT Reserved3
        ULONG MxCsr
        ULONG MxCsr_Mask
        M128A FloatRegisters[8]
    IF UNAME_MACHINE[-2:] == 'x64':
        M128A XmmRegisters[16]
        UCHAR Reserved4[96]
    ELSE:
        M128A XmmRegisters[8]
        UCHAR Reserved4[224]
    ctypedef XSAVE_FORMAT *PXSAVE_FORMAT
    ctypedef XSAVE_FORMAT XMM_SAVE_AREA32

    ctypedef struct CONTEXT:
        DWORD64 P1Home
        DWORD64 P2Home
        DWORD64 P3Home
        DWORD64 P4Home
        DWORD64 P5Home
        DWORD64 P6Home
        DWORD ContextFlags
        DWORD MxCsr
        WORD SegCs
        WORD SegDs
        WORD SegEs
        WORD SegFs
        WORD SegGs
        WORD SegSs
        DWORD EFlags
        DWORD64 Dr0
        DWORD64 Dr1
        DWORD64 Dr2
        DWORD64 Dr3
        DWORD64 Dr6
        DWORD64 Dr7
        DWORD64 Rax
        DWORD64 Rcx
        DWORD64 Rdx
        DWORD64 Rbx
        DWORD64 Rsp
        DWORD64 Rbp
        DWORD64 Rsi
        DWORD64 Rdi
        DWORD64 R8
        DWORD64 R9
        DWORD64 R10
        DWORD64 R11
        DWORD64 R12
        DWORD64 R13
        DWORD64 R14
        DWORD64 R15
        DWORD64 Rip
        XMM_SAVE_AREA32 FltSave
        M128A Header[2]
        M128A Legacy[8]
        M128A Xmm0
        M128A Xmm1
        M128A Xmm2
        M128A Xmm3
        M128A Xmm4
        M128A Xmm5
        M128A Xmm6
        M128A Xmm7
        M128A Xmm8
        M128A Xmm9
        M128A Xmm10
        M128A Xmm11
        M128A Xmm12
        M128A Xmm13
        M128A Xmm14
        M128A Xmm15
        M128A VectorRegister[26]
        DWORD64 VectorControl
        DWORD64 DebugControl
        DWORD64 LastBranchToRip
        DWORD64 LastBranchFromRip
        DWORD64 LastExceptionToRip
        DWORD64 LastExceptionFromRip
    ctypedef CONTEXT *PCONTEXT
    ctypedef CONTEXT *LPCONTEXT

    ctypedef struct LDT_ENTRY:
        WORD  LimitLow
        WORD  BaseLow
        BYTE BaseMid
        BYTE Flags1
        BYTE Flags2
        DWORD Type
        DWORD Dpl
        DWORD Pres
        DWORD LimitHi
        DWORD Sys
        DWORD Reserved_0
        DWORD Default_Big
        DWORD Granularity
        DWORD BaseHi
    ctypedef LDT_ENTRY *PLDT_ENTRY
    ctypedef LDT_ENTRY *LPLDT_ENTRY

    ctypedef struct RIO_BUFFERID_t:
        pass
    ctypedef RIO_BUFFERID_t *RIO_BUFFERID
    ctypedef RIO_BUFFERID *PRIO_BUFFERID

    ctypedef struct RIO_CQ_t:
        pass
    ctypedef RIO_CQ_t *RIO_CQ
    ctypedef RIO_CQ *PRIO_CQ

    ctypedef struct RIO_RQ_t:
        pass
    ctypedef RIO_RQ_t *RIO_RQ
    ctypedef RIO_RQ *PRIO_RQ

    ctypedef struct RIO_BUF:
        RIO_BUFFERID BufferId
        ULONG Offset
        ULONG Length
    ctypedef RIO_BUF *PRIO_BUF

    ctypedef struct WSACMSGHDR:
        UINT cmsg_len
        INT  cmsg_level
        INT  cmsg_type

    ctypedef struct SOCKADDR:
        USHORT sa_family
        CHAR   sa_data[14]

    ctypedef struct _S_un_b:
        UCHAR s_b1
        UCHAR s_b2
        UCHAR s_b3
        UCHAR s_b4

    ctypedef struct _S_un_w:
        USHORT s_w1
        USHORT s_w2

    ctypedef union _S_un:
        _S_un_b S_un_b
        _S_un_w S_un_w
        ULONG S_addr

    ctypedef struct IN_ADDR:
        _S_un S_un
        UCHAR s_b1
        UCHAR s_b2
        UCHAR s_b3
        UCHAR s_b4
        USHORT s_w1
        USHORT s_w2
        ULONG s_addr
        UCHAR s_host
        UCHAR s_net
        USHORT s_imp
        UCHAR s_impno
        UCHAR s_lh
    ctypedef IN_ADDR *PIN_ADDR
    ctypedef IN_ADDR *LPIN_ADDR

    ctypedef SHORT ADDRESS_FAMILY
    ctypedef struct SOCKADDR_IN:
        ADDRESS_FAMILY sa_family
        CHAR sa_data[14]
        IN_ADDR sin_addr
        CHAR sin_zero[8]

    ctypedef struct SOCKADDR_IN6:
        pass

    ctypedef union SOCKADDR_INET:
        SOCKADDR_IN Ipv4
        SOCKADDR_IN6 Ipv6
        ADDRESS_FAMILY si_family
    ctypedef SOCKADDR_INET *PSOCKADDR_INET

    ctypedef enum RIO_NOTIFICATION_COMPLETION_TYPE:
        RIO_EVENT_COMPLETION = 1
        RIO_IOCP_COMPLETION  = 2

    ctypedef struct RIO_NOTIFICATION_COMPLETION:
        RIO_NOTIFICATION_COMPLETION_TYPE Type
        HANDLE EventHandle
        BOOL   NotifyReset
        HANDLE IocpHandle
        PVOID  CompletionKey
        PVOID  Overlapped
    ctypedef RIO_NOTIFICATION_COMPLETION *PRIO_NOTIFICATION_COMPLETION

    ctypedef struct RIORESULT:
        LONG Status
        ULONG BytesTransferred
        ULONGLONG SocketContext
        ULONGLONG RequestContext
    ctypedef RIORESULT *PRIORESULT

    ctypedef struct TRANSMIT_FILE_BUFFERS:
        PVOID Head
        DWORD HeadLength
        PVOID Tail
        DWORD TailLength
    ctypedef TRANSMIT_FILE_BUFFERS *PTRANSMIT_FILE_BUFFERS

    ctypedef enum KPROFILE_SOURCE:
        ProfileTime
        ProfileAlignmentFixup
        ProfileTotalIssues
        ProfilePipelineDry
        ProfileLoadInstructions
        ProfilePipelineFrozen
        ProfileBranchInstructions
        ProfileTotalNonissues
        ProfileDcacheMisses
        ProfileIcacheMisses
        ProfileCacheMisses
        ProfileBranchMispredictions
        ProfileStoreInstructions
        ProfileFpInstructions
        ProfileIntegerInstructions
        Profile2Issue
        Profile3Issue
        Profile4Issue
        ProfileSpecialInstructions
        ProfileTotalCycles
        ProfileIcacheIssues
        ProfileDcacheAccesses
        ProfileMemoryBarrierCycles
        ProfileLoadLinkedIssues
        ProfileMaximum



    DWORD CREATE_ALWAYS
    DWORD CREATE_NEW
    DWORD OPEN_ALWAYS
    DWORD OPEN_EXISTING
    DWORD TRUNCATE_EXISTING

    DWORD GENERIC_WRITE
    DWORD FILE_ATTRIBUTE_NORMAL
# vim: set ts=8 sw=4 sts=4 tw=80 et nospell:                                   #
