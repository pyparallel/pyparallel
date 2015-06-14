from libc cimport stdio

cdef extern from "stdio.h" nogil:
    stdio.FILE *fdopen(int fd, char *mode)
