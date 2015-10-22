from types cimport *

cdef extern from *:
    ctypedef enum AVRT_PRIORITY:
        AVRT_PRIORITY_VERYLOW       # -2
        AVRT_PRIORITY_LOW           # -1
        AVRT_PRIORITY_NORMAL        #  0
        AVRT_PRIORITY_HIGH          #  1
        AVRT_PRIORITY_CRITICAL      #  2

    BOOL __stdcall AvSetMmThreadPriority(HANDLE AvrtHandle, AVRT_PRIORITY Priority)
    HANDLE __stdcall AvSetMmThreadCharacteristics(LPCTSTR TaskName, LPDWORD TaskIndex)
    HANDLE __stdcall AvSetMmMaxThreadCharacteristics(LPCTSTR FirstTask, LPCTSTR SecondTask, LPDWORD TaskIndex)
    BOOL __stdcall AvRevertMmThreadCharacteristics(HANDLE AvrtHandle)
    BOOL __stdcall AvQuerySystemResponsiveness(HANDLE AvrtHandle, PULONG SystemResponsivenessValue)

# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:
