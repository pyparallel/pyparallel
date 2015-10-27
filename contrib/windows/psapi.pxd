from types cimport *

cdef extern from *:

    BOOL __stdcall GetModuleInformation(
        HANDLE hProcess,
        HMODULE hModule,
        LPMODULEINFO lpmodinfo,
        DWORD cb
    )

    BOOL __stdcall GetPerformanceInfo(
        PPERFORMANCE_INFORMATION pPerformanceInformation,
        DWORD cb
    )

    BOOL __stdcall GetProcessMemoryInfo(
        HANDLE Process,
        PPROCESS_MEMORY_COUNTERS ppsmemCounters,
        DWORD cb
    )

# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:

