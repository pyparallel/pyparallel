/* PxTimer object implementation.
 * Copyright 2015, Trent Nelson <trent@trent.me>
 */

#include "Python.h"
#include "pxtimerobject.h"

static PyTypeObject PxTimerObject_Type;

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
PxTimer_ShutdownCallback(
    _Inout_ PTP_CALLBACK_INSTANCE instance,
    _Inout_opt_ PVOID context,
    _Inout_ PTP_TIMER timer
)
{
    PxTimerObject *t = (PxTimerObject *)context;
    PxContext *c = t->ctx;
    PxState *px = c->px;

    _PyParallel_EnteredCallback(c, instance);

    EnterCriticalSection(&t->cs);
    LeaveCriticalSection(&t->cs);
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
    Heap *snapshot;

    _PyParallel_EnteredCallback(c, instance);

    EnterCriticalSection(&t->cs);

    if (PxTimer_STOP_REQUESTED(t)) {
        PxTimer_SET_STOPPED(t);
        goto end;
    }

    /* 'STARTED' is set if a timer has ever run.  'RUNNING' is set when it's
     * actually running, and then unset at the end.  And _PxTimer_SetActive()
     * is used to set the TLS 'active_timer' which allows the timer to quickly
     * differentiate between calls originating from itself (e.g. calling stop()
     * or setting an attribute) versus external calls (from other threads). */
    PxTimer_SET_STARTED(t);
    PxTimer_SET_RUNNING(t);
    _PxTimer_SetActive(t);

    snapshot = PxContext_HeapSnapshot(c);

    result = PyEval_CallObjectWithKeywords(c->func, c->args, c->kwds);

    if (!result)
        PxTimer_EXCEPTION();

    if (pxtimer_set_data(t, result))
        PxTimer_EXCEPTION();

    PxContext_RollbackHeap(c, &snapshot);

end:
    PxTimer_UNSET_RUNNING(t);
    _PxTimer_ClearActive();
    LeaveCriticalSection(&t->cs);
}


BOOL
CALLBACK
PxTimer_StartCallback(
    PINIT_ONCE init_once,
    PVOID param,
    PVOID *context
)
{
    PxTimerObject *t = (PxTimerObject *)context;

    assert(PxTimer_VALID(t));

    PxTimer_SET_START_REQUESTED(t);
    SetThreadpoolTimer(t->ptp_timer,
                       &t->duetime,
                       t->period,
                       t->window_length);

    return TRUE;
}

BOOL
CALLBACK
PxTimer_StopCallback(
    PINIT_ONCE init_once,
    PVOID param,
    PVOID *context
)
{
    PxTimerObject *t = (PxTimerObject *)context;

    assert(PxTimer_VALID(t));

    /* Toggle the stop requested flag and set another timer with NULL
     * parameters.  This will synchronously trigger another timer callback,
     * which tests for the flag and registers the stop, which we use to
     * indicate that the timer is not and will not run again. */
    PxTimer_SET_STOP_REQUESTED(t);
    SetThreadpoolTimer(t->ptp_timer, NULL, 0, 0);

    return TRUE;
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
    PxSocket *s;
    PxTimerObject *t;
    PyObject *parent;

    if (nitems != 0)
        __debugbreak();

    if (type != &PxTimerObject_Type)
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

    t->heap_override = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
    if (!t->heap_override)
        PxTimer_SYSERROR("HeapCreate(t->heap_override)");

    c->io_obj = (PyObject *)t;
    c->io_type = Px_IOTYPE_TIMER;

    if (!PxContext_InitObject(c, c->io_obj, type, 0))
        PxTimer_FATAL();

    /* Used in concert with stop() and pxtimer_dealloc(). */
    if (!Py_PXCTX())
        Py_REFCNT(t) = 2;

    if (!_PyParallel_ProtectObject(c->io_obj))
        __debugbreak();

    InitializeSRWLock(&t->data_srwlock);

    t->ctx = c;

    InitializeCriticalSectionAndSpinCount(&t->cs, 4000);

    _PxState_RegisterTimer(t);

end:
    Py_RETURN_NONE;
}

int
pxtimer_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    PxTimerObject *t = (PxTimerObject *)self;

    /* xxx: remember to activate context and call the relevant tp_init() slot
     * for generic object initialization. */

    return -1;
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
    Px_CLOSE_THREADPOOL_WORK(t->shutdown);
    Px_HEAP_DESTROY(c->heap_handle);
    Px_HEAP_DESTROY(t->heap_override);
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

    if (!PxTimer_VALID(t)) {
        PyErr_SetString(PyExc_ValueError, "invalid timer object");
        return NULL;
    }

    InitOnceExecuteOnce(&t->start, PxTimer_StartCallback, t, NULL);

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

    if (!PxTimer_VALID(t)) {
        PyErr_SetString(PyExc_ValueError, "invalid timer object");
        return NULL;
    }

    if (t == PxTimer_GetActive())
        /* Shortcut if stop() is called within our own callback. */
        PxTimer_SET_STOP_REQUESTED(t);
    else
        InitOnceExecuteOnce(&t->stop, PxTimer_StopCallback, t, NULL);

    Py_RETURN_NONE;
}

PyDoc_STRVAR(
    pxtimer_set_timeout_doc,
    "sets the timeout\n\n"
);

PyObject *
pxtimer_set_timeout(PyObject *self, PyObject *delta)
{
    PxTimerObject *t = (PxTimerObject *)self;

    Py_RETURN_NONE;
}

PyObject *
pxtimer_shutdown(PyObject *self)
{
    return NULL;
}

int
pxtimer_set_data(PxTimerObject *t, PyObject *data)
{
    PyObject *old_data = t->data;

    if (!PyObject_Copyable(data)) {
        PyErr_SetString(PyExc_ValueError,
                        "cannot set non-copyable object");
        return -1;
    }

    if (t != PxTimer_GetActive()) {
        PyErr_SetString(PyExc_RuntimeError,
                        "cannot set timer data from a different thread");
        return -1;
    }

    _PyParallel_SetHeapOverride(t->heap_override);

    AcquireSRWLockExclusive(&t->data_srwlock);
    t->data = PyObject_Copy(data);
    ReleaseSRWLockExclusive(&t->data_srwlock);

    _PyParallel_RemoveHeapOverride();

    /* This may need to be changed down the track if PyObject_Copy()
     * implementations do anything other than a single malloc call. */
    if (old_data)
        HeapFree(t->heap_override, 0, old_data);

    return 0;
}

PyObject *
pxtimer_get_data(PxTimerObject *t)
{
    PyObject *data;

    AcquireSRWLockShared(&t->data_srwlock);
    if (!t->data || t->data == Py_None)
        data = Py_None;
    else if (Py_ISPY(t->data))
        data = Py_None;
    else
        data = PyObject_Copy(t->data);
    ReleaseSRWLockShared(&t->data_srwlock);

    return data;
}


static PyGetSetDef PxTimerGetSetList[] = {
    { "data", (getter)pxtimer_get_data },
    { NULL },
}

#define _PXTIMER(n, a) _METHOD(pxtimer, n, a)
#define _PXTIMER_N(n) _PXTIMER(n, METH_NOARGS)
#define _PXTIMER_O(n) _PXTIMER(n, METH_O)
#define _PXTIMER_V(n) _PXTIMER(n, METH_VARARGS)

static PyMethodDef PxTimerMethods[] = {
    _PXTIMER_N(stop),
    _PXTIMER_N(set_timeout),
    { NULL, NULL }
};

#define _MEMBER(n, t, c, f, d) {#n, t, offsetof(c, n), f, d}
#define _PXTIMERMEM(n, t, f, d)  _MEMBER(n, t, PxTimerObject, f, d)
#define _PXTIMER_CB(n)        _PXTIMERMEM(n, T_OBJECT,    0, #n " callback")
#define _PXTIMER_ATTR_O(n)    _PXTIMERMEM(n, T_OBJECT_EX, 0, #n " callback")
#define _PXTIMER_ATTR_OR(n)   _PXTIMERMEM(n, T_OBJECT_EX, 1, #n " callback")
#define _PXTIMER_ATTR_I(n)    _PXTIMERMEM(n, T_INT,       0, #n " attribute")
#define _PXTIMER_ATTR_IR(n)   _PXTIMERMEM(n, T_INT,       1, #n " attribute")
#define _PXTIMER_ATTR_UI(n)   _PXTIMERMEM(n, T_UINT,      0, #n " attribute")
#define _PXTIMER_ATTR_UIR(n)  _PXTIMERMEM(n, T_UINT,      1, #n " attribute")
#define _PXTIMER_ATTR_LL(n)   _PXTIMERMEM(n, T_LONGLONG,  0, #n " attribute")
#define _PXTIMER_ATTR_LLR(n)  _PXTIMERMEM(n, T_LONGLONG,  1, #n " attribute")
#define _PXTIMER_ATTR_ULL(n)  _PXTIMERMEM(n, T_ULONGLONG, 0, #n " attribute")
#define _PXTIMER_ATTR_ULLR(n) _PXTIMERMEM(n, T_ULONGLONG, 1, #n " attribute")
#define _PXTIMER_ATTR_B(n)    _PXTIMERMEM(n, T_BOOL,      0, #n " attribute")
#define _PXTIMER_ATTR_BR(n)   _PXTIMERMEM(n, T_BOOL,      1, #n " attribute")
#define _PXTIMER_ATTR_D(n)    _PXTIMERMEM(n, T_DOUBLE,    0, #n " attribute")
#define _PXTIMER_ATTR_DR(n)   _PXTIMERMEM(n, T_DOUBLE,    1, #n " attribute")
#define _PXTIMER_ATTR_S(n)    _PXTIMERMEM(n, T_STRING,    0, #n " attribute")

static PyMemberDef PxTimerMembers[] = {
    _PXTIMER_ATTR_LLR(duetime),
    _PXTIMER_ATTR_UIR(period),
    _PXTIMER_ATTR_UIR(window_length),
    _PXTIMER_ATTR_OR(parent),

    { NULL }
};


static PyTypeObject PxTimerObject_Type = {
    PyVarObject_HEAD_INIT(0, 0)
    "_parallel.timer",                          /* tp_name */
    sizeof(PxTimerObject),                      /* tp_basicsize */
    0,                                          /* tp_itemsize */
    pxtimer_dealloc,                            /* tp_dealloc */
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
    PyObject_GenericGetAttr,                    /* tp_getattro */
    PyObject_GenericSetAttr,                    /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                         /* tp_flags */
    "Parallel Timer Object",                    /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    PxTimerMethods,                             /* tp_methods */
    PxTimerMembers,                             /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    pxtimer_init,                               /* tp_init */
    pxtimer_alloc,                              /* tp_alloc */
    0,                                          /* tp_new */
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

PyObject *
PxTimer_New(
    PyObject *duetime,
    PyObject *period,
    PyObject *window_length,
    PyObject *func,
    PyObject *args,
    PyObject *kwds,
    PyObject *errback
)
{
    return NULL;
}

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


/* vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                  */
