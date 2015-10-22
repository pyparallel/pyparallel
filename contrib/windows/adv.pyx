
include "adv.pxi"

from types cimport *
from base cimport *
from adv cimport *

cdef class Operation:
    cdef:
        OPERATION_PARAMS params

    def __cinit__(self, operation_id, start_flags, end_flags):
        self.params.start.Version = OPERATION_API_VERSION
        self.params.start.OperationId = operation_id
        self.params.start.Flags = start_flags

        self.params.end.Version = OPERATION_API_VERSION
        self.params.end.OperationId = operation_id
        self.params.end.Flags = end_flags

    cdef BOOL _start(self):
        return OperationStart(&self.params.start)

    cdef BOOL _end(self):
        return OperationEnd(&self.params.end)

    def start(self):
        if not self._start():
            raise OSError("OperationStart")

    def end(self):
        if not self._end():
            raise OSError("OperationEnd")

# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:
