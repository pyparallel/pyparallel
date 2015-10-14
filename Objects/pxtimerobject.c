/* PxTimer object implementation.
 * Copyright 2015, Trent Nelson <trent@trent.me>
 */

#include "Python.h"
#include "pxtimerobject.h"

static PyTypeObject PxTimer_Type;

Py_TLS PxTimerObject *active_timer = NULL;

PxTimerObject *
PxTimer_GetActive(void)
{
    return active_timer;
}

void
_PxTimer_SetActive(PxTimerObject *t)
{
    assert(t);
    active_timer = t;
}

void
_PxTimer_ClearActive(void)
{
    assert(active_timer);
    active_timer = NULL;
}

void
CALLBACK
PxTimer_ShutdownWorkCallback(
    _Inout_ PTP_CALLBACK_INSTANCE instance,
    _Inout_opt_ PVOID context,
    _Inout_ PTP_WORK work
)
{
    PxTimerObject *t = (PxTimerObject *)context;
    PxContext *c = t->ctx;
    PxState *px = c->px;

    WaitForThreadpoolTimerCallbacks(t->ptp_timer, TRUE /* cancel callbacks */);

    EnterCriticalSection(&t->cs);

    Px_CLOSE_THREADPOOL_TIMER(t->ptp_timer);

    Py_REFCNT(t) -= 1;

    if (Py_REFCNT(t) < 0)
        __debugbreak();

    ((PyObject *)t)->is_px = _Py_NOT_PARALLEL;
    Py_PXFLAGS(t) &= ~Py_PXFLAGS_ISPX;
    Py_PXFLAGS(t) |=  (Py_PXFLAGS_ISPY | Py_PXFLAGS_WASPX);

    PxTimer_SET_SHUTDOWN(t);

    LeaveCriticalSection(&t->cs);

    PxContext_CallbackComplete(c);
}

void
CALLBACK
PxTimer_Callback(
    _Inout_ PTP_CALLBACK_INSTANCE instance,
    _Inout_opt_ PVOID context,
    _Inout_ PTP_TIMER timer
)
{
    PxTimerObject *t = (PxTimerObject *)context;
    PxContext *c = t->ctx;
    PxState *px = c->px;
    PyObject *result;
    Heap *prev_snapshot, *this_snapshot;
    LARGE_INTEGER start, end, elapsed, frequency;
    int expect_snapshot = 1;
    int expected_snapshot_group = 0;

    _PyParallel_EnteredCallback(c, instance);

    if (PxTimer_STOP_REQUESTED(t)) {
        EnterCriticalSection(&t->cs);
        PxTimer_SET_STOPPED(t);
        PxTimer_UNSET_STOP_REQUESTED(t);
        goto leave;
    }

    if (!TryEnterCriticalSection(&t->cs)) {
        InterlockedIncrement(&t->cs_contention);
        return;
    }

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    /* 'STARTED' is set if a timer has ever run.  'RUNNING' is set when it's
     * actually running, and then unset at the end.  And _PxTimer_SetActive()
     * is used to set the TLS 'active_timer' which allows the timer to quickly
     * differentiate between calls originating from itself (e.g. calling stop()
     * or setting an attribute) versus external calls (from other threads). */
    PxTimer_UNSET_START_REQUESTED(t);
    PxTimer_SET_STARTED(t);
    PxTimer_SET_RUNNING(t);
    _PxTimer_SetActive(t);

    switch (c->h->group) {
        case 1:
            /* Context's active heap belongs to the context. */
            prev_snapshot = &t->snapshot;
            this_snapshot = &c->snapshot;
            break;
        case 2:
            /* Context's active heap belongs to the timer. */
            prev_snapshot = &c->snapshot;
            this_snapshot = &t->snapshot;
            break;
        default:
            __debugbreak();
    }

    /* Make sure the context's snapshot pointer (c->s) is empty, and that the
     * underlying snapshot struct is also empty (this_snapshot->base). */
    if (c->s)
        __debugbreak();

    if (this_snapshot->base)
        __debugbreak();

    _PxContext_HeapSnapshot(c, this_snapshot);

    result = PyEval_CallObjectWithKeywords(c->func, c->args, c->kwds);

    if (!result)
        PxTimer_EXCEPTION();

    AcquireSRWLockExclusive(&t->data_srwlock);
    t->data = result;
    ReleaseSRWLockExclusive(&t->data_srwlock);

    if (++t->count < 0) {
        t->times_wrapped++;
        t->count = 1;
    } else if (t->count == 1)
        expect_snapshot = 0;

    /* We now switch to our alternate heap and roll back any snapshot if
     * applicable. */
    switch (c->h->group) {
        case 1:
            /* Context's active heap belongs to the context.  Switch to the
             * timer heap. */
            t->last_ctx_heap = c->h;
            c->h = t->last_timer_heap;
            expected_snapshot_group = 2;
            break;
        case 2:
            /* Context's active heap belongs to the timer.  Switch to the
             * context's heap. */
            t->last_timer_heap = c->h;
            c->h = t->last_ctx_heap;
            expected_snapshot_group = 1;
            break;
        default:
            __debugbreak();
    }

    if (expect_snapshot) {
        if (!prev_snapshot->base)
            __debugbreak();
        if (prev_snapshot->group != expected_snapshot_group)
            __debugbreak();

        _PxContext_Rewind(c, prev_snapshot);

    } else {
        if (prev_snapshot->base)
            __debugbreak();
    }

end:
    QueryPerformanceCounter(&end);
    elapsed.QuadPart = end.QuadPart - start.QuadPart;
    elapsed.QuadPart *= 1000000;
    elapsed.QuadPart /= frequency.QuadPart;
    AcquireSRWLockExclusive(&t->duration_srwlock);
    t->duration.QuadPart = elapsed.QuadPart;
    ReleaseSRWLockExclusive(&t->duration_srwlock);

    PxTimer_UNSET_RUNNING(t);
    _PxTimer_ClearActive();
leave:
    LeaveCriticalSection(&t->cs);
}


BOOL
CALLBACK
PxTimer_StartOnceCallback(
    PINIT_ONCE init_once,
    PVOID param,
    PVOID *context
)
{
    PxTimerObject *t = (PxTimerObject *)param;

    assert(PxTimer_Valid(t));

    PxTimer_SET_START_REQUESTED(t);
    InitOnceInitialize(&t->stop_once);
    SetThreadpoolTimer(t->ptp_timer,
                       &t->duetime,
                       t->period,
                       t->window_length);

    return TRUE;
}

BOOL
CALLBACK
PxTimer_StopOnceCallback(
    PINIT_ONCE init_once,
    PVOID param,
    PVOID *context
)
{
    PxTimerObject *t = (PxTimerObject *)param;

    /* Toggle the stop requested flag and set another timer with NULL
     * parameters.  This will synchronously trigger another timer callback,
     * which tests for the flag and registers the stop, which we use to
     * indicate that the timer is not and will not run again. */
    PxTimer_SET_STOP_REQUESTED(t);
    InitOnceInitialize(&t->start_once);
    SetThreadpoolTimer(t->ptp_timer, NULL, 0, 0);

    return TRUE;
}

BOOL
CALLBACK
PxTimer_ShutdownOnceCallback(
    PINIT_ONCE init_once,
    PVOID param,
    PVOID *context
)
{
    PxTimerObject *t = (PxTimerObject *)param;

    PxTimer_SET_SHUTDOWN_REQUESTED(t);
    pxtimer_stop((PyObject *)t);
    SubmitThreadpoolWork(t->shutdown_work);

    return TRUE;
}

int
PxTimer_Valid(PxTimerObject *t)
{
    return PxTimer_VALID(t);
}

int
PxTimer_IsSet(PxTimerObject *t)
{
    return IsThreadpoolTimerSet(t->ptp_timer);
}


/* This method is responsible for allocating and initializing all the internal
 * parts of the timer, e.g. the context, threadpool timer object, etc.  Upon
 * successful creation of the timer object, it is responsible for adding to
 * the active context's timer list, or if there is no active context, the
 * PxState's context list.
 *
 * The pxtimer_init() method is responsible for translating the Python
 * construction to the relevant duetime/period/window parts. */
PyObject *
pxtimer_alloc(PyTypeObject *type, Py_ssize_t nitems)
{
    Context *c, *x = PxContext_GetActive();
    PxState *px;
    PxTimerObject *t;
    PyObject *result = NULL;

    if (nitems != 0)
        __debugbreak();

    if (type != &PxTimer_Type)
        __debugbreak();

    c = PxContext_New(0);
    if (!c)
        return NULL;

    px = c->px;

    if (c->tp_ctx) {
        if (x->tp_ctx != x)
            __debugbreak();
        if (c->ptp_cbe != x->ptp_cbe)
            __debugbreak();
        if (!x->io_obj)
            __debugbreak();
        c->parent = x->io_obj;
        /* xxx todo: refactor Context/PxSocket such that contexts manage the
         * children, not the sockets. */
        __debugbreak();
    } else {
        /* Timer was created from the main thread, so associate with the main
         * PxState threadpool groups. */
        c->ptp_cbe = px->ptp_cbe;
    }

    t = (PxTimerObject *)PxContext_Malloc(c, sizeof(PxTimerObject), 0, 0);
    if (!t)
        PxTimer_FATAL();

    c->io_obj = (PyObject *)t;
    c->io_type = Px_IOTYPE_TIMER;

    if (!PxContext_InitObject(c, c->io_obj, type, 0))
        PxTimer_FATAL();

    t->last_ctx_heap = c->h;
    _PxContext_InitAdditionalHeap(c, &t->heap, 0);

    if (t->last_ctx_heap == c->h)
        /* Should be altered by call above. */
        __debugbreak();

    t->last_timer_heap = c->h;

    if (t->last_timer_heap->group == t->last_ctx_heap->group)
        __debugbreak();

    /* Used in concert with stop() and pxtimer_dealloc(). */
    if (!Py_PXCTX())
        Py_REFCNT(t) = 2;

    InitializeSRWLock(&t->data_srwlock);

    t->ctx = c;

    InitializeCriticalSectionAndSpinCount(&t->cs, 4000);

    t->shutdown_work = CreateThreadpoolWork(PxTimer_ShutdownWorkCallback,
                                            t,
                                            c->ptp_cbe);

    if (!t->shutdown_work)
        PxTimer_SYSERROR("CreateThreadpoolWork(PxTimer_ShutdownWorkCallback)");

    t->ptp_timer = CreateThreadpoolTimer(PxTimer_Callback, t, c->ptp_cbe);
    if (!t->ptp_timer)
        PxTimer_SYSERROR("CreateThreadpoolTimer(PxTimer_Callback)");

    InitOnceInitialize(&t->start_once);
    InitOnceInitialize(&t->stop_once);
    InitOnceInitialize(&t->shutdown_once);

    t->data = Py_None;

    _PxState_RegisterTimer(t);
    _PxState_RegisterContext(c);

    result = (PyObject *)t;
end:
    return result;
}

PyObject *
pxtimer_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PxTimerObject *t;

    t = (PxTimerObject *)type->tp_alloc(type, 0);
    if (!t)
        return NULL;

    /* Do we need to do anything else here?  Seems like we could let the
     * generic PyType_GenericNew() handle all of this functionality. */
    return (PyObject *)t;
}

void
pxtimer_dealloc(PxTimerObject *t)
{
    PxContext *c = t->ctx;
    PxState *px = c->px;

    _PxState_UnregisterTimer(t);
    _PxState_UnregisterContext(c);

    DeleteCriticalSection(&t->cs);

    Px_CLOSE_THREADPOOL_TIMER(t->ptp_timer);
    Px_CLOSE_THREADPOOL_WORK(t->shutdown_work);
    Px_HEAP_DESTROY(c->heap_handle);
    Px_FREE(c);

    InterlockedDecrement(&px->active);
    InterlockedDecrement(&px->contexts_active);
}

PyDoc_STRVAR(
    pxtimer_start_doc,
    "starts the timer\n\n"
);

PyObject *
pxtimer_start(PyObject *self)
{
    PxTimerObject *t = (PxTimerObject *)self;

    if (!PxTimer_Valid(t)) {
        PyErr_SetString(PyExc_ValueError, "invalid timer object");
        return NULL;
    }

    InitOnceExecuteOnce(&t->start_once, PxTimer_StartOnceCallback, t, NULL);

    Py_RETURN_NONE;
}

PyDoc_STRVAR(
    pxtimer_stop_doc,
    "stops the timer if it was running\n\n"
);

PyObject *
pxtimer_stop(PyObject *self)
{
    PxTimerObject *t = (PxTimerObject *)self;

    InitOnceExecuteOnce(&t->stop_once, PxTimer_StopOnceCallback, t, NULL);

    Py_RETURN_NONE;
}

PyObject *
pxtimer_shutdown(PyObject *self)
{
    PxTimerObject *t = (PxTimerObject *)self;

    InitOnceExecuteOnce(&t->shutdown_once,
                        PxTimer_ShutdownOnceCallback,
                        t,
                        NULL);

    Py_RETURN_NONE;
}

PyObject *
pxtimer_get_data(PxTimerObject *t, void *closure)
{
    PyObject *data;

    AcquireSRWLockShared(&t->data_srwlock);
    if (!t->data || t->data == Py_None)
        data = Py_None;
    else if (Py_ISPY(t->data))
        data = t->data;
    else
        data = PyObject_Copy(t->data);
    ReleaseSRWLockShared(&t->data_srwlock);

    Py_INCREF(data);
    return data;
}

PyObject *
pxtimer_get_duration(PxTimerObject *t, void *closure)
{
    LARGE_INTEGER d;

    AcquireSRWLockShared(&t->duration_srwlock);
    d.QuadPart = t->duration.QuadPart;
    ReleaseSRWLockShared(&t->duration_srwlock);

    return PyLong_FromLongLong(d.QuadPart);
}

PyObject *
pxtimer_get_critical_section_contention(PxTimerObject *t, void *closure)
{
    return PyLong_FromLong(t->cs_contention);
}

PyObject *
pxtimer_get_duetime(PxTimerObject *t, void *closure)
{
    LARGE_INTEGER i;
    i.LowPart = t->duetime.dwLowDateTime;
    i.HighPart = t->duetime.dwHighDateTime;
    return PyLong_FromLongLong(i.QuadPart);
}

int
pxtimer_set_duetime(PxTimerObject *t, PyObject *o, void *closure)
{
    if (!FILETIME_FromPyObject(&t->duetime, o))
        return -1;
    return 0;
}

PyObject *
pxtimer_get_period(PxTimerObject *t, void *closure)
{
    return PyLong_FromUnsignedLong(t->period);
}

int
pxtimer_set_period(PxTimerObject *t, PyObject *o, void *closure)
{
    if (!PyLong_Check(o)) {
        PyErr_SetString(PyExc_ValueError,
                        "period must be an integer");
        return -1;
    }

    t->period = PyLong_AsLong(o);

    /* Update the timer (to pick up the new period) if it's already been
     * started. */
    if (PxTimer_Valid(t) && PxTimer_IsSet(t))
        SetThreadpoolTimer(t->ptp_timer,
                           &t->duetime,
                           t->period,
                           t->window_length);

    return 0;
}

PyObject *
pxtimer_get_window_length(PxTimerObject *t, void *closure)
{
    return PyLong_FromUnsignedLong(t->window_length);
}

int
pxtimer_set_window_length(PxTimerObject *t, PyObject *o, void *closure)
{
    if (!PyLong_Check(o)) {
        PyErr_SetString(PyExc_ValueError,
                        "window_length must be an integer");
        return -1;
    }

    t->window_length = PyLong_AsLong(o);
    return 0;
}

PyObject *
pxtimer_get_func(PxTimerObject *t, void *closure)
{
    PyObject *func = t->ctx->func;
    Py_INCREF(func);
    return func;
}

int
pxtimer_set_func(PxTimerObject *t, PyObject *o, void *closure)
{
    if (!PyCallable_Check(o)) {
        PyErr_SetString(PyExc_ValueError,
                        "func must be a callable object");
        return -1;
    }

    Py_CLEAR(t->ctx->func);
    Py_INCREF(o);
    t->ctx->func = o;
    return 0;
}

PyObject *
pxtimer_get_args(PxTimerObject *t, void *closure)
{
    PyObject *args = t->ctx->args;
    PyObject *result = (args ? args : Py_None);
    Py_INCREF(result);
    return result;
}

int
pxtimer_set_args(PxTimerObject *t, PyObject *o, void *closure)
{
    if (!PyTuple_Check(o)) {
        PyErr_SetString(PyExc_ValueError,
                        "args must be a tuple object");
        return -1;
    }

    Py_CLEAR(t->ctx->args);
    Py_INCREF(o);
    t->ctx->args = o;
    return 0;
}

PyObject *
pxtimer_get_kwds(PxTimerObject *t, void *closure)
{
    PyObject *kwds = t->ctx->kwds;
    PyObject *result = (kwds ? kwds : Py_None);
    Py_INCREF(result);
    return result;
}

int
pxtimer_set_kwds(PxTimerObject *t, PyObject *o, void *closure)
{
    if (!PyDict_Check(o)) {
        PyErr_SetString(PyExc_ValueError,
                        "kwds must be a dict object");
        return -1;
    }

    Py_CLEAR(t->ctx->kwds);
    Py_INCREF(o);
    t->ctx->kwds = o;
    return 0;
}

PyObject *
pxtimer_get_exception_handler(PxTimerObject *t, void *closure)
{
    PyObject *errback = t->ctx->errback;
    PyObject *result = (errback ? errback : Py_None);
    Py_INCREF(result);
    return result;
}

int
pxtimer_set_exception_handler(PxTimerObject *t, PyObject *o, void *closure)
{
    if (!PyCallable_Check(o)) {
        PyErr_SetString(PyExc_ValueError,
                        "exception_handler must be a callable object");
        return -1;
    }

    Py_CLEAR(t->ctx->errback);
    Py_INCREF(o);
    t->ctx->errback = o;
    return 0;
}

PyObject *
pxtimer_get_is_set(PxTimerObject *t, void *closure)
{
    return PyBool_FromLong(PxTimer_IsSet(t));
}

int
pxtimer_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *duetime = NULL, *period = NULL, *func = NULL;
    PyObject *_args = NULL, *_kwds = NULL;
    PyObject *exception_handler = NULL;
    PyObject *window_length = NULL;
    PxTimerObject *t = (PxTimerObject *)self;

    static char *kwlist[] = {
        "duetime",
        "period",
        "func",
        "args",
        "kwds",
        "exception_handler",
        "window_length",
        NULL
    };

    static const char *fmt = "OOO|OOOO:timer.__init__()";


    if (!PyArg_ParseTupleAndKeywords(args, kwds, fmt, kwlist,
                                     &duetime,
                                     &period,
                                     &func,
                                     &_args,
                                     &_kwds,
                                     &exception_handler,
                                     &window_length))
        return -1;

    if (pxtimer_set_duetime(t, duetime, NULL) < 0)
        return -1;

    if (pxtimer_set_period(t, period, NULL) < 0)
        return -1;

    if (pxtimer_set_func(t, func, NULL) < 0)
        return -1;

    if (_args && pxtimer_set_args(t, _args, NULL) < 0)
        return -1;

    if (_kwds && pxtimer_set_kwds(t, _kwds, NULL) < 0)
        return -1;

    if (exception_handler &&
        pxtimer_set_exception_handler(t, exception_handler, NULL) < 0)
        return -1;

    if (window_length && pxtimer_set_window_length(t, window_length, NULL) < 0)
        return -1;

    PxTimer_SET_VALID(t);

    return 0;
}

static PyGetSetDef PxTimer_GetSetList[] = {
    {
        "data",
        (getter)pxtimer_get_data,
        (setter)NULL,
        "the results of the timer function are accessible via the `data` "
        "attribute; when accessed, a read-lock is automatically obtained "
        "and the object is copied into a new object, which is then returned "
        "(thus, if you need to refer to the data more than once, don't keep "
        "calling `t.data`, take a copy via `data = t.data` and use that)"
    },
    {
        "duetime",
        (getter)pxtimer_get_duetime,
        (setter)pxtimer_set_duetime,
        "indicates when the timer will be invoked; use datetime.timedelta() "
        "to specify relative times, and datetime.datetime() for absolute times"
    },
    {
        "period",
        (getter)pxtimer_get_period,
        (setter)pxtimer_set_period,
        "if non-zero, indicates the number of milliseconds between subsequent "
        "invocations of the timer; use this if you want your timer to repeat "
        "at a given rate until stopped"
    },
    {
        "func",
        (getter)pxtimer_get_func,
        (setter)pxtimer_set_func,
        "the function the timer will execute during callback"
    },
    {
        "args",
        (getter)pxtimer_get_args,
        (setter)pxtimer_set_args,
        "if set, the args that will be passed to the function when "
        "executed during the callback"
    },
    {
        "kwds",
        (getter)pxtimer_get_kwds,
        (setter)pxtimer_set_kwds,
        "if set, the keywords that will be passed to the function when "
        "executed during the callback"
    },
    {
        "exception_handler",
        (getter)pxtimer_get_exception_handler,
        (setter)pxtimer_set_exception_handler,
        "if set, a function that will be called if the timer encounters an "
        "exception during callback execution; the function signature must "
        "expect a 3-part tuple; the first item will be the timer, the second "
        "will be a string that, if set, refers to an offending system call, "
        "and the third will be another 3-part tuple containing the exception "
        "information (in the format (exc_type, exc_value, exc_traceback))"
    },
    {
        "is_set",
        (getter)pxtimer_get_is_set,
        (setter)NULL,
        "boolean indicating whether or not the timer has been set (started)",
    },
    {
        "duration",
        (getter)pxtimer_get_duration,
        (setter)NULL,
        "number of microseconds taken for the timer callback to execute",
    },
    {
        "critical_section_contention",
        (getter)pxtimer_get_critical_section_contention,
        (setter)NULL,
        "number of times the timer callback was entered, but the critical "
        "section was already held because a previous timer run had not yet "
        "finished executing (this can happen when period is too short "
        "relative to the execution duration of the callback)"
    },
    { NULL },
};

static PyMethodDef PxTimer_Methods[] = {
    {
        "start",
        (PyCFunction)pxtimer_start,
        METH_NOARGS,
        "start the timer"
    },
    {
        "stop",
        (PyCFunction)pxtimer_stop,
        METH_NOARGS,
        "stop the timer"
    },
    {
        "shutdown",
        (PyCFunction)pxtimer_shutdown,
        METH_NOARGS,
        "shuts the timer down (a shutdown timer cannot be started again)"
    },
    { NULL, NULL }
};

PyTypeObject PxTimer_Type = {
    PyVarObject_HEAD_INIT(0, 0)
    "timer",                                    /* tp_name */
    sizeof(PxTimerObject),                      /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)pxtimer_dealloc,                /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_reserved */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT      |
        Py_TPFLAGS_BASETYPE,                    /* tp_flags */
    "Parallel Timer Object",                    /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    PxTimer_Methods,                            /* tp_methods */
    0,                                          /* tp_members */
    PxTimer_GetSetList,                         /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    pxtimer_init,                               /* tp_init */
    pxtimer_alloc,                              /* tp_alloc */
    pxtimer_new,                                /* tp_new */
    0,                                          /* tp_free */
    0,                                          /* tp_is_gc */
    0,                                          /* tp_bases */
    0,                                          /* tp_mro */
    0,                                          /* tp_cache */
    0,                                          /* tp_subclasses */
    0,                                          /* tp_weaklist */
    0,                                          /* tp_del */
    0,                                          /* tp_version_tag */
    0,                                          /* tp_copy */
};

/*
PyObject *
PxTimer_StartTimers(void)
{
    PLIST_ENTRY entry, head;
    PxTimerObject *timer;
    PxState *px;
    PyObject *result = NULL;

    px = PXSTATE();
    if (!px) {
        PyErr_SetString(PyExc_RuntimeError,
                        "start_timers() called before any timers created");
        return NULL;
    }

    EnterCriticalSection(&px->timers_cs);

    if (IsListEmpty(&px->timers)) {
        if (px->num_timers)
            __debugbreak();
        goto end;
    } else {
        if (!px->num_timers)
            __debugbreak();
        goto end;
    }

    head = &px->timers;
    entry = head->Flink;
    do {
        timer = CONTAINING_RECORD(entry, PxTimerObject, entry);
        if (!_protect((PyObject *)timer)) {
            if (!PyErr_Occurred())
                __debugbreak();
            goto end;
        }

        if (!pxtimer_start((PyObject *)timer)) {
            if (!PyErr_Occurred())
                __debugbreak();
            goto end;
        }

    } while ((entry = entry->Flink) != head);

    result = Py_None;
end:
    LeaveCriticalSection(&px->timers_cs);
    Py_XINCREF(result);
    return result;
}
*/


/* vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                  */
