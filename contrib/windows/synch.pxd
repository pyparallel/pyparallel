# synchapi.h

cdef extern from "<windows.h>":

    void __stdcall InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
    BOOL __stdcall InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount)
    void __stdcall EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
    BOOL __stdcall TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
    void __stdcall LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
    void __stdcall DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
