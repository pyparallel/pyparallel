from types cimport *

cdef extern from *:

    BOOL FileTimeToSystemTime(PFILETIME filetime, PSYSTEMTIME systime)
    void GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
    void GetSystemTimePreciseAsFileTime(LPFILETIME lpSystemTimeAsFileTime)

    HANDLE __stdcall CreateFile(
        LPCTSTR lpFileName,
        DWORD dwDesiredAccess,
        DWORD dwShareMode,
        LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        DWORD dwCreationDisposition,
        DWORD dwFlagsAndAttributes,
        HANDLE hTemplateFile
    )

    HANDLE __stdcall CreateFileW(
        LPCWSTR lpFileName,
        DWORD dwDesiredAccess,
        DWORD dwShareMode,
        LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        DWORD dwCreationDisposition,
        DWORD dwFlagsAndAttributes,
        HANDLE hTemplateFile
    )


    BOOL __stdcall ReadFile(
        HANDLE hFile,
        LPVOID lpBuffer,
        DWORD nNumberOfBytesToRead,
        LPDWORD lpNumberOfBytesRead,
        LPOVERLAPPED lpOverlapped
    )

    BOOL __stdcall ReadFileEx(
        HANDLE hFile,
        LPVOID lpBuffer,
        DWORD nNumberOfBytesToRead,
        LPOVERLAPPED lpOverlapped,
        LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    )

    BOOL __stdcall ReadFileScatter(
        HANDLE hFile,
        FILE_SEGMENT_ELEMENT aSegmentArray[],
        DWORD nNumberOfBytesToRead,
        LPDWORD lpReserved,
        LPOVERLAPPED lpOverlapped
    )

    BOOL __stdcall WriteFile(
        HANDLE hFile,
        LPCVOID lpBuffer,
        DWORD nNumberOfBytesToWrite,
        LPDWORD lpNumberOfBytesWritten,
        LPOVERLAPPED lpOverlapped
    )

    BOOL __stdcall WriteFileEx(
        HANDLE hFile,
        LPCVOID lpBuffer,
        DWORD nNumberOfBytesToWrite,
        LPOVERLAPPED lpOverlapped,
        LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    )

    BOOL __stdcall WriteFileGather(
        HANDLE hFile,
        FILE_SEGMENT_ELEMENT aSegmentArray[],
        DWORD nNumberOfBytesToWrite,
        LPDWORD lpReserved,
        LPOVERLAPPED lpOverlapped
    )

    BOOL __stdcall SetEndOfFile(HANDLE hFile)

    HANDLE __stdcall CreateFileMapping(
        HANDLE hFile,
        LPSECURITY_ATTRIBUTES lpAttributes,
        DWORD flProtect,
        DWORD dwMaximumSizeHigh,
        DWORD dwMaximumSizeLow,
        LPCTSTR lpName
    )

    HANDLE __stdcall CreateFileMappingW(
        HANDLE hFile,
        LPSECURITY_ATTRIBUTES lpAttributes,
        DWORD flProtect,
        DWORD dwMaximumSizeHigh,
        DWORD dwMaximumSizeLow,
        LPCWSTR lpName
    )

    ctypedef enum MOVE_METHOD:
        FILE_BEGIN      = 0
        FILE_CURRENT    = 1
        FILE_END        = 2

    DWORD __stdcall SetFilePointer(
        HANDLE hFile,
        LONG lDistanceToMove,
        PLONG lpDistanceToMoveHigh,
        DWORD dwMoveMethod
    )

    BOOL __stdcall SetFilePointerEx(
        HANDLE hFile,
        LARGE_INTEGER liDistanceToMove,
        PLARGE_INTEGER lpNewFilePointer,
        DWORD dwMoveMethod
    )

    BOOL __stdcall SetFileValidData(
        HANDLE hFile,
        LONGLONG ValidDataLength
    )

    HMODULE __stdcall GetModuleHandle(LPCTSTR lpModuleName)

    BOOL __stdcall GetDiskFreeSpace(
        LPCTSTR lpRootPathName,
        LPDWORD lpSectorsPerCluster,
        LPDWORD lpBytesPerSector,
        LPDWORD lpNumberOfFreeClusters,
        LPDWORD lpTotalNumberOfClusters
    )

    BOOL __stdcall GetDiskFreeSpaceEx(
        LPCTSTR lpDirectoryName,
        PULARGE_INTEGER lpFreeBytesAvailable,
        PULARGE_INTEGER lpTotalNumberOfBytes,
        PULARGE_INTEGER lpTotalNumberOfFreeBytes
    )

    BOOL __stdcall GetFileInformationByHandle(
        HANDLE hFile,
        LPBY_HANDLE_FILE_INFORMATION lpFileInformation
    )

    BOOL __stdcall GetFileInformationByHandleEx(
        HANDLE hFile,
        FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
        LPVOID lpFileInformation,
        DWORD dwBufferSize
    )


# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:

