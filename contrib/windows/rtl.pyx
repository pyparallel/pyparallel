from cpython.mem cimport PyMem_Malloc, PyMem_Free

include "wdm.pxi"
from rtl cimport *

cdef class Bitmap:
    def __cinit__(self, ULONG number_of_bits):
        cdef:
            PULONG buf
            ULONG num_bytes
            ULONG allocated_size

        num_bytes = number_of_bits >> 3
        allocated_size = (num_bytes + sizeof(ULONG)) & ~(sizeof(ULONG)-1)
        buf = <PULONG>PyMem_Malloc(allocated_size)
        if not buf:
            raise MemoryError()

        self.allocated_size = allocated_size
        RtlInitializeBitMap(&self.bitmap, buf, number_of_bits)
        RtlClearAllBits(&self.bitmap)

    def __dealloc__(self):
        PyMem_Free(self.bitmap.Buffer)

    cpdef ULONG num_clear_bits(self):
        return RtlNumberOfClearBits(&self.bitmap)

    cpdef ULONG num_set_bits(self):
        return RtlNumberOfSetBits(&self.bitmap)

    cpdef BOOLEAN check_bit(self, ULONG bit_position):
        return RtlCheckBit(&self.bitmap, bit_position)

    cpdef BOOLEAN test_bit(self, ULONG bit_number):
        return RtlTestBit(&self.bitmap, bit_number)

    cpdef BOOLEAN are_bits_clear(self, ULONG starting_index, ULONG length):
        return RtlAreBitsClear(&self.bitmap, starting_index, length)

    cpdef BOOLEAN are_bits_set(self, ULONG starting_index, ULONG length):
        return RtlAreBitsSet(&self.bitmap, starting_index, length)

    cpdef void clear_all_bits(self):
        RtlClearAllBits(&self.bitmap)

    cpdef void clear_bits(self, ULONG starting_index, ULONG number_to_clear):
        RtlClearBits(&self.bitmap, starting_index, number_to_clear)

    cpdef void clear_bit(self, ULONG bit_number):
        RtlClearBit(&self.bitmap, bit_number)

    cpdef ULONG find_clear_bits(self, ULONG number_to_find, ULONG hint_index):
        return RtlFindClearBits(&self.bitmap, number_to_find, hint_index)

    cpdef ULONG find_clear_bits_and_set(self, ULONG number_to_find,
                                              ULONG hint_index):
        return RtlFindClearBitsAndSet(&self.bitmap, number_to_find, hint_index)

    cpdef void set_all_bits(self):
        RtlSetAllBits(&self.bitmap)

    cpdef void set_bits(self, ULONG starting_index, ULONG number_to_set):
        RtlSetBits(&self.bitmap, starting_index, number_to_set)

    cpdef void set_bit(self, ULONG bit_number):
        RtlSetBit(&self.bitmap, bit_number)

    cpdef ULONG find_set_bits(self, ULONG number_to_find, ULONG hint_index):
        return RtlFindSetBits(&self.bitmap, number_to_find, hint_index)

    cpdef ULONG find_set_bits_and_clear(self, ULONG number_to_find,
                                              ULONG hint_index):
        return RtlFindSetBitsAndClear(&self.bitmap, number_to_find, hint_index)

    cdef ULONG _find_first_run_clear(self, PULONG starting_index):
        return RtlFindFirstRunClear(&self.bitmap, starting_index)

    cdef ULONG _find_last_backward_run_clear(self, ULONG from_index,
                                                   PULONG starting_run_index):
        return RtlFindLastBackwardRunClear(&self.bitmap,
                                           from_index,
                                           starting_run_index)

    cdef ULONG _find_longest_run_clear(self, PULONG starting_index):
        return RtlFindLongestRunClear(&self.bitmap, starting_index)

    cdef ULONG _find_next_forward_run_clear(self, ULONG from_index,
                                                  PULONG starting_index):
        return RtlFindNextForwardRunClear(&self.bitmap,
                                          from_index,
                                          starting_index)

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
