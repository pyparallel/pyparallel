#ifndef FILEIO_H
#define FILEIO_H

#define PyAsync_CACHING_DEFAULT         (0)
#define PyAsync_CACHING_BUFFERED        (1)
#define PyAsync_CACHING_RANDOMACCESS    (2)
#define PyAsync_CACHING_SEQUENTIALSCAN  (3)
#define PyAsync_CACHING_WRITETHROUGH    (4)
#define PyAsync_CACHING_TEMPORARY       (5)

#ifdef MS_WINDOWS
#ifndef HANDLE
typedef void *HANDLE;
#endif
#endif

typedef struct _fileio {
    PyObject_HEAD
    int fd;
    unsigned int created : 1;
    unsigned int readable : 1;
    unsigned int writable : 1;
    signed int seekable : 2; /* -1 means unknown */
    unsigned int closefd : 1;
    unsigned int deallocating: 1;
    PyObject *weakreflist;
    PyObject *dict;
    PyObject *closer;
#ifdef WITH_PARALLEL
#ifdef MS_WINDOWS
    HANDLE      h;
#endif
    Py_UNICODE *name;
    Py_ssize_t  size;
    int         caching;
    int         native;
    int         istty;
    PyObject   *callback;
    PyObject   *errback;
    PyObject   *owner;
    LARGE_INTEGER read_offset;
    LARGE_INTEGER write_offset;
#endif
} fileio;

extern PyTypeObject PyFileIO_Type;

#define PyFileIO_Check(op) (PyObject_TypeCheck((op), &PyFileIO_Type))


#endif /* FILEIO_H */
/* vim:set ts=8 sw=4 sts=4 tw=78 et: */
