# winnt.h

from types cimport *

cdef extern from *:

    LONGLONG Int32x32To64(LONG Multiplier, LONG Multiplicand)
    ULONGLONG UInt32x32To64(DWORD Multiplier, DWORD Multiplicand)

    DWORD __stdcall PopulationCount64(DWORD64 operand)

    ctypedef enum CACHE_LINE_LEVEL:
        PF_TEMPORAL_LEVEL_1
        PF_NON_TEMPORAL_LEVEL_ALL

    void PreFetchCacheLine(int Level, void const *Address)

    void MemoryBarrier()
    void YieldProcessor()

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
