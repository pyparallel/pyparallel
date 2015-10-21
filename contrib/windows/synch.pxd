# synchapi.h
from types cimport *

cdef extern from *:
    # Init Once
    BOOL __stdcall InitOnceExecuteOnce(PINIT_ONCE InitOnce, PINIT_ONCE_FN InitFn, PVOID Parameter, LPVOID Context)
    void __stdcall InitOnceInitialize(PINIT_ONCE InitOnce)

    # Critical Sections
    void __stdcall InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
    BOOL __stdcall InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount)
    void __stdcall EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
    BOOL __stdcall TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
    void __stdcall LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
    void __stdcall DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)

# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:
