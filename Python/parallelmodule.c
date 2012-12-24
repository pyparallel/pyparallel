
#include "Python.h"
#include <Windows.h>

#include "pyparallel_private.h"

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

/*
typedef struct _ParallelContext {
    HANDLE ev;
    Py_ssize_t argc;
    PyFunctionObject *f;
    PyObject **args;
    PyObject **results;
} ParallelContext;
*/

typedef struct _ParallelContext {
    HANDLE ev;
    Py_ssize_t argc;
    PyFunctionObject *f;
    PyObject **args;
    PyObject **results;
} PXCTX, ParallelContext;

typedef struct ParallelContext2 {
    PyObject *func;
    PyObject *args;
    PyObject **result;
} PXCTX2;

typedef struct ParallelContext3 {
    HANDLE    done;
    PyObject *func;
    PyObject *args;
    PyObject **result;
} PXCTX3;

typedef struct ParallelContext4 {
    HANDLE      done;
    Py_ssize_t  ix;
    PyObject   *func;
    PyObject   *arg;
    PyObject  **result;
} PXCTX4;


void
NTAPI
px_callback(PTP_CALLBACK_INSTANCE instance, void *context)
{
    ParallelContext *ctx = (ParallelContext *)context;
    Py_ssize_t i;
    PyTupleObject arg;
    PyObject_INIT_VAR(&arg, &PyTuple_Type, 1);

#ifdef Py_DEBUG
    assert(Py_MainThreadId != _Py_get_current_thread_id());
    assert(Py_ParallelContextsEnabled == 1);
#endif

    for (i = 0; i <= ctx->argc; i++) {
        arg.ob_item[0] = (PyObject *)ctx->args[i];
        ctx->results[i] = PyObject_CallObject((PyObject *)ctx->f,
                                              (PyObject *)&arg);
    }

    if (ctx->ev != NULL)
        SetEventWhenCallbackReturns(instance, ctx->ev);
}

static PyObject *
px_cb1(void *instance, void *context)
{
    PXCTX2 *c = (PXCTX2 *)context;
    PyObject *result;

    result = PyObject_CallObject(c->func, c->args);
    return result;
}

void
px_cb2(void *instance, void *context)
{
    PXCTX2 *c = (PXCTX2 *)context;
    PyObject *result;

    result = PyObject_CallObject(c->func, c->args);
    *(c->result) = result;
}

void
NTAPI
px_cb3(PTP_CALLBACK_INSTANCE instance, void *context)
{
    PXCTX3 *c = (PXCTX3 *)context;

    *(c->result) = PyObject_CallObject(c->func, c->args);

    if (c->done)
        SetEventWhenCallbackReturns(instance, c->done);
}

void
NTAPI
px_cb4(PTP_CALLBACK_INSTANCE instance, void *context)
{
    PyObject *args;
    PXCTX4 *c = (PXCTX4 *)context;

    _PyParallel_EnteredParallelContext(context);

    args = Py_BuildValue("(O)", c->arg);
    *(c->result) = PyObject_CallObject(c->func, args);

    if (c->done)
        SetEventWhenCallbackReturns(instance, c->done);

    _PyParallel_LeavingParallelContext();
}


PyDoc_STRVAR(map_doc,
"map(callable, iterable) -> list\n\
\n\
Calls ``callable`` with each item in ``iterable``.\n\
Returns a list of results.");

#define CACHE_LINE_SIZE 64

static
PyObject *
parallel_map(PyObject *self, PyObject *args)
{
    PyObject *func, *list = NULL;
    PyFunctionObject *f;
    PyListObject *l, *results;
    PyObject ***all_results;
    PyObject *o, *r, *o1, *o2;
    Py_ssize_t i, y, sz, actual_size, aligned_size, align;
    Py_ssize_t results_size, ctx_size;
    PXCTX c;
    ParallelContext **contexts;
    TP_WORK *work, *last;
    HANDLE last_event;
    DWORD err;

    if (!PyArg_ParseTuple(args, "OO:map", &func, &list))
        return NULL;

    if (!PyFunction_Check(func)) {
        PyErr_SetString(PyExc_TypeError, "argument 1 must be a function");
        return NULL;
    }

    f = (PyFunctionObject *)func;

    if (!PyList_Check(list)) {
        PyErr_SetString(PyExc_TypeError, "argument 2 must be a list");
        return NULL;
    }

    l = (PyListObject *)list;
    sz = PyList_GET_SIZE(l);
    o = (PyObject *)PyList_GET_ITEM(l, 0);
    o1 = (PyObject *)PyList_GET_ITEM(l, 1);
    o2 = (PyObject *)PyList_GET_ITEM(l, 2);

    results = (PyListObject *)PyList_New(1);

    last_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    /*ParallelContext *c = (ParallelContext *)malloc(sizeof(ParallelContext));*/
    c.ev = last_event;
    c.f = f;
    c.argc = 1;
    c.args = &o;
    c.results = &results->ob_item[0];

    _PyParallel_EnableParallelContexts();
    if (!TrySubmitThreadpoolCallback(px_callback, (void *)&c, NULL)) {
        PyErr_SetString(PyExc_RuntimeError, "TrySubmitThreadpoolCallback");
        return NULL;
    }

    err = WaitForSingleObject(last_event, 1000*1000);
    if (err == WAIT_FAILED) {
        PyErr_SetString(PyExc_RuntimeError, \
                        "WaitForSingleObject -> WAIT_FAILED");
        return NULL;
    } else if (err == WAIT_TIMEOUT) {
        PyErr_SetString(PyExc_RuntimeError, \
                        "WaitForSingleObject -> WAIT_TIMEOUT");
        return NULL;
    } else if (err != WAIT_OBJECT_0) {
        PyErr_SetString(PyExc_RuntimeError, \
                        "WaitForSingleObject != WAIT_OBJECT_0");
        return NULL;
    }

    _PyParallel_DisableParallelContexts();

    return (PyObject *)results;
}

static
PyObject *
parallel_t1(PyObject *self, PyObject *args)
{
    int i;
    PyObject *func, *result;
    PXCTX2 c;

    if (!PyArg_ParseTuple(args, "Oi:t1", &func, &i)) {
        PyErr_SetString(PyExc_TypeError, "invalid arguments");
        return NULL;
    }
    Py_INCREF(func);

    if (!PyCallable_Check(func)) {
        PyErr_SetString(PyExc_TypeError, "argument 1 must be a function");
        return NULL;
    }

    c.func = func;
    c.args = Py_BuildValue("(i)", i);
    Py_INCREF(c.args);
    result = px_cb1(NULL, &c);
    Py_XINCREF(result);
    return result;
}

static
PyObject *
parallel_t2(PyObject *self, PyObject *args)
{
    int i;
    PyObject *func, *result;
    PXCTX2 c;

    if (!PyArg_ParseTuple(args, "Oi:t1", &func, &i)) {
        PyErr_SetString(PyExc_TypeError, "invalid arguments");
        return NULL;
    }
    Py_INCREF(func);

    if (!PyCallable_Check(func)) {
        PyErr_SetString(PyExc_TypeError, "argument 1 must be a function");
        return NULL;
    }

    c.func = func;
    c.args = Py_BuildValue("(i)", i);
    c.result = &result;
    Py_INCREF(c.args);
    px_cb2(NULL, &c);
    Py_XINCREF(result);
    return result;
}

static
PyObject *
parallel_t3(PyObject *self, PyObject *args)
{
    int i;
    PyObject *func, *result;
    PXCTX3 c;
    DWORD err;

    if (!PyArg_ParseTuple(args, "Oi:t1", &func, &i)) {
        PyErr_SetString(PyExc_TypeError, "invalid arguments");
        return NULL;
    }
    Py_INCREF(func);

    if (!PyCallable_Check(func)) {
        PyErr_SetString(PyExc_TypeError, "argument 1 must be a function");
        return NULL;
    }

    c.done = CreateEvent(NULL, FALSE, FALSE, NULL);
    c.func = func;
    c.args = Py_BuildValue("(i)", i);
    c.result = &result;
    Py_INCREF(c.args);

    _PyParallel_EnableParallelContexts();
    if (!TrySubmitThreadpoolCallback(px_cb3, (void *)&c, NULL)) {
        PyErr_SetString(PyExc_RuntimeError, "TrySubmitThreadpoolCallback");
        return NULL;
    }

    err = WaitForSingleObject(c.done, 1000*1000);
    if (err == WAIT_FAILED) {
        PyErr_SetString(PyExc_RuntimeError, \
                        "WaitForSingleObject -> WAIT_FAILED");
        return NULL;
    } else if (err == WAIT_TIMEOUT) {
        PyErr_SetString(PyExc_RuntimeError, \
                        "WaitForSingleObject -> WAIT_TIMEOUT");
        return NULL;
    } else if (err != WAIT_OBJECT_0) {
        PyErr_SetString(PyExc_RuntimeError, \
                        "WaitForSingleObject != WAIT_OBJECT_0");
        return NULL;
    }
    _PyParallel_DisableParallelContexts();

    Py_XINCREF(result);
    return result;
}

static
PyObject *
parallel_t4(PyObject *self, PyObject *args)
{
    PyObject *func, *list, *result, *r;
    PXCTX4 **contexts;
    PyObject **results;
    PXCTX4 *ctx;
    Py_ssize_t i, size;
    DWORD err;
    HANDLE last_event;

    if (!PyArg_ParseTuple(args, "OO:t4", &func, &list)) {
        PyErr_SetString(PyExc_TypeError, "invalid arguments");
        return NULL;
    }

    if (!PyCallable_Check(func)) {
        PyErr_SetString(PyExc_TypeError, "argument 1 must be a function");
        return NULL;
    }

    if (!PyList_Check(list)) {
        PyErr_SetString(PyExc_TypeError, "argument 2 must be a list");
        return NULL;
    }

    Py_INCREF(func);
    Py_INCREF(list);

    size = PyList_GET_SIZE(list);

    contexts = (PXCTX4   **)malloc(sizeof(PXCTX4   *) * size);
    results  = (PyObject **)malloc(sizeof(PyObject *) * size);

    last_event = CreateEvent(NULL, FALSE, FALSE, NULL);

    _PyParallel_EnableParallelContexts();
    for (i = 0; i < size; i++) {
        ctx = contexts[i] = (PXCTX4 *)malloc(sizeof(PXCTX4));
        ctx->ix     = i;
        ctx->func   = func;
        ctx->result = &results[i];
        ctx->arg    = PyList_GET_ITEM(list, i);
        /* We can't use Py_INCREF here as px ctx has been activated. */
        ctx->arg->ob_refcnt++;

        if (i == size-1)
            ctx->done = last_event;
        else
            ctx->done = NULL;

        if (!TrySubmitThreadpoolCallback(px_cb4, (void *)ctx, NULL)) {
            PyErr_SetString(PyExc_RuntimeError, "TrySubmitThreadpoolCallback");
            return NULL;
        }
    }

    err = WaitForSingleObject(last_event, 1000*1000);
    _PyParallel_DisableParallelContexts();

    if (err == WAIT_FAILED) {
        PyErr_SetString(PyExc_RuntimeError, \
                        "WaitForSingleObject -> WAIT_FAILED");
        return NULL;
    } else if (err == WAIT_TIMEOUT) {
        PyErr_SetString(PyExc_RuntimeError, \
                        "WaitForSingleObject -> WAIT_TIMEOUT");
        return NULL;
    } else if (err != WAIT_OBJECT_0) {
        PyErr_SetString(PyExc_RuntimeError, \
                        "WaitForSingleObject != WAIT_OBJECT_0");
        return NULL;
    }

    Py_DECREF(func);
    Py_DECREF(list);

    result = PyList_New(size);
    for (i = 0; i < size; i++) {
        r = results[i];
        Py_XINCREF(r);
        PyList_SET_ITEM(result, i, r);
        ctx = contexts[i];
        Py_DECREF(ctx->arg);
        free((void *)ctx);
    }
    Py_INCREF(result);
    return result;
}


static PyMethodDef parallel_methods[] = {
    { "run", (PyCFunction)parallel_run, METH_NOARGS, run_doc },
    { "run_once", (PyCFunction)parallel_run_once, METH_NOARGS, run_once_doc },
    { "map", (PyCFunction)parallel_map, METH_VARARGS, map_doc },
    { "t1", (PyCFunction)parallel_t1, METH_VARARGS, map_doc },
    { "t2", (PyCFunction)parallel_t2, METH_VARARGS, map_doc },
    { "t3", (PyCFunction)parallel_t3, METH_VARARGS, map_doc },
    { "t4", (PyCFunction)parallel_t4, METH_VARARGS, map_doc },

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

static
PyObject *
_disabled_parallel_map(PyObject *self, PyObject *args)
{
    PyObject *func, *list = NULL;
    PyFunctionObject *f;
    PyListObject *l, *results;
    PyObject ***all_results;
    Py_ssize_t i, y, sz, actual_size, aligned_size, align;
    Py_ssize_t results_size, ctx_size;
    ParallelContext **contexts;
    TP_WORK *work, *last;
    HANDLE last_event;
    DWORD err;

    if (!PyArg_ParseTuple(args, "OO:map", &func, &list))
        return NULL;

    if (!PyFunction_Check(func)) {
        PyErr_SetString(PyExc_TypeError, "argument 1 must be a function");
        return NULL;
    }

    f = (PyFunctionObject *)func;

    if (!PyList_Check(list)) {
        PyErr_SetString(PyExc_TypeError, "argument 2 must be a list");
        return NULL;
    }

    l = (PyListObject *)list;

    align = sizeof(void *)-1;
    actual_size = PyList_GET_SIZE(l);
    aligned_size = (actual_size+align) & ~align;

    results_size = sizeof(void *) * aligned_size;
    ctx_size = sizeof(ParallelContext) * actual_size;

    all_results = (PyObject ***)_aligned_malloc(results_size, CACHE_LINE_SIZE);
    contexts = (ParallelContext *)_aligned_malloc(ctx_size, CACHE_LINE_SIZE);

    for (i = 0; i <= actual_size; i+=aligned_size) {
        ParallelContext *c = contexts[0] = \
            (ParallelContext *)malloc(sizeof(ParallelContext));
        c->f = f;
        if (i + aligned_size < actual_size) {
            c->argc = actual_size;
            last_event = CreateEvent(NULL, FALSE, FALSE, NULL);
            c->ev = &last_event;
        } else {
            c->argc = aligned_size;
            c->ev = NULL;
        }

        c->args = (PyObject **)l->ob_item[i];
        c->results = (PyObject **)all_results[(i+align) & ~align];

        if (!TrySubmitThreadpoolCallback(px_callback, (void *)c, NULL)) {
            free(all_results);
            free(contexts);
            PyErr_SetString(PyExc_RuntimeError, "TrySubmitThreadpoolCallback");
            return NULL;
        }
    }

    err = WaitForSingleObject(last_event, INFINITE);
    if (err == WAIT_FAILED) {
        free(all_results);
        free(contexts);
        PyErr_SetString(PyExc_RuntimeError, "WaitForSingleObject");
        return NULL;
    }

#ifdef Py_DEBUG
    assert(err == WAIT_OBJECT_0);
#endif

    results = (PyListObject *)PyList_New(actual_size);
    results->ob_item = (PyObject **)all_results;

    return (PyObject *)results;
}

/* vim:set ts=8 sw=4 sts=4 tw=78 et: */
