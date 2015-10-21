#include "Python.h"

#define _LOAD(n) do {                                                          \
    PyObject *mod;                                                             \
    extern PyObject *PyInit_##n();                                             \
    mod = PyInit_##n();                                                        \
    if (!mod)                                                                  \
        return 0;                                                              \
    if (PyModule_AddObject(m, #n, mod))                                        \
        return 0;                                                              \
} while (0)

int
_load_cython_windows_modules(PyObject *m)
{
    _LOAD(processthreads);
    _LOAD(rtl);
    _LOAD(user);
    return 1;
}

/* vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                  */
