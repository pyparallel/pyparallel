from cpython import *
from types import *

cdef extern from "../../Python/pyparallel_private.h":
    ctypedef struct PxContext:
        pass

    void *PxContext_Malloc(PxContext *c, Py_ssize_t size, Py_ssize_t alignment, int no_realloc)
