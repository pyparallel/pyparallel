from types cimport *

cdef extern from *:
    # Bitmaps:
    void RtlInitializeBitMap(PRTL_BITMAP BitMapHeader, PULONG BitMapBuffer, ULONG SizeOfBitmap)

    BOOLEAN RtlAreBitsClear(PRTL_BITMAP BitMapHeader, ULONG StartingIndex, ULONG Length)
    BOOLEAN RtlAreBitsSet(PRTL_BITMAP BitMapHeader, ULONG StartingIndex, ULONG Length)
    BOOLEAN RtlCheckBit(PRTL_BITMAP BitMapHeader, ULONG BitPosition)
    BOOLEAN RtlClearAllBits(PRTL_BITMAP BitMapHeader)
    BOOLEAN RtlClearBit(PRTL_BITMAP BitMapHeader, ULONG BitNumber)
    BOOLEAN RtlClearBits(PRTL_BITMAP BitMapHeader, ULONG StartingIndex, ULONG NumberToClear)

    ULONG RtlFindClearBits(PRTL_BITMAP BitMapHeader, ULONG NumberToFind, ULONG HintIndex)
    ULONG RtlFindClearBitsAndSet(PRTL_BITMAP BitMapHeader, ULONG NumberToFind, ULONG HintIndex)

    ULONG RtlFindClearRuns(PRTL_BITMAP BitMapHeader, PRTL_BITMAP_RUN RunArray, ULONG SizeOfRunArray, BOOLEAN LocateLongestRuns)
    ULONG RtlFindFirstRunClear(PRTL_BITMAP BitMapHeader, PULONG StartingIndex)
    ULONG RtlFindLastBackwardRunClear(PRTL_BITMAP BitMapHeader, ULONG FromIndex, PULONG StartingRunIndex)

    ULONG RtlFindLongestRunClear(PRTL_BITMAP BitMapHeader, PULONG StartingIndex)
    ULONG RtlFindNextForwardRunClear(PRTL_BITMAP BitMapHeader, ULONG FromIndex, PULONG StartingRunIndex)
    ULONG RtlFindSetBits(PRTL_BITMAP BitMapHeader, ULONG NumberToFind, ULONG HintIndex)
    ULONG RtlFindSetBitsAndClear(PRTL_BITMAP BitMapHeader, ULONG NumberToFind, ULONG HintIndex)

    ULONG RtlNumberOfClearBits(PRTL_BITMAP BitMapHeader)
    ULONG RtlNumberOfSetBits(PRTL_BITMAP BitMapHeader)

    void RtlSetAllBits(PRTL_BITMAP BitMapHeader)
    void RtlSetBits(PRTL_BITMAP BitMapHeader, ULONG StartingIndex, ULONG NumberToSet)
    void RtlSetBit(PRTL_BITMAP BitMapHeader, ULONG BitNumber)

    CCHAR RtlFindMostSignificantBit(ULONGLONG Set)
    CCHAR RtlFindLeastSignificantBit(ULONGLONG Set)

    ULONG RtlNumberOfSetBitsUlongPtr(ULONG_PTR Target)

    BOOLEAN RtlTestBit(PRTL_BITMAP BitMapHeader, ULONG BitNumber)
    # End of bitmaps

    void RtlPrefetchMemoryNonTemporal(PVOID Source, Py_ssize_t Length)
    void RtlCopyMemory(PVOID dst, PVOID src, Py_ssize_t Length)

# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:
