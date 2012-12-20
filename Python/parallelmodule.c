
#include "Python.h"

PyDoc_STRVAR(parallel_doc,
"Parallel module.\n\
\n\
Functions:\n\
\n\
run() -- runs the parallel even loop forever.\n\
run_once() -- runs one iteration of the parallel event loop.\n");

PyDoc_STRVAR(run_doc,
"run() -> None\n\
\n\
Runs the parallel event loop forever.");

static
PyObject *
parallel_run(PyObject *self, PyObject *args)
{
    Py_RETURN_NONE;
}

PyDoc_STRVAR(run_once_doc,
"run_once() -> None\n\
\n\
Runs the parallel event loop once.");

static
PyObject *
parallel_run_once(PyObject *self, PyObject *args)
{
    Py_RETURN_NONE;
}

static PyMethodDef parallel_methods[] = {
    { "run", (PyCFunction)parallel_run, METH_NOARGS, run_doc },
    { "run_once", (PyCFunction)parallel_run_once, METH_NOARGS, run_once_doc },

    { NULL, NULL } /* sentinel */
};

static struct PyModuleDef parallelmodule = {
    PyModuleDef_HEAD_INIT,
    "parallel",
    parallel_doc,
    -1, /* multiple "initialization" just copies the module dict. */
    parallel_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyObject *
_PyParallel_ModInit(void)
{
    PyObject *m, *d;

    m = PyModule_Create(&parallelmodule);
    if (m == NULL)
        return NULL;

    return m;
}

/* vim:set ts=8 sw=4 sts=4 tw=78 et: */
