from types cimport *

cdef extern from *:
    # Bitmaps:
    void RtlInitializeBitMap(
        PRTL_BITMAP BitMapHeader,
        PULONG BitMapBuffer,
        ULONG SizeOfBitmap
    )

    BOOLEAN RtlAreBitsClear(
        PRTL_BITMAP BitMapHeader,
        ULONG StartingIndex,
        ULONG Length
    )

    BOOLEAN RtlAreBitsSet(
        PRTL_BITMAP BitMapHeader,
        ULONG StartingIndex,
        ULONG Length
    )

    BOOLEAN RtlCheckBit(
        PRTL_BITMAP BitMapHeader,
        ULONG BitPosition
    )

    BOOLEAN RtlClearAllBits(PRTL_BITMAP BitMapHeader)

    BOOLEAN RtlClearBit(
        PRTL_BITMAP BitMapHeader,
        ULONG BitNumber
    )

    BOOLEAN RtlClearBits(
        PRTL_BITMAP BitMapHeader,
        ULONG StartingIndex,
        ULONG NumberToClear
    )

    ULONG RtlFindClearBits(
        PRTL_BITMAP BitMapHeader,
        ULONG NumberToFind,
        ULONG HintIndex
    )

    ULONG RtlFindClearBitsAndSet(
        PRTL_BITMAP BitMapHeader,
        ULONG NumberToFind,
        ULONG HintIndex
    )

    ULONG RtlFindClearRuns(
        PRTL_BITMAP BitMapHeader,
        PRTL_BITMAP_RUN RunArray,
        ULONG SizeOfRunArray,
        BOOLEAN LocateLongestRuns
    )

    ULONG RtlFindFirstRunClear(
        PRTL_BITMAP BitMapHeader,
        PULONG StartingIndex
    )

    ULONG RtlFindLastBackwardRunClear(
        PRTL_BITMAP BitMapHeader,
        ULONG FromIndex,
        PULONG StartingRunIndex
    )

    ULONG RtlFindLongestRunClear(
        PRTL_BITMAP BitMapHeader,
        PULONG StartingIndex
    )

    ULONG RtlFindNextForwardRunClear(
        PRTL_BITMAP BitMapHeader,
        ULONG FromIndex,
        PULONG StartingRunIndex
    )

    ULONG RtlFindSetBits(
        PRTL_BITMAP BitMapHeader,
        ULONG NumberToFind,
        ULONG HintIndex
    )

    ULONG RtlFindSetBitsAndClear(
        PRTL_BITMAP BitMapHeader,
        ULONG NumberToFind,
        ULONG HintIndex
    )

    ULONG RtlNumberOfClearBits(PRTL_BITMAP BitMapHeader)

    ULONG RtlNumberOfSetBits(PRTL_BITMAP BitMapHeader)

    void RtlSetAllBits(PRTL_BITMAP BitMapHeader)
    void RtlSetBits(
        PRTL_BITMAP BitMapHeader,
        ULONG StartingIndex,
        ULONG NumberToSet
    )

    void RtlSetBit(
        PRTL_BITMAP BitMapHeader,
        ULONG BitNumber
    )

    CCHAR RtlFindMostSignificantBit(ULONGLONG Set)
    CCHAR RtlFindLeastSignificantBit(ULONGLONG Set)

    ULONG RtlNumberOfSetBitsUlongPtr(ULONG_PTR Target)

    BOOLEAN RtlTestBit(
        PRTL_BITMAP BitMapHeader,
        ULONG BitNumber
    )
    # End of bitmaps

    void RtlPrefetchMemoryNonTemporal(PVOID Source, Py_ssize_t Length)

    void RtlCopyMemory(PVOID dst, PVOID src, Py_ssize_t Length)


cdef class Bitmap:
    cdef:
        RTL_BITMAP bitmap
        public ULONG allocated_size

    cpdef ULONG num_clear_bits(self)
    cpdef ULONG num_set_bits(self)
    cpdef BOOLEAN check_bit(self, ULONG bit_position)
    cpdef BOOLEAN test_bit(self, ULONG bit_number)
    cpdef BOOLEAN are_bits_clear(self, ULONG starting_index, ULONG length)
    cpdef BOOLEAN are_bits_set(self, ULONG starting_index, ULONG length)
    cpdef void clear_all_bits(self)
    cpdef void clear_bits(self, ULONG starting_index, ULONG number_to_clear)
    cpdef void clear_bit(self, ULONG bit_number)
    cpdef ULONG find_clear_bits(self, ULONG number_to_find, ULONG hint_index)

    cpdef ULONG find_clear_bits_and_set(
        self,
        ULONG number_to_find,
        ULONG hint_index
    )

    cpdef void set_all_bits(self)

    cpdef void set_bits(self, ULONG starting_index, ULONG number_to_set)

    cpdef void set_bit(self, ULONG bit_number)

    cpdef ULONG find_set_bits(self, ULONG number_to_find, ULONG hint_index)

    cpdef ULONG find_set_bits_and_clear(
        self,
        ULONG number_to_find,
        ULONG hint_index
    )

    cdef ULONG _find_first_run_clear(self, PULONG starting_index)

    cdef ULONG _find_last_backward_run_clear(
        self,
        ULONG from_index,
        PULONG starting_run_index
    )

    cdef ULONG _find_longest_run_clear(self, PULONG starting_index)

    cdef ULONG _find_next_forward_run_clear(
        self,
        ULONG from_index,
        PULONG starting_index
    )

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
