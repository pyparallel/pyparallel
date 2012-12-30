#include "Python.h"

#ifndef WITH_PARALLEL
#error "This file should only be included when WITH_PARALLEL is defined."
#endif

#ifdef __cpplus
extern "C" {
#endif

/* XXX TODO:
 *  - Review all dealloc methods -- make nullops where approp.
 *  - Review free lists!
 */

#include "pyparallel_private.h"

__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
__declspec(thread) PyParallelContext *ctx = NULL;
/*__declspec(thread) PyThreadState _PxThreadState = { NULL };*/
__declspec(thread) PyThreadState *pstate;
#define _TMPBUF_SIZE 1024

__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
long Py_MainThreadId  = -1;
long Py_MainProcessId = -1;
long Py_ParallelContextsEnabled = -1;

void *Heap_Malloc(size_t);
void *_PyHeap_Malloc(Context *c, size_t n);

void
_PyParallel_DisassociateCurrentThreadFromCallback(void)
{
    Context *c = ctx;
    if (c->disassociated)
        return;
    DisassociateCurrentThreadFromCallback((PTP_CALLBACK_INSTANCE)c->instance);
    c->disassociated = 1;
}

void
_PyParallel_BlockingCall(void)
{
    Context *c = ctx;
    Stats   *s = &(c->stats);
    Px_GUARD

    s->blocking_calls++;
    _PyParallel_DisassociateCurrentThreadFromCallback();
}

void *
Heap_Init(Context *c, size_t n)
{
    Heap  *h;
    Stats *s = &(c->stats);
    size_t size;

    if (n < Px_DEFAULT_HEAP_SIZE)
        size = Px_DEFAULT_HEAP_SIZE;
    else
        size = n;

    size = Px_CACHE_ALIGN(size);

    assert (!c->h);

    h = &(c->heap);

    h->size = size;
    h->base = h->next = HeapAlloc(c->heap_handle, 0, h->size);
    if (!h->base)
        return PyErr_SetFromWindowsErr(0);
    h->remaining = size;
    s->remaining += size;
    s->size += size;
    s->heaps++;
    c->h = h;
    h->sle_next = (Heap *)_PyHeap_Malloc(c, Px_CACHE_ALIGN(sizeof(Heap)));
    assert(h->sle_next);
    memset((void *)h->sle_next, 0, sizeof(Px_CACHE_ALIGN(sizeof(Heap))));
    return h;
}

__inline
void *
_PyHeap_Init(Context *c, Py_ssize_t n)
{
    return Heap_Init(c, n);
}


/*
static __inline
void *
Heap_Extend(Context *c)
{
    Heap *oldh = c->h;
    Heap *newh = oldh->sle_next;
    if (!Heap_Init(c, newh, oldh->size * 2))
        return PyErr_SetFromWindowsErr(0);
    assert(newh == ctx->h);
    newh->sle_prev = oldh;
    return newh;
}
*/

void *
Heap_LocalMalloc(Context *c, size_t size)
{
    void *next;
    wchar_t *fmt;

    if (size > c->tbuf_remaining) {
        next = (void *)malloc(size);
        if (!next)
            return PyErr_NoMemory();

        fmt = L"Heap_LocalMalloc: local buffer exhausted ("    \
              L"requested: %lld, available: %lld).  Resorted " \
              L"to malloc() -- note that memory will not be "  \
              L"freed!\n";
        fwprintf_s(stderr, fmt, size, c->tbuf_remaining);
        return next;
    }

    c->tbuf_mallocs++;
    c->tbuf_allocated += size;
    c->tbuf_remaining -= size;

    next = c->tbuf_next;
    c->tbuf_next = Px_PTRADD(c->tbuf_next, size);
    assert(Px_PTRADD(c->tbuf_base, c->tbuf_allocated) == c->tbuf_next);
    return next;
}

void *
_PyHeap_Malloc(Context *c, size_t n)
{
    void  *next;
    Heap  *h;
    Stats *s = &c->stats;

    size_t size = Px_ALIGN(n);

begin:
    h = c->h;
    if (size < h->remaining) {
        h->allocated += size;
        s->allocated += size;

        h->remaining -= size;
        s->remaining -= size;

        h->mallocs++;
        s->mallocs++;

        next = h->next;
        h->next = Px_PTRADD(h->next, size);
        assert(Px_PTRADD(h->base, h->allocated) == h->next);
        return next;
    } else {
        /* XXX TODO: merge this with Heap_Init(). */
        Heap *oldh;
        /* Try allocate another chunk. */
        size_t newsize = Px_CACHE_ALIGN(Px_MAX(size * 2, h->size));
        next = HeapAlloc(c->heap_handle, 0, newsize);
        if (!next)
            return Heap_LocalMalloc(c, size);

        /* Update our heap details to reflect the new chunk. */
        /* (Note that this code is almost identical to Heap_Init.) */
        oldh = c->h;
        h = oldh->sle_next;
        h->sle_prev = oldh;
        h->base = h->next = next;
        h->size  = newsize;
        s->size += newsize;
        h->allocated  = 0;
        h->remaining  = newsize;
        s->remaining += newsize; /* forgot why this is += */
        s->heaps++;
        c->h = h;
        h->sle_next = (Heap *)_PyHeap_Malloc(c, Px_CACHE_ALIGN(sizeof(Heap)));
        assert(h->sle_next);
    }

    goto begin;
}

void *
Heap_Malloc(size_t n)
{
    return _PyHeap_Malloc(ctx, n);
}

void *
_PyHeap_Realloc(Context *c, void *p, size_t n)
{
    void  *r;
    Heap  *h = c->h;
    Stats *s = &c->stats;
    r = _PyHeap_Malloc(c, n);
    if (!r)
        return NULL;
    h->reallocs++;
    s->reallocs++;
    memcpy(r, p, n);
    return r;
}

void *
Heap_Realloc(void *p, size_t n)
{
    return _PyHeap_Realloc(ctx, p, n);
}

void
_PyHeap_Free(Context *c, void *p)
{
    Heap  *h = c->h;
    Stats *s = &c->stats;

    h->frees++;
    s->frees++;
}

void
Heap_Free(void *p)
{
    _PyHeap_Free(ctx, p);
}


__inline
int
null_or_non_none_return_type(PyObject *op)
{
    if (!op)
        return 1;

    if (op == Py_None)
        return 0;

    Py_DECREF(op);
    PyErr_SetString(PyExc_ValueError, "non-None return value detected");
    return 1;
}

__inline
void
Px_INCCTX(Context *c)
{
    InterlockedIncrement(&(c->refcnt));
}

__inline
void
Px_DECCTX(Context *c)
{
    InterlockedDecrement(&(c->refcnt));
}

void *
_PyParallel_CreatedNewThreadState(PyThreadState *tstate)
{
    PxState *px;

    px = (PxState *)malloc(sizeof(PxState));
    if (!px)
        return PyErr_NoMemory();

    memset((void *)px, 0, sizeof(PxState));

    px->errors = PxList_New();
    if (!px->errors)
        goto free_px;

    px->completed_callbacks = PxList_New();
    if (!px->completed_callbacks)
        goto free_errors;

    px->completed_errbacks = PxList_New();
    if (!px->completed_errbacks)
        goto free_completed_callbacks;

    px->incoming = PxList_New();
    if (!px->incoming)
        goto free_completed_errbacks;

    px->finished = PxList_New();
    if (!px->finished)
        goto free_incoming;

    px->wakeup = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!px->wakeup)
        goto free_finished;

    tstate->px = px;

    tstate->is_parallel_thread = 0;

    goto done;

free_finished:
    PxList_FreeListHead(px->finished);

free_incoming:
    PxList_FreeListHead(px->incoming);

free_completed_errbacks:
    PxList_FreeListHead(px->completed_errbacks);

free_completed_callbacks:
    PxList_FreeListHead(px->completed_callbacks);

free_errors:
    PxList_FreeListHead(px->errors);

free_px:
    free(px);
    px = NULL;

done:
    if (!px)
        PyErr_SetFromWindowsErr(0);

    return px;
}

typedef struct _work {
    PyObject *func;
    PyObject *args;
    PyObject *kwds;
    PyObject *callback;
    PyObject *errback;
} work;

void
_PyParallel_HandleErrors(void)
{
    PxState *px;
    PxListItem *error;
    if (!pstate->curexc_type)
        return;

    px = (PxState *)pstate->px;
    error = ctx->error;
    PxList_TimestampItem(error);
    error->from = pstate;
    error->p1 = pstate->curexc_type;
    error->p2 = pstate->curexc_value;
    error->p3 = pstate->curexc_traceback;
    PxList_Push(px->errors, error);
}

/*
int
_PyParallel_EnterParallelContext(void *context)
{
    Callback *cb = (Callback *)context;
    Stats    *s;

    assert(
        (_tbuf_base == NULL && _tbuf_next == NULL) ||
        (_tbuf_base && _tbuf_next)
    );

    if (_tbuf_base == NULL) {
        * We're a newly-created thread. *
        _tbuf_base = _tbuf[0];
        _tbuf_next = _tbuf[0];
    }

    pstate = &_PxThreadState;
    memset((void *)pstate, 0, sizeof(PyThreadState));

    pstate->is_parallel_thread = 1;
    pstate->interp = cb->tstate->interp;
    pstate->thread_id = _Py_get_current_thread_id();

    memset((void *)&ctx, 0, sizeof(Context));
    ctx->cb = cb;

    if (!Heap_Init(&ctx->heap, 0))
        return 0;

    return 1;
}



void
_PyParallel_LeaveParallelContext(void)
{
    Stats    *s;

    if (ctx->heap_handle)
        HeapDestroy(ctx->heap_handle);

    _PyParallel_HandleErrors();
}
*/

void
NTAPI
_PyParallel_SimpleWorkCallback(PTP_CALLBACK_INSTANCE instance, void *context)
{
    Context  *c = (Context *)context;
    Stats    *s;
    PxState  *px;
    PyObject *r;
    PyObject *args;

    ctx = c;
    c->instance = instance;
    pstate = &_PxThreadState;
    memset((void *)pstate, 0, sizeof(PyThreadState));

    pstate->is_parallel_thread = 1;
    pstate->interp = c->tstate->interp;
    pstate->thread_id = _Py_get_current_thread_id();
    px = (PxState *)c->tstate->px;

    s = &(c->stats);
    InterlockedDecrement(&px->pending);
    InterlockedIncrement(&px->inflight);
    s->start = _Py_rdtsc();
    c->result = PyObject_Call(c->func, c->args, c->kwds);
    s->end = _Py_rdtsc();
    InterlockedIncrement64(&px->done);
    InterlockedDecrement(&px->inflight);

    if (!c->result) {
        if (c->errback) {
            PyObject *exc;
            exc = PyTuple_Pack(3, pstate->curexc_type,
                                  pstate->curexc_value,
                                  pstate->curexc_traceback);
            if (!exc)
                goto error;

            args = Py_BuildValue("(O)", exc);
            r = PyObject_CallObject(c->errback, args);
            if (!null_or_non_none_return_type(r)) {
                c->errback_completed->from = c;
                PxList_TimestampItem(c->errback_completed);
                PxList_Push(px->completed_errbacks, c->errback_completed);
                SetEvent(px->wakeup);
                return;
            }
        }
        goto error;
    } else {
        if (c->callback) {
            args = Py_BuildValue("(O)", c->result);
            r = PyObject_CallObject(c->callback, args);
            if (null_or_non_none_return_type(r))
                goto error;
        }
        c->callback_completed->from = c;
        PxList_TimestampItem(c->callback_completed);
        PxList_Push(px->completed_callbacks, c->callback_completed);
        SetEvent(px->wakeup);
        return;
    }

    /* unreachable */
    assert(0);

error:
    assert(pstate->curexc_type != NULL);
    PxList_TimestampItem(c->error);
    c->error->from = c;
    c->error->p1 = pstate->curexc_type;
    c->error->p2 = pstate->curexc_value;
    c->error->p3 = pstate->curexc_traceback;
    PxList_Push(px->errors, c->error);
    SetEvent(px->wakeup);
    return;
}

void
_PyParallel_Init(void)
{
    _Py_sfence();

    if (Py_MainProcessId == -1) {
        if (Py_MainThreadId != -1)
            Py_FatalError("_PyParallel_Init: invariant failed: "  \
                          "Py_MainThreadId should also be -1 if " \
                          "Py_MainProcessId is -1.");
    }
    if (Py_MainThreadId == -1) {
        if (Py_MainProcessId != -1)
            Py_FatalError("_PyParallel_Init: invariant failed: "   \
                          "Py_MainProcessId should also be -1 if " \
                          "Py_MainThreadId is -1.");
    }

    if (Py_MainProcessId == -1) {
        Py_MainProcessId = GetCurrentProcessId();
        if (Py_MainProcessId != _Py_get_current_process_id())
            Py_FatalError("_PyParallel_Init: intrinsics failure: " \
                          "_Py_get_current_process_id() != "       \
                          "GetCurrentProcessId()");
    }

    if (Py_MainThreadId == -1) {
        Py_MainThreadId = GetCurrentThreadId();
        if (Py_MainThreadId != _Py_get_current_thread_id())
            Py_FatalError("_PyParallel_Init: intrinsics failure: " \
                          "_Py_get_current_thread_id() != "        \
                          "GetCurrentThreadId()");
    }

    Py_ParallelContextsEnabled = 0;
    _Py_lfence();
    _Py_clflush(&Py_MainThreadId);
}

void
_PyParallel_ClearMainThreadId(void)
{
    _Py_sfence();
    Py_MainThreadId = 0;
    _Py_lfence();
    _Py_clflush(&Py_MainThreadId);
}

void
_PyParallel_CreatedGIL(void)
{
    _PyParallel_ClearMainThreadId();
}

void
_PyParallel_AboutToDropGIL(void)
{
    _PyParallel_ClearMainThreadId();
}

void
_PyParallel_DestroyedGIL(void)
{
    _PyParallel_ClearMainThreadId();
}

void
_PyParallel_JustAcquiredGIL(void)
{
    char buf[128], *fmt;

    _Py_lfence();
    if (Py_MainThreadId != 0) {
        fmt = "_PyParallel_JustAcquiredGIL: invariant failed: "   \
              "expected Py_MainThreadId to have value 0, actual " \
              "value: %d";
        (void)snprintf(buf, sizeof(buf), fmt, Py_MainThreadId);
        Py_FatalError(buf);
    }

    if (Py_MainProcessId == -1)
        Py_FatalError("_PyParallel_JustAcquiredGIL: Py_MainProcessId == -1");

    _Py_sfence();
    Py_MainThreadId = _Py_get_current_thread_id();
    _Py_lfence();
    _Py_clflush(&Py_MainThreadId);
}

void
_PyParallel_SetMainProcessId(long id)
{
    _Py_sfence();
    Py_MainProcessId = id;
    _Py_lfence();
    _Py_clflush(&Py_MainThreadId);
}

void
_PyParallel_ClearMainProcessId(void)
{
    _PyParallel_SetMainProcessId(0);
}

void
_PyParallel_RestoreMainProcessId(void)
{
    _PyParallel_SetMainProcessId(_Py_get_current_process_id());
}

void
_PyParallel_EnableParallelContexts(void)
{
    _Py_sfence();
    Py_ParallelContextsEnabled = 1;
    _Py_lfence();
    _Py_clflush(&Py_MainThreadId);
}

void
_PyParallel_DisableParallelContexts(void)
{
    _Py_sfence();
    Py_ParallelContextsEnabled = 0;
    _Py_lfence();
    _Py_clflush(&Py_MainThreadId);
}

void
_PyParallel_ContextGuardFailure(const char *function,
                                const char *filename,
                                int lineno,
                                int was_px_ctx)
{
    int err;
    char buf[128], *fmt;
    memset((void *)buf, 0, sizeof(buf));

    if (was_px_ctx)
        fmt = "%s called outside of parallel context (%s:%d)";
    else
        fmt = "%s called from within parallel context (%s:%d)";

    err = snprintf(buf, sizeof(buf), fmt, function, filename, lineno);
    if (err == -1)
        Py_FatalError("_PyParallel_ContextGuardFailure: snprintf failed");
    else
        Py_FatalError(buf);
}

void
_PyParallel_NewThreadState(PyThreadState *tstate)
{
    return;
}

/* mod _parallel */
static
PyObject *
_parallel_map(PyObject *self, PyObject *args)
{
    return NULL;;
}


PyDoc_STRVAR(_parallel_doc,
"_parallel module.\n\
\n\
Functions:\n\
\n\
map()\n");

PyDoc_STRVAR(_parallel_map_doc,
"map(callable, iterable) -> list\n\
\n\
Calls ``callable`` with each item in ``iterable``.\n\
Returns a list of results.");

#define _METHOD(m, n, a) {#n, (PyCFunction)##m##_##n##, a, m##_##n##_doc }
#define _PARALLEL(n, a) _METHOD(_parallel, n, a)
#define _PARALLEL_N(n) _METHOD(_parallel, n, METH_NOARGS)
#define _PARALLEL_O(n) _METHOD(_parallel, n, METH_O)
#define _PARALLEL_V(n) _METHOD(_parallel, n, METH_VARARGS)
static PyMethodDef _parallel_methods[] = {
    _PARALLEL_V(map),

    { NULL, NULL } /* sentinel */
};

static struct PyModuleDef _parallelmodule = {
    PyModuleDef_HEAD_INIT,
    "_parallel",
    _parallel_doc,
    -1, /* multiple "initialization" just copies the module dict. */
    _parallel_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyObject *
_PyParallel_ModInit(void)
{
    PyObject *m;

    m = PyModule_Create(&_parallelmodule);
    if (m == NULL)
        return NULL;

    return m;
}

/* mod _async */
PyObject *
_async_run(PyObject *self, PyObject *args)
{
    if (args) {
        PyErr_SetString(PyExc_AsyncError, "run() called with args");
        return NULL;
    }

    PyErr_SetNone(PyExc_AsyncRunCalledWithoutEventsError);

    return NULL;
}

__inline
int
free_context(Context *c)
{
    Px_DECCTX(c);
    assert(c->refcnt >= 0);
    if (c->refcnt > 0)
        return 0;

    PxList_FreeListItem(c->error);
    PxList_FreeListItem(c->errback_completed);
    PxList_FreeListItem(c->callback_completed);

    /* ignore errors from this for now */
    HeapDestroy(c->heap_handle);
    free(c);
    c = NULL;
    return 1;
}


PyObject *
_async_run_once(PyObject *self, PyObject *args)
{
    int err = 0;
    int wait = -1;
    unsigned short depth_hint = 0;
    unsigned int waited = 0;
    unsigned int depth = 0;
    unsigned int events = 0;
    unsigned int errors = 0;
    unsigned int completed_errbacks = 0;
    unsigned int completed_callbacks = 0;
    PyObject *result = NULL;
    Context *c;
    PxState *px;
    PxListItem *item = NULL;
    PyThreadState *tstate = PyThreadState_GET();

    /*
    if (!PyArg_ParseTuple(args, "|i", &wait))
        return NULL;
    */

    px = (PxState *)tstate->px;

    if (px->submitted == 0) {
        PyErr_SetNone(PyExc_AsyncRunCalledWithoutEventsError);
        return NULL;
    }

    item = PxList_Flush(px->finished);
    if (item) {
        do {
            free_context((Context *)item->from);
        } while (item = PxList_FreeListItemAfterNext(item));
    }

start:
    errors = 0;
    item = PxList_FlushWithDepthHint(px->errors, &depth_hint);
    if (item) {
        size_t depth = 0;
        size_t newdepth = 0;
        PyObject *e;
        PyObject *t;
        PxListItem *next;

        assert(depth_hint != 0);

        t = PyTuple_New((Py_ssize_t)depth_hint);
        if (!t) {
            PxList_Push(px->errors, item);
            return PyErr_NoMemory();
        }

        do {
            depth++;
            if ((depth_hint && depth > depth_hint)) {
                newdepth = depth + PxList_CountItems(item);
                if (!_PyTuple_Resize(&t, newdepth)) {
                    PxList_Push(px->errors, item);
                    return PyErr_NoMemory();
                }
                depth_hint = 0;
            }
            e = PyTuple_New(3);
            if (!e) {
                PxList_Push(px->errors, item);
                return PyErr_NoMemory();
            }

            PyTuple_SET_ITEM(e, 0, (PyObject *)item->p1);
            PyTuple_SET_ITEM(e, 1, (PyObject *)item->p2);
            PyTuple_SET_ITEM(e, 2, (PyObject *)item->p3);

            PyTuple_SET_ITEM(t, depth-1, e);

            item = PxList_Transfer(px->finished, item);
        } while (item);

        return t;
    }

    /* Process 'incoming' work items. */
    item = PxList_Flush(px->incoming);
    if (item) {
        PxListHead *transfer;

        do {
            HANDLE wait;
            PyObject *func, *args, *kwds, *result;

            func = (PyObject *)item->p1;
            args = (PyObject *)item->p2;
            kwds = (PyObject *)item->p3;
            wait = (HANDLE)item->p4;

            if (!args) {
                if (kwds)
                    args = PyTuple_New(0);
                else
                    args = Py_None;
            } else {
                if (!PyTuple_Check(args))
                    args = Py_BuildValue("(O)", args);
            }

            Py_INCREF(func);
            Py_INCREF(args);
            Py_XINCREF(kwds);

            if (kwds)
                result = PyObject_Call(func, args, kwds);
            else
                result = PyObject_CallObject(func, args);

            if (wait) {
                if (null_or_non_none_return_type(result))
                    PyErr_Fetch((PyObject **)&(item->p1),
                                (PyObject **)&(item->p2),
                                (PyObject **)&(item->p3));
                else {
                    item->p1 = NULL;
                    item->p2 = result;
                    item->p3 = NULL;
                }
                SetEvent((HANDLE)item->p4);
                item = PxList_Next(item);
            } else {
                if (null_or_non_none_return_type(result)) {
                    assert (tstate->curexc_type != NULL);
                    item->p1 = tstate->curexc_type;
                    item->p2 = tstate->curexc_value;
                    item->p3 = tstate->curexc_traceback;
                    transfer = px->errors;
                    errors++;
                } else {
                    assert(result == Py_None);
                    transfer = px->finished;
                }
                item = PxList_Transfer(transfer, item);
            }
        } while (item);
    }
    if (errors)
        goto start;

    /* Process completed items. */
    item = PxList_Flush(px->completed_callbacks);
    if (item) {
        do {
            /* XXX TODO: update stats. */
            completed_callbacks++;
            item = PxList_Transfer(px->finished, item);
        } while (item);
    }

    item = PxList_Flush(px->completed_errbacks);
    if (item) {
        do {
            /* XXX TODO: update stats. */
            completed_errbacks++;
            item = PxList_Transfer(px->finished, item);
        } while (item);
    }

    if (completed_callbacks || completed_errbacks)
        Py_RETURN_NONE;

    err = WaitForSingleObject(px->wakeup, 1000*1000);
    if (err == WAIT_OBJECT_0)
        goto start;

    switch (err) {
        case WAIT_ABANDONED:
            PyErr_SetFromWindowsErr(0);
        case WAIT_TIMEOUT:
            Py_RETURN_NONE;
        case WAIT_FAILED:
            PyErr_SetFromWindowsErr(0);
    }
    return NULL;
}

PyObject *
_async_map(PyObject *self, PyObject *args)
{
    PyObject *result;

    return NULL;
}

__inline
int
extract_args(PyObject *args, Context *c)
{
    if (!PyArg_UnpackTuple(
            args, "", 1, 5,
            &(c->func), &(c->args), &(c->kwds),
            &(c->callback), &(c->errback)))
        return 0;

    if (c->callback == Py_None) {
        Py_DECREF(c->callback);
        c->callback = NULL;
    }

    if (c->errback == Py_None) {
        Py_DECREF(c->errback);
        c->errback = NULL;
    }

    if (!PyTuple_Check(c->args)) {
        PyObject *tmp = c->args;
        c->args = Py_BuildValue("(O)", c->args);
        Py_DECREF(tmp);
    }

    return 1;
}

__inline
void
incref_args(Context *c)
{
    Py_INCREF(c->func);
    Py_XINCREF(c->args);
    Py_XINCREF(c->kwds);
    Py_XINCREF(c->callback);
    Py_XINCREF(c->errback);
}

__inline
void
decref_args(Context *c)
{
    Py_DECREF(c->func);
    Py_XDECREF(c->args);
    Py_XDECREF(c->kwds);
    Py_XDECREF(c->callback);
    Py_XDECREF(c->errback);
}

__inline
int
submit_work(Context *c)
{
    int retval;
    PTP_SIMPLE_CALLBACK cb;

    cb = _PyParallel_SimpleWorkCallback;
    retval = TrySubmitThreadpoolCallback(cb, c, NULL);
    if (!retval)
        PyErr_SetFromWindowsErr(0);
    return retval;
}

PyObject *
_async_submit_work(PyObject *self, PyObject *args)
{
    PyCodeObject *code;
    PyObject *result = NULL;
    Context  *c = (Context *)malloc(sizeof(Context));

    if (!c)
        return PyErr_NoMemory();

    memset((void *)c, 0, sizeof(Context));

    if (!extract_args(args, c))
        goto free_context;

    c->heap_handle = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
    if (!c->heap_handle) {
        PyErr_SetFromWindowsErr(0);
        goto free_context;
    }

    if (!_PyHeap_Init(c, 0))
        goto free_heap;

    c->error = PxList_NewItem();
    if (!c->error)
        goto free_heap;

    c->callback_completed = PxList_NewItem();
    if (!c->callback_completed)
        goto free_error;

    c->errback_completed = PxList_NewItem();
    if (!c->errback_completed)
        goto free_callback_completed;

    c->outgoing = PxList_New();
    if (!c->outgoing)
        goto free_errback_completed;

    c->tbuf_next = c->tbuf_base = &c->tbuf[0];
    c->tbuf_remaining = _PX_TMPBUF_SIZE;

    c->tstate = PyThreadState_GET();
    assert(c->tstate);
    c->px = (PxState *)c->tstate->px;
    InterlockedIncrement64(&(c->px->submitted));
    InterlockedIncrement(&(c->px->pending));

    incref_args(c);

    c->refcnt = 1;

    if (!submit_work(c))
        goto decref_args;

    result = (Py_INCREF(Py_None), Py_None);
    goto done;

decref_args:
    decref_args(c);

free_outgoing:
    PxList_FreeList(c->outgoing);

free_errback_completed:
    PxList_FreeListItem(c->errback_completed);

free_callback_completed:
    PxList_FreeListItem(c->callback_completed);

free_error:
    PxList_FreeListItem(c->error);

free_heap:
    HeapDestroy(c->heap_handle);

free_context:
    free(c);

done:
    return (result ? result : PyErr_NoMemory());
}

PyObject *
_async_submit_wait(PyObject *self, PyObject *args)
{
    PyObject *result;

    return NULL;
}

PyObject *
_async_submit_timer(PyObject *self, PyObject *args)
{
    PyObject *result;

    return NULL;
}

PyObject *
_async_submit_io(PyObject *self, PyObject *args)
{
    PyObject *result;

    return NULL;
}

PyObject *
_async_submit_server(PyObject *self, PyObject *args)
{
    PyObject *result;

    return NULL;
}

PyObject *
_async_submit_client(PyObject *self, PyObject *args)
{
    PyObject *result;

    return NULL;
}

PyObject *
_async_submit_class(PyObject *self, PyObject *args)
{
    PyObject *result;

    return NULL;
}

PyObject *
_call_from_main_thread(PyObject *self, PyObject *args, int wait)
{
    int rv;
    int err;
    Context *c;
    PyObject *result = NULL;
    PxListItem *item;
    PxState *px;
    Px_GUARD

    c = ctx;
    item = PxList_NewItem();
    if (!item)
        return PyErr_NoMemory();

    if (!PyArg_UnpackTuple(args, "", 1, 3, &(item->p1),
                           &(item->p2), &(item->p3)))
    {
        PxList_FreeListItem(item);
        return NULL;
    }

    if (wait) {
        item->p4 = (void *)CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!item->p4) {
            PxList_FreeListItem(item);
            return NULL;
        }
    }

    item->from = c;
    px = c->px;
    PxList_TimestampItem(item);
    if (!wait) {
        Px_INCCTX(c);
        PxList_Push(c->outgoing, item);
    }
    PxList_Push(px->incoming, item);
    if (!wait)
        return Py_None;

    err = WaitForSingleObject(item->p4, INFINITE);
    switch (err) {
        case WAIT_ABANDONED:
            Py_FatalError("WaitForSingleObject->WAIT_ABANDONED");
            goto cleanup;
        case WAIT_TIMEOUT:
            Py_FatalError("WaitForSingleObject->INFINITE timeout");
            goto cleanup;
        case WAIT_FAILED:
            PyErr_SetFromWindowsErr(0);
            goto cleanup;
    }
    assert(err == WAIT_OBJECT_0);

    if (item->p1 && PyExceptionClass_Check((PyObject *)item->p1)) {
        PyErr_Restore((PyObject *)item->p1,
                      (PyObject *)item->p2,
                      (PyObject *)item->p3);
        goto cleanup;
    }

    assert(item->p1 == NULL);
    assert(item->p2 != NULL);
    assert(item->p3 == NULL);
    result = (PyObject *)item->p2;

cleanup:
    CloseHandle(item->p4);
    item->p4 = NULL;
    PxList_FreeListItem(item);
    Px_DECCTX(c);

    return result;
}

PyObject *
_async_call_from_main_thread(PyObject *self, PyObject *args)
{
    return _call_from_main_thread(self, args, 0);
}

PyObject *
_async_call_from_main_thread_and_wait(PyObject *self, PyObject *args)
{
    return _call_from_main_thread(self, args, 1);
}

PyDoc_STRVAR(_async_doc,
"_async module.\n\
\n\
Functions:\n\
\n\
run()\n\
map(callable, iterable[, chunksize[, callback[, errback]]])\n\
submit_work(func[, args[, kwds[, callback[, errback]]]])\n\
submit_wait(wait, func[, args[, kwds[, callback[, errback]]]])\n\
submit_timer(timer, func[, args[, kwds[, callback[, errback]]]])\n\
submit_io(func[, args[, kwds[, callback[, errback]]]])\n\
submit_server(obj)\n\
submit_client(obj)\n\
");

PyDoc_STRVAR(_async_run_doc,
"run() -> None\n\
\n\
Runs the _async event loop.");


PyDoc_STRVAR(_async_register_doc,
"register(object) -> None\n\
\n\
Register an asynchronous object.");

PyDoc_STRVAR(_async_unregister_doc,
"unregister(object) -> None\n\
\n\
Unregisters an asynchronous object.");

PyDoc_STRVAR(_async_map_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_run_once_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_submit_io_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_submit_work_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_submit_wait_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_submit_timer_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_submit_class_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_submit_client_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_submit_server_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_call_from_main_thread_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_call_from_main_thread_and_wait_doc, "XXX TODO\n");

#define _ASYNC(n, a) _METHOD(_async, n, a)
#define _ASYNC_N(n) _METHOD(_async, n, METH_NOARGS)
#define _ASYNC_O(n) _METHOD(_async, n, METH_O)
#define _ASYNC_V(n) _METHOD(_async, n, METH_VARARGS)
PyMethodDef _async_methods[] = {
    _ASYNC_V(map),
    _ASYNC_N(run),
    _ASYNC_N(run_once),
    _ASYNC_V(submit_io),
    _ASYNC_V(submit_work),
    _ASYNC_V(submit_wait),
    _ASYNC_V(submit_timer),
    _ASYNC_O(submit_class),
    _ASYNC_O(submit_client),
    _ASYNC_O(submit_server),
    _ASYNC_V(call_from_main_thread),
    _ASYNC_V(call_from_main_thread_and_wait),

    { NULL, NULL } /* sentinel */
};

struct PyModuleDef _asyncmodule = {
    PyModuleDef_HEAD_INIT,
    "_async",
    _async_doc,
    -1, /* multiple "initialization" just copies the module dict. */
    _async_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyObject *
_PyAsync_ModInit(void)
{
    PyObject *m, *d;

    m = PyModule_Create(&_asyncmodule);
    if (m == NULL)
        return NULL;

    return m;
}


/* Objects (PyObjects) */
__inline
PyObject *
Object_Init(PyObject *op, PyTypeObject *tp)
{
    Stats  *s;
    Object *o;

/* Make sure we're not called for PyVarObjects... */
#ifdef Py_DEBUG
    assert(tp->tp_itemsize == 0);
#endif

    s = &ctx->stats;
    o = (Object *)Heap_Malloc(sizeof(Object));

    Py_TYPE(op) = tp;
    Py_REFCNT(op) = 1;
    Py_PX(op) = ctx;

    o->op = op;
    append_object(&ctx->objects, o);
    s->objects++;

    return op;
}

__inline
PyObject *
Object_New(PyTypeObject *tp)
{
    return Object_Init((PyObject *)Heap_Malloc(_PyObject_SIZE(tp)), tp);
}

/* VarObjects (PyVarObjects) */
__inline
PyVarObject *
VarObject_Init(PyVarObject *op, PyTypeObject *tp, Py_ssize_t size)
{
    Stats  *s;
    Object *o;

/* Make sure we're not called for PyObjects... */
#ifdef Py_DEBUG
    assert(tp->tp_itemsize > 0);
#endif

    s = &ctx->stats;
    o = (Object *)Heap_Malloc(sizeof(Object));

    Py_SIZE(op) = size;
    Py_TYPE(op) = tp;
    Py_REFCNT(op) = 1;
    Py_PX(op) = ctx;
    o->op = (PyObject *)op;
    append_object(&ctx->varobjs, o);
    s->varobjs++;

    return op;
}

__inline
PyVarObject *
VarObject_New(PyTypeObject *tp, Py_ssize_t nitems)
{
    register const size_t sz = _PyObject_VAR_SIZE(tp, nitems);
    register PyVarObject *v = (PyVarObject *)Heap_Malloc(sz);
    return VarObject_Init(v, tp, nitems);
}

__inline
PyVarObject *
VarObject_Resize(PyVarObject *op, Py_ssize_t n)
{
    register const int was_resize = 1;
    register const size_t sz = _PyObject_VAR_SIZE(Py_TYPE(op), n);
    PyVarObject *r = (PyVarObject *)Heap_Malloc(sz);
    ctx->h->resizes++;
    ctx->stats.resizes++;
    return r;
}

/* And now for the exported symbols... */
void
_Px_NewReference(PyObject *op)
{
    op->ob_refcnt = 1;
    op->px_ctx = ctx;
    ctx->stats.newrefs++;
}

void
_Px_ForgetReference(PyObject *op)
{
    ctx->stats.forgetrefs++;
}

void
_Px_Dealloc(PyObject *op)
{
    ctx->h->deallocs++;
    ctx->stats.deallocs++;
}

PyObject *
_PxObject_New(PyTypeObject *tp)
{
    return Object_New(tp);
}

PyVarObject *
_PxObject_NewVar(PyTypeObject *tp, Py_ssize_t nitems)
{
    return VarObject_New(tp, nitems);
}

PyObject *
_PxObject_Init(PyObject *op, PyTypeObject *tp)
{
    return Object_Init(op, tp);
}

PyVarObject *
_PxObject_InitVar(PyVarObject *op, PyTypeObject *tp, Py_ssize_t nitems)
{
    return VarObject_Init(op, tp, nitems);
}

PyVarObject *
_PxObject_Resize(PyVarObject *op, Py_ssize_t nitems)
{
    return VarObject_Resize(op, nitems);
}

void *
_PxMem_Malloc(size_t n)
{
    return Heap_Malloc(n);
}

void *
_PxMem_Realloc(void *p, size_t n)
{
    return Heap_Realloc(p, n);
}

void
_PxMem_Free(void *p)
{
    Heap_Free(p);
}


#ifdef __cpplus
}
#endif

















































