#processthreadsapi.h
from types cimport *

cdef extern from "<windows.h>":

    BOOL __stdcall GetSystemCpuSetInformation(
        PSYSTEM_CPU_SET_INFORMATION Information,
        ULONG BufferLength,
        PULONG ReturnedLength,
        HANDLE Process,
        ULONG Flags
    )

    DWORD __stdcall GetCurrentProcessId()
    DWORD __stdcall GetCurrentThreadId()
    HANDLE __stdcall GetCurrentProcess()
    HANDLE __stdcall GetCurrentThread()

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

    BOOL __stdcall GetThreadTimes(
        HANDLE hThread,
        LPFILETIME lpCreationTime,
        LPFILETIME lpExitTime,
        LPFILETIME lpKernelTime,
        LPFILETIME lpUserTime
    )

    BOOL __stdcall GetProcessHandleCount(
        HANDLE hProcess,
        PDWORD pdwHandleCount
    )

    BOOL __stdcall SetProcessWorkingSetSize(
        HANDLE hProcess,
        SIZE_T dwMinimumWorkingSetSize,
        SIZE_T dwMaximumWorkingSetSize
    )

    BOOL __stdcall GetProcessWorkingSetSize(
        HANDLE hProcess,
        PSIZE_T lpMinimumWorkingSetSize,
        PSIZE_T lpMaximumWorkingSetSize
    )

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
