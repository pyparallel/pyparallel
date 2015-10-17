# fileapi.h
from . cimport types

cdef extern from "<windows.h>":

    BOOL FileTimeToSystemTime(PFILETIME filetime, PSYSTEMTIME systime)
    void GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
    void GetSystemTimePreciseAsFileTime(LPFILETIME lpSystemTimeAsFileTime)

