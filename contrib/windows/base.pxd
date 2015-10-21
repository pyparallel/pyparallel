# WinBase.h
from types cimport *

cdef extern from *:

    BOOL __stdcall GetProcessInformation(HANDLE hProcess, PROCESS_INFORMATION_CLASS ProcessInformationClass, PMEMORY_PRIORITY_INFORMATION ProcessInformation, DWORD ProcessInformationSize)

    BOOL __stdcall SystemTimeToFileTime(SYSTEMTIME *lpSystemTime, LPFILETIME lpFileTime)
    void __stdcall GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
    void __stdcall GetSystemTimePreciseAsFileTime(LPFILETIME lpSystemTimeAsFileTime)

    BOOL GetProcessorSystemCycleTime(USHORT Group, PSYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION Buffer, PDWORD ReturnedLength)

    BOOL __stdcall QueryIdleProcessorCycleTime(PULONG BufferLength, PULONG64 ProcessorIdleCycleTime)
    BOOL QueryIdleProcessorCycleTimeEx(USHORT Group, PULONG BufferLength, PULONG64 ProcessorIdleCycleTime)

    BOOL __stdcall QueryProcessCycleTime(HANDLE ProcessHandle, PULONG64 CycleTime)
    BOOL __stdcall QueryThreadCycleTime(HANDLE ThreadHandle, PULONG64 CycleTime)

    BOOL __stdcall GetProcessIoCounters(HANDLE hProcess, PIO_COUNTERS lpIoCounters)
    DWORD GetActiveProcessorCount(WORD GroupNumber)

    BOOL __stdcall GetNumaAvailableMemoryNode(UCHAR Node, PULONGLONG AvailableBytes)
    BOOL __stdcall GetNumaAvailableMemoryNodeEx(USHORT Node, PULONGLONG AvailableBytes)

    BOOL GetLogicalProcessorInformationEx(LOGICAL_PROCESSOR_RELATIONSHIP RelationshipType, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX Buffer, PDWORD ReturnedLength)

    PVOID SecureZeroMemory(void *buf, Py_ssize_t cnt)

    BOOL __stdcall CloseHandle(HANDLE hObject)

# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:
