# WinBase.h
from types cimport *

cdef extern from *:

    BOOL __stdcall GetProcessInformation(
        HANDLE hProcess,
        PROCESS_INFORMATION_CLASS ProcessInformationClass,
        PMEMORY_PRIORITY_INFORMATION ProcessInformation,
        DWORD ProcessInformationSize
    )

    BOOL __stdcall FlushViewOfFile(
        LPCVOID     lpBaseAddress,
        Py_ssize_t  dwNumberOfBytesToFlush
    )

    BOOL __stdcall GetQueuedCompletionStatus(
        HANDLE CompletionPort,
        LPDWORD lpNumberOfBytes,
        PULONG_PTR lpCompletionKey,
        LPOVERLAPPED *lpOverlapped,
        DWORD dwMilliseconds
    )

    BOOL __stdcall SystemTimeToFileTime(
        SYSTEMTIME *lpSystemTime,
        LPFILETIME lpFileTime
    )

    void __stdcall GetSystemTimeAsFileTime(
        LPFILETIME lpSystemTimeAsFileTime
    )

    void __stdcall GetSystemTimePreciseAsFileTime(
        LPFILETIME lpSystemTimeAsFileTime
    )

    BOOL GetProcessorSystemCycleTime(
        USHORT Group,
        PSYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION Buffer,
        PDWORD ReturnedLength
    )

    BOOL __stdcall QueryIdleProcessorCycleTime(
        PULONG BufferLength,
        PULONG64 ProcessorIdleCycleTime
    )

    BOOL QueryIdleProcessorCycleTimeEx(
        USHORT Group,
        PULONG BufferLength,
        PULONG64 ProcessorIdleCycleTime
    )

    BOOL __stdcall QueryProcessCycleTime(
        HANDLE ProcessHandle,
        PULONG64 CycleTime
    )

    BOOL __stdcall QueryThreadCycleTime(
        HANDLE ThreadHandle,
        PULONG64 CycleTime
    )

    BOOL __stdcall GetProcessIoCounters(
        HANDLE hProcess,
        PIO_COUNTERS lpIoCounters
    )

    DWORD GetActiveProcessorCount(WORD GroupNumber)

    BOOL __stdcall GetNumaAvailableMemoryNode(
        UCHAR Node,
        PULONGLONG AvailableBytes
    )

    BOOL __stdcall GetNumaAvailableMemoryNodeEx(
        USHORT Node,
        PULONGLONG AvailableBytes
    )

    BOOL GetLogicalProcessorInformationEx(
        LOGICAL_PROCESSOR_RELATIONSHIP RelationshipType,
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX Buffer,
        PDWORD ReturnedLength
    )

    void CopyMemory(
        PVOID Destination,
        const void *Source,
        Py_ssize_t Length
    )

    PVOID SecureZeroMemory(void *buf, Py_ssize_t cnt)

    BOOL __stdcall SetFileIoOverlappedRange(
        HANDLE FileHandle,
        PUCHAR OverlappedRangeStart,
        ULONG Length
    )

    BOOL __stdcall CloseHandle(HANDLE hObject)

    BOOL DeviceIoControl(
        HANDLE hDevice,
        DWORD dwIoControlCode,
        LPVOID lpInBuffer,
        DWORD nInBufferSize,
        LPVOID lpOutBuffer,
        DWORD nOutBufferSize,
        LPDWORD lpBytesReturned,
        LPOVERLAPPED lpOverlapped
    )

    BOOL HasOverlappedIoCompleted(LPOVERLAPPED lpOverlapped)

    DWORD __stdcall GetCompressedFileSize(
        LPCTSTR lpFileName,
        LPDWORD lpFileSizeHigh
    )

    HANDLE __stdcall CreateFileMappingNuma(
        HANDLE hFile,
        LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
        DWORD flProtect,
        DWORD dwMaximumSizeHigh,
        DWORD dwMaximumSizeLow,
        LPCTSTR lpName,
        DWORD nndPreferred
    )

    HANDLE __stdcall OpenFileMapping(
        DWORD dwDesiredAccess,
        BOOL bInheritHandle,
        LPCTSTR lpName
    )

    LPVOID __stdcall MapViewOfFile(
        HANDLE hFileMappingObject,
        DWORD dwDesiredAccess,
        DWORD dwFileOffsetHigh,
        DWORD dwFileOffsetLow,
        SIZE_T dwNumberOfBytesToMap,
        LPVOID lpBaseAddress
    )

    LPVOID __stdcall MapViewOfFileExNuma(
        HANDLE hFileMappingObject,
        DWORD dwDesiredAccess,
        DWORD dwFileOffsetHigh,
        DWORD dwFileOffsetLow,
        SIZE_T dwNumberOfBytesToMap,
        LPVOID lpBaseAddress,
        DWORD nndPreferred
    )

    BOOL __stdcall UnmapViewOfFile(LPCVOID lpBaseAddress)

    void __stdcall GetSystemInfo(LPSYSTEM_INFO lpSystemInfo)

    BOOL __stdcall GetPhysicallyInstalledSystemMemory(
        PULONGLONG TotalMemoryInKilobytes
    )

    BOOL __stdcall SetThreadContext(
        HANDLE hThread,
        const CONTEXT *lpContext
    )

    BOOL __stdcall GetThreadContext(
        HANDLE hThread,
        LPCONTEXT lpContext
    )

    BOOL __stdcall InitializeContext(
        PVOID Buffer,
        DWORD ContextFlags,
        PCONTEXT *Context,
        PWORD ContextLength
    )

    BOOL __stdcall CopyContext(
        PCONTEXT Destination,
        DWORD ContextFlags,
        PCONTEXT Source
    )

    DWORD64 __stdcall GetEnabledXStateFeatures()

    BOOL __stdcall GetXStateFeaturesMask(
        PCONTEXT Context,
        PWORD64 FeatureMask
    )

    PVOID __stdcall LocateXStateContext(
        PCONTEXT Context,
        DWORD FeatureId,
        PDWORD Length
    )

    BOOL __stdcall SetXStateFeaturesMask(
        PCONTEXT Context,
        DWORD64 FeatureMask
    )

    BOOL __stdcall GetThreadSelectorEntry(
        HANDLE hThread,
        DWORD dwSelector,
        LPLDT_ENTRY lpSelectorEntry
    )

    BOOL __stdcall FlushInstructionCache(
        HANDLE hProcess,
        LPCVOID lpBaseAddress,
        SIZE_T dwSize
    )

    SIZE_T __stdcall GetLargePageMinimum()

    LPVOID __stdcall VirtualAlloc(
        LPVOID lpAddress,
        SIZE_T dwSize,
        DWORD flAllocationType,
        DWORD flProtect
    )

    LPVOID __stdcall VirtualAllocEx(
        HANDLE hProcess,
        LPVOID lpAddress,
        SIZE_T dwSize,
        DWORD flAllocationType,
        DWORD flProtect
    )

    LPVOID __stdcall VirtualAllocExNuma(
        HANDLE hProcess,
        LPVOID lpAddress,
        SIZE_T dwSize,
        DWORD flAllocationType,
        DWORD flProtect,
        DWORD nndPreferred
    )

    SIZE_T __stdcall VirtualQuery(
        LPCVOID lpAddress,
        PMEMORY_BASIC_INFORMATION lpBuffer,
        SIZE_T dwLength
    )

    SIZE_T __stdcall VirtualQueryEx(
        HANDLE hProcess,
        LPCVOID lpAddress,
        PMEMORY_BASIC_INFORMATION lpBuffer,
        SIZE_T dwLength
    )

    BOOL __stdcall VirtualProtect(
        LPVOID lpAddress,
        SIZE_T dwSize,
        DWORD flNewProtect,
        PDWORD lpflOldProtect
    )

    BOOL __stdcall VirtualProtect(
        HANDLE hProcess,
        LPVOID lpAddress,
        SIZE_T dwSize,
        DWORD flNewProtect,
        PDWORD lpflOldProtect
    )

    BOOL __stdcall VirtualLock(
        LPVOID lpAddress,
        SIZE_T dwSize
    )

    BOOL __stdcall VirtualUnlock(
        LPVOID lpAddress,
        SIZE_T dwSize
    )

    BOOL __stdcall VirtualFree(
        LPVOID lpAddress,
        SIZE_T dwSize,
        DWORD dwFreeType
    )

    BOOL __stdcall VirtualFreeEx(
        HANDLE hProcess,
        LPVOID lpAddress,
        SIZE_T dwSize,
        DWORD dwFreeType
    )

    BOOL __stdcall MapUserPhysicalPages(
        PVOID lpAddress,
        ULONG_PTR NumberOfPages,
        PULONG_PTR UserPfnArray
    )

    BOOL __stdcall MapUserPhysicalPagesScatter(
        PVOID *VirtualAddresses,
        ULONG_PTR NumberOfPages,
        PULONG_PTR PageArray
    )

    BOOL __stdcall AllocateUserPhysicalPages(
        HANDLE hProcess,
        PULONG_PTR NumberOfPages,
        PULONG_PTR UserPfnArray
    )

    BOOL __stdcall AllocateUserPhysicalPagesNuma(
        HANDLE hProcess,
        PULONG_PTR NumberOfPages,
        PULONG_PTR UserPfnArray,
        DWORD nndPreferred
    )

    BOOL __stdcall FreeUserPhysicalPages(
        HANDLE hProcess,
        PULONG_PTR NumberOfPages,
        PULONG_PTR UserPfnArray
    )

    BOOL __stdcall OpenProcessToken(
        HANDLE ProcessHandle,
        DWORD DesiredAccess,
        PHANDLE TokenHandle
    )

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
