from types cimport *

cdef extern from "<windows.h>":
    HANDLE __stdcall CreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID)

    ctypedef enum TH32CS_FLAGS:
        TH32CS_INHERIT      = 0x80000000
        TH32CS_SNAPHEAPLIST = 0x00000001
        TH32CS_SNAPMODULE   = 0x00000008
        TH32CS_SNAPMODULE32 = 0x00000010
        TH32CS_SNAPPROCESS  = 0x00000002
        TH32CS_SNAPTHREAD   = 0x00000004

    BOOL __stdcall Thread32First(HANDLE hSnapshot, LPTHREADENTRY32 lpte)
    BOOL __stdcall Thread32Next(HANDLE hSnapshot, LPTHREADENTRY32 lpte)

    BOOL __stdcall Heap32ListFirst(HANDLE hSnapshot, LPHEAPLIST32 lphl)
    BOOL __stdcall Heap32ListNext(HANDLE hSnapshot, LPHEAPLIST32 lphl)

    BOOL __stdcall Heap32First(LPHEAPENTRY32 lphe, DWORD th32ProcessID, ULONG_PTR th32HeapID)
    BOOL __stdcall Heap32Next(LPHEAPENTRY32 lphe)



# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:
