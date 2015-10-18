#include "Python.h"

int
_load_cython_windows_modules(PyObject *m)
{
    {
    PyObject *processthreads;
    extern int PyInit_processthreads();
    processthreads = PyInit_processthreads();
    if (!processthreads)
        return NULL;

    if (PyModule_AddObject(m, "processthreads", processthreads))
        return NULL;
    }

}

/* vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                  */
