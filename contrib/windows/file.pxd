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

    DWORD CREATE_ALWAYS
    DWORD CREATE_NEW
    DWORD OPEN_ALWAYS
    DWORD OPEN_EXISTING
    DWORD TRUNCATE_EXISTING

    DWORD GENERIC_WRITE
    DWORD FILE_ATTRIBUTE_NORMAL

    BOOL __stdcall WriteFile(
        HANDLE hFile,
        LPCVOID lpBuffer,
        DWORD nNumberOfBytesToWrite,
        LPDWORD lpNumberOfBytesWritten,
        LPOVERLAPPED lpOverlapped
    )

# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:

