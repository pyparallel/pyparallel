from types cimport *

cdef extern from "<windows.h>":
    HANDLE __stdcall AvSetMmThreadCharacteristics(LPCTSTR TaskName, LPDWORD TaskIndex)
    HANDLE __stdcall AvSetMmThreadCharacteristics(LPCTSTR FirstTask, LPCTSTR SecondTask, LPDWORD TaskIndex)
    BOOL __stdcall AvRevertMmThreadCharacteristics(HANDLE AvrtHandle)
    BOOL __stdcall AvQuerySystemResponsiveness(HANDLE AvrtHandle, PULONG SystemResponsivenessValue)

