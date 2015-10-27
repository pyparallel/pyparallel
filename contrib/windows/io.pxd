# WinBase.h
from types cimport *

cdef extern from *:

    HANDLE __stdcall CreateIoCompletionPort(
        HANDLE FileHandle,
        HANDLE ExistingCompletionPort,
        ULONG_PTR CompletionKey,
        DWORD NumberOfConcurrentThreads
    )

    BOOL __stdcall PostQueuedCompletionStatus(
        HANDLE CompletionPort,
        DWORD dwNumberOfBytesTransferred,
        ULONG_PTR dwCompletionKey,
        LPOVERLAPPED lpOverlapped
    )

    BOOL __stdcall GetQueuedCompletionStatusEx(
        HANDLE CompletionPort,
        LPOVERLAPPED_ENTRY lpCompletionPortEntries,
        ULONG ulCount,
        PULONG ulNumEntriesRemoved,
        DWORD dwMilliseconds,
        BOOL fAlertable
    )

    BOOL __stdcall GetOverlappedResult(
        HANDLE hFile,
        LPOVERLAPPED lpOverlapped,
        LPDWORD lpNumberOfBytesTransferred,
        BOOL bWait
    )

    BOOL __stdcall CancelIoEx(
        HANDLE hFile,
        LPOVERLAPPED lpOverlapped
    )

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
