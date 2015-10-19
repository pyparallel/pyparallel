# winnt.h

from . cimport types

cdef extern from "<windows.h>":

    LONGLONG Int32x32To64(LONG Multiplier, LONG Multiplicand)
    ULONGLONG UInt32x32To64(DWORD Multiplier, DWORD Multiplicand)

    DWORD __stdcall PopulationCount64(DWORD64 operand)

