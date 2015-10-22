/* PxThread object implementation.
 * Copyright 2015, Trent Nelson <trent@trent.me>
 */

#include "Python.h"
#include "pxthreadobject.h"

static PyTypeObject PxThread_Type;

void
CALLBACK
PxThread_ShutdownWorkCallback(
    _Inout_ PTP_CALLBACK_INSTANCE instance,
    _Inout_opt_ PVOID context,
    _Inout_ PTP_WORK work
)
{
    PxThreadObject *t = (PxThreadObject *)context;
    PxContext *c = t->ctx;
    PxState *px = c->px;

    SetEvent(t->shutdown_event);

    WaitForSingleObject(t->thread_handle, INFINITE);
    PxThread_UpdateTimes(t);

    EnterCriticalSection(&t->cs);

    Py_REFCNT(t) -= 1;

    if (Py_REFCNT(t) < 0)
        __debugbreak();

    ((PyObject *)t)->is_px = _Py_NOT_PARALLEL;
    Py_PXFLAGS(t) &= ~Py_PXFLAGS_ISPX;
    Py_PXFLAGS(t) |=  (Py_PXFLAGS_ISPY | Py_PXFLAGS_WASPX);

    PxThread_SET_SHUTDOWN(t);

    LeaveCriticalSection(&t->cs);

    PxContext_CallbackComplete(c);
}

BOOL
CALLBACK
PxThread_StartOnceCallback(
    PINIT_ONCE init_once,
    PVOID param,
    PVOID *context
)
{
    PxThreadObject *t = (PxThreadObject *)param;
    PxContext *c = t->ctx;

    assert(PxThread_Valid(t));

    PxThread_SET_START_REQUESTED(t);
    if (ResumeThread(t->thread_handle) == -1)
        PxThread_SYSERROR("ResumeThread");

    return TRUE;
end:
    return FALSE;
}

BOOL
CALLBACK
PxThread_ShutdownOnceCallback(
    PINIT_ONCE init_once,
    PVOID param,
    PVOID *context
)
{
    PxThreadObject *t = (PxThreadObject *)param;

    PxThread_SET_SHUTDOWN_REQUESTED(t);
    //pxthread_suspend((PyObject *)t);
    SubmitThreadpoolWork(t->shutdown_work);

    return TRUE;
}

int
PxThread_Valid(PxThreadObject *t)
{
    return PxThread_VALID(t);
}

DWORD
WINAPI
PxThread_Main(LPVOID param)
{
    PxThreadObject *t = (PxThreadObject *)param;
    PxContext *c = t->ctx;
    PxState *px = c->px;
    PyObject *result;
    Heap *prev_snapshot, *this_snapshot;
    LARGE_INTEGER start, end, elapsed, frequency;
    int expect_snapshot = 1;
    int expected_snapshot_group = 0;

    _PyParallel_EnteredCallback(c, NULL);

    if (PyErr_Occurred())
        __debugbreak();

    EnterCriticalSection(&t->cs);

    if (t->task_id) {
        if (!t->task_name)
            __debugbreak();
        if (t->second_task_id) {
            /* Not yet supported. */
            __debugbreak();
        } else {
            HANDLE h = AvSetMmThreadCharacteristics(t->task_name,
                                                    &t->task_index);
            if (!h)
                PxThread_SYSERROR("AvSetMmThreadCharacteristics");
            t->avrt_handle = h;
        }
    }

begin:
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    if (t->avrt_handle) {
        if (!AvQuerySystemResponsiveness(t->avrt_handle,
                                         &t->system_responsiveness))
            PxThread_SYSERROR("AvQuerySystemResponsiveness");
    }

    switch (c->h->group) {
        case 1:
            /* Context's active heap belongs to the context. */
            prev_snapshot = &t->snapshot;
            this_snapshot = &c->snapshot;
            break;
        case 2:
            /* Context's active heap belongs to the thread. */
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
        PxThread_EXCEPTION();

    if (PyIter_Check(result))
        __debugbreak();
    else {
        AcquireSRWLockExclusive(&t->data_srwlock);
        t->data = result;
        ReleaseSRWLockExclusive(&t->data_srwlock);
    }

    if (++t->count < 0) {
        t->count_wrapped++;
        t->count = 1;
    } else if (t->count == 1)
        expect_snapshot = 0;

    /* We now switch to our alternate heap and roll back any snapshot if
     * applicable. */
    switch (c->h->group) {
        case 1:
            /* Context's active heap belongs to the context.  Switch to the
             * thread heap. */
            t->last_ctx_heap = c->h;
            c->h = t->last_thread_heap;
            expected_snapshot_group = 2;
            break;
        case 2:
            /* Context's active heap belongs to the thread.  Switch to the
             * context's heap. */
            t->last_thread_heap = c->h;
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
        SecureZeroMemory(prev_snapshot, sizeof(Heap));

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

    if (t->interval) {
        DWORD result = WaitForSingleObject(t->shutdown_event, t->interval);
        if (result == WAIT_TIMEOUT)
            goto begin;
    }

    /* xxx todo: shutdown */
    //pxthread_shutdown((PyObject *)t);
    return 0;
}


PyObject *
pxthread_alloc(PyTypeObject *type, Py_ssize_t nitems)
{
    Context *c, *x = PxContext_GetActive();
    PxState *px;
    PxThreadObject *t;
    PyObject *result = NULL;

    if (nitems != 0)
        __debugbreak();

    if (type != &PxThread_Type)
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
        __debugbreak();
    } else {
        c->ptp_cbe = px->ptp_cbe;
    }

    t = (PxThreadObject *)PxContext_Malloc(c, sizeof(PxThreadObject), 0, 0);
    if (!t)
        PxThread_FATAL();

    t->shutdown_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!t->shutdown_event)
        PxThread_FATAL();

    c->io_obj = (PyObject *)t;
    c->io_type = Px_IOTYPE_THREAD;

    if (!PxContext_InitObject(c, c->io_obj, type, 0))
        PxThread_FATAL();

    t->last_ctx_heap = c->h;
    _PxContext_InitAdditionalHeap(c, &t->heap, 0);

    if (t->last_ctx_heap == c->h)
        /* Should be altered by call above. */
        __debugbreak();

    t->last_thread_heap = c->h;

    if (t->last_thread_heap->group == t->last_ctx_heap->group)
        __debugbreak();

    /* Used in concert with stop() and pxthread_dealloc(). */
    if (!Py_PXCTX())
        Py_REFCNT(t) = 2;

    InitializeSRWLock(&t->data_srwlock);
    InitializeSRWLock(&t->times_srwlock);

    t->ctx = c;

    InitializeCriticalSectionAndSpinCount(&t->cs, 4000);

    t->shutdown_work = CreateThreadpoolWork(PxThread_ShutdownWorkCallback,
                                            t,
                                            c->ptp_cbe);

    InitOnceInitialize(&t->start_once);
    InitOnceInitialize(&t->shutdown_once);

    t->data = Py_None;

    _PxState_RegisterContext(c);

    result = (PyObject *)t;
end:
    return result;
}

int
PxThread_UpdateTimes(PxThreadObject *t)
{
    BOOL success;

    if (t->thread_id != GetCurrentThreadId())
        __debugbreak();

    AcquireSRWLockExclusive(&t->times_srwlock);
    success = GetThreadTimes(t->thread_handle,
                             &t->creationtime,
                             &t->exittime,
                             &t->kerneltime,
                             &t->usertime);
    ReleaseSRWLockExclusive(&t->times_srwlock);

    return success;
}

PyObject *
pxthread_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PxThreadObject *t;

    t = (PxThreadObject *)type->tp_alloc(type, 0);
    if (!t)
        return NULL;

    /* Do we need to do anything else here?  Seems like we could let the
     * generic PyType_GenericNew() handle all of this functionality. */
    return (PyObject *)t;
}

void
pxthread_dealloc(PxThreadObject *t)
{
    PxContext *c = t->ctx;
    PxState *px = c->px;

    if (t->thread_id == GetCurrentThreadId())
        __debugbreak();

    _PxState_UnregisterContext(c);

    DeleteCriticalSection(&t->cs);

    Px_HEAP_DESTROY(c->heap_handle);
    Px_FREE(c);

    InterlockedDecrement(&px->active);
    InterlockedDecrement(&px->contexts_active);
}

PyDoc_STRVAR(
    pxthread_start_doc,
    "starts a thread (can only be called once)\n\n"
);

PyObject *
pxthread_start(PyObject *self)
{
    PxThreadObject *t = (PxThreadObject *)self;

    if (!PxThread_Valid(t)) {
        PyErr_SetString(PyExc_ValueError, "invalid thread object");
        return NULL;
    }

    InitOnceExecuteOnce(&t->start_once, PxThread_StartOnceCallback, t, NULL);

    Py_RETURN_NONE;
}

PyDoc_STRVAR(
    pxthread_resume_doc,
    "resume a suspended thread\n\n"
);

PyObject *
pxthread_resume(PyObject *self)
{
    PxThreadObject *t = (PxThreadObject *)self;

    if (!PxThread_Valid(t)) {
        PyErr_SetString(PyExc_ValueError, "invalid thread object");
        return NULL;
    }

    InitOnceExecuteOnce(&t->start_once, PxThread_StartOnceCallback, t, NULL);

    Py_RETURN_NONE;
}

PyDoc_STRVAR(
    pxthread_suspend_doc,
    "suspend a running thread"
);

PyObject *
pxthread_suspend(PyObject *self)
{
    PxThreadObject *t = (PxThreadObject *)self;

    if (t->thread_id == GetCurrentThreadId())
        __debugbreak();

    SuspendThread(t->thread_handle);

    Py_RETURN_NONE;
}

PyObject *
pxthread_shutdown(PyObject *self)
{
    PxThreadObject *t = (PxThreadObject *)self;

    InitOnceExecuteOnce(&t->shutdown_once,
                        PxThread_ShutdownOnceCallback,
                        t,
                        NULL);

    Py_RETURN_NONE;
}

PyObject *
pxthread_get_data(PxThreadObject *t, void *closure)
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

    return data;
}

PyObject *
pxthread_get_duration(PxThreadObject *t, void *closure)
{
    LARGE_INTEGER d;

    AcquireSRWLockShared(&t->duration_srwlock);
    d.QuadPart = t->duration.QuadPart;
    ReleaseSRWLockShared(&t->duration_srwlock);

    return PyLong_FromLongLong(d.QuadPart);
}

PyObject *
pxthread_get_critical_section_contention(PxThreadObject *t, void *closure)
{
    return PyLong_FromLong(t->cs_contention);
}

PyObject *
pxthread_get_creationtime(PxThreadObject *t, void *closure)
{
    LARGE_INTEGER i;
    i.LowPart = t->creationtime.dwLowDateTime;
    i.HighPart = t->creationtime.dwHighDateTime;
    return PyLong_FromLongLong(i.QuadPart);
}

int
pxthread_set_creationtime(PxThreadObject *t, PyObject *o, void *closure)
{
    if (!FILETIME_FromPyObject(&t->creationtime, o))
        return -1;
    return 0;
}

PyObject *
pxthread_get_exittime(PxThreadObject *t, void *closure)
{
    LARGE_INTEGER i;
    i.LowPart = t->exittime.dwLowDateTime;
    i.HighPart = t->exittime.dwHighDateTime;
    return PyLong_FromLongLong(i.QuadPart);
}

int
pxthread_set_exittime(PxThreadObject *t, PyObject *o, void *closure)
{
    if (!FILETIME_FromPyObject(&t->exittime, o))
        return -1;
    return 0;
}

PyObject *
pxthread_get_kerneltime(PxThreadObject *t, void *closure)
{
    LARGE_INTEGER i;
    i.LowPart = t->kerneltime.dwLowDateTime;
    i.HighPart = t->kerneltime.dwHighDateTime;
    return PyLong_FromLongLong(i.QuadPart);
}

int
pxthread_set_kerneltime(PxThreadObject *t, PyObject *o, void *closure)
{
    if (!FILETIME_FromPyObject(&t->kerneltime, o))
        return -1;
    return 0;
}

PyObject *
pxthread_get_usertime(PxThreadObject *t, void *closure)
{
    LARGE_INTEGER i;
    i.LowPart = t->usertime.dwLowDateTime;
    i.HighPart = t->usertime.dwHighDateTime;
    return PyLong_FromLongLong(i.QuadPart);
}

int
pxthread_set_usertime(PxThreadObject *t, PyObject *o, void *closure)
{
    if (!FILETIME_FromPyObject(&t->usertime, o))
        return -1;
    return 0;
}

PyObject *
pxthread_get_interval(PxThreadObject *t, void *closure)
{
    return PyLong_FromUnsignedLong(t->interval);
}

int
pxthread_set_interval(PxThreadObject *t, PyObject *o, void *closure)
{
    if (!PyLong_Check(o)) {
        PyErr_SetString(PyExc_ValueError,
                        "interval must be an integer");
        return -1;
    }

    t->interval = PyLong_AsLong(o);

    return 0;
}

PyObject *
pxthread_get_func(PxThreadObject *t, void *closure)
{
    PyObject *func = t->ctx->func;
    Py_INCREF(func);
    return func;
}

int
pxthread_set_func(PxThreadObject *t, PyObject *o, void *closure)
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
pxthread_get_args(PxThreadObject *t, void *closure)
{
    PyObject *args = t->ctx->args;
    PyObject *result = (args ? args : Py_None);
    Py_INCREF(result);
    return result;
}

int
pxthread_set_args(PxThreadObject *t, PyObject *o, void *closure)
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
pxthread_get_kwds(PxThreadObject *t, void *closure)
{
    PyObject *kwds = t->ctx->kwds;
    PyObject *result = (kwds ? kwds : Py_None);
    Py_INCREF(result);
    return result;
}

int
pxthread_set_kwds(PxThreadObject *t, PyObject *o, void *closure)
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
pxthread_get_exception_handler(PxThreadObject *t, void *closure)
{
    PyObject *errback = t->ctx->errback;
    PyObject *result = (errback ? errback : Py_None);
    Py_INCREF(result);
    return result;
}

int
pxthread_set_exception_handler(PxThreadObject *t, PyObject *o, void *closure)
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

int
pxthread_set_thread_characteristics(PxThreadObject *t,
                                    PyObject *o,
                                    void *closure)
{
    if (PyLong_Check(o)) {
        t->task_id = (TASK_ID)PyLong_AsLong(o);
        if (!TaskIdToTaskName(t->task_id, &t->task_name)) {
            PyErr_SetString(PyExc_ValueError,
                            "thread_characteristics: invalid task ID");
            return -1;
        }
        return 0;
    }
    PyErr_SetString(PyExc_ValueError,
                    "thread_characteristics: invalid value");
    return -1;
}

ULONG
PxThread_GetSystemResponsiveness(PxThreadObject *t)
{
    return t->system_responsiveness;
}

PyObject *
pxthread_get_system_responsiveness(PxThreadObject *t, void *closure)
{
    return PyLong_FromUnsignedLong(PxThread_GetSystemResponsiveness(t));
}

PyObject *
pxthread_get_started(PxThreadObject *t, void *closure)
{
    return PyBool_FromLong(PxThread_STARTED(t));
}

PyObject *
pxthread_get_count(PxThreadObject *t, void *closure)
{
    return PyLong_FromLongLong(t->count);
}

PyObject *
pxthread_get_count_wrapped(PxThreadObject *t, void *closure)
{
    return PyLong_FromLong(t->count_wrapped);
}

int
pxthread_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *func = NULL, *_args = NULL, *_kwds = NULL;
    PyObject *interval = NULL;
    PyObject *exception_handler = NULL;
    PyObject *thread_characteristics = NULL;
    PxThreadObject *t = (PxThreadObject *)self;
    PxContext *c = t->ctx;

    static char *kwlist[] = {
        "func",
        "args",
        "kwds",
        "interval",
        "exception_handler",
        "thread_characteristics",
        NULL
    };

    static const char *fmt = "O|OOOO:thread.__init__()";

    if (!PyArg_ParseTupleAndKeywords(args, kwds, fmt, kwlist,
                                     &func,
                                     &_args,
                                     &_kwds,
                                     &interval,
                                     &exception_handler,
                                     &thread_characteristics))
        return -1;

    if (pxthread_set_func(t, func, NULL) < 0)
        return -1;

    if (_args && pxthread_set_args(t, _args, NULL) < 0)
        return -1;

    if (_kwds && pxthread_set_kwds(t, _kwds, NULL) < 0)
        return -1;

    if (interval && pxthread_set_interval(t, interval, NULL) < 0)
        return -1;

    if (exception_handler &&
        pxthread_set_exception_handler(t, exception_handler, NULL) < 0)
        return -1;

    if (thread_characteristics &&
        pxthread_set_thread_characteristics(t,
                                            thread_characteristics,
                                            NULL) < 0)
        return -1;

    t->thread_handle = CreateThread(NULL,
                                    0,
                                    &PxThread_Main,
                                    t,
                                    CREATE_SUSPENDED,
                                    &t->thread_id);
    if (!t->thread_handle)
        PxThread_SYSERROR("CreateThread");

    PxThread_SET_VALID(t);

    return 0;
end:
    return -1;
}

static PyGetSetDef PxThread_GetSetList[] = {
    {
        "data",
        (getter)pxthread_get_data,
        (setter)NULL,
        "the results of the thread function are accessible via the `data` "
        "attribute; when accessed, a read-lock is automatically obtained "
        "and the object is copied into a new object, which is then returned "
        "(thus, if you need to refer to the data more than once, don't keep "
        "calling `t.data`, take a copy via `data = t.data` and use that)"
    },
    {
        "interval",
        (getter)pxthread_get_interval,
        (setter)pxthread_set_interval,
        "if non-zero, indicates the number of milliseconds between subsequent "
        "invocations of the thread; use this if you want your thread to repeat "
        "at a given rate until stopped"
    },
    {
        "func",
        (getter)pxthread_get_func,
        (setter)pxthread_set_func,
        "the function the thread will execute during callback"
    },
    {
        "args",
        (getter)pxthread_get_args,
        (setter)pxthread_set_args,
        "if set, the args that will be passed to the function when "
        "executed during the callback"
    },
    {
        "kwds",
        (getter)pxthread_get_kwds,
        (setter)pxthread_set_kwds,
        "if set, the keywords that will be passed to the function when "
        "executed during the callback"
    },
    {
        "exception_handler",
        (getter)pxthread_get_exception_handler,
        (setter)pxthread_set_exception_handler,
        "if set, a function that will be called if the thread encounters an "
        "exception during callback execution; the function signature must "
        "expect a 3-part tuple; the first item will be the thread, the second "
        "will be a string that, if set, refers to an offending system call, "
        "and the third will be another 3-part tuple containing the exception "
        "information (in the format (exc_type, exc_value, exc_traceback))"
    },
    {
        "started",
        (getter)pxthread_get_started,
        (setter)NULL,
        "boolean indicating whether or not the thread has been started",
    },
    {
        "creationtime",
        (getter)pxthread_get_creationtime,
        (setter)NULL,
        "creation time of the thread",
    },
    {
        "exittime",
        (getter)pxthread_get_exittime,
        (setter)NULL,
        "time the thread exited",
    },
    {
        "kerneltime",
        (getter)pxthread_get_kerneltime,
        (setter)NULL,
        "time spent executing in kernel mode",
    },
    {
        "usertime",
        (getter)pxthread_get_usertime,
        (setter)NULL,
        "time spent executing in user mode",
    },
    {
        "duration",
        (getter)pxthread_get_duration,
        (setter)NULL,
        "number of microseconds taken for the thread callback to execute",
    },
    {
        "count",
        (getter)pxthread_get_count,
        (setter)NULL,
        "number of times the thread has executed"
    },
    {
        "count_wrapped",
        (getter)pxthread_get_count_wrapped,
        (setter)NULL,
        "number of times the counter has wrapped around",
    },
    {
        "critical_section_contention",
        (getter)pxthread_get_critical_section_contention,
        (setter)NULL,
        "number of times the thread callback was entered, but the critical "
        "section was already held because a previous thread run had not yet "
        "finished executing (this can happen when interval is too short "
        "relative to the execution duration of the callback)"
    },
    {
        "system_responsiveness",
        (getter)pxthread_get_system_responsiveness,
        (setter)NULL,
        "if this thread has multimedia characteristics set, this will return "
        "a value between 10 and 100 percent, indicating how responsive the "
        "system currently is",
    },
    { NULL },
};

static PyMethodDef PxThread_Methods[] = {
    {
        "start",
        (PyCFunction)pxthread_start,
        METH_NOARGS,
        "starts a thread"
    },
    {
        "suspend",
        (PyCFunction)pxthread_suspend,
        METH_NOARGS,
        "suspend a running thread"
    },
    {
        "resume",
        (PyCFunction)pxthread_resume,
        METH_NOARGS,
        "resume a suspended thread"
    },
    {
        "shutdown",
        (PyCFunction)pxthread_shutdown,
        METH_NOARGS,
        "shuts the thread down"
    },
    { NULL, NULL }
};

PyTypeObject PxThread_Type = {
    PyVarObject_HEAD_INIT(0, 0)
    "thread",                                    /* tp_name */
    sizeof(PxThreadObject),                      /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)pxthread_dealloc,                /* tp_dealloc */
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
    "Parallel Thread Object",                   /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    PxThread_Methods,                           /* tp_methods */
    0,                                          /* tp_members */
    PxThread_GetSetList,                        /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    pxthread_init,                              /* tp_init */
    pxthread_alloc,                             /* tp_alloc */
    pxthread_new,                               /* tp_new */
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

/* vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                  */
