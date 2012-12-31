#include "Python.h"

#ifdef __cpplus
extern "C" {
#endif

#include "pyparallel_private.h"

__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
__declspec(thread) PyParallelContext *ctx = NULL;
#define _TMPBUF_SIZE 1024

__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
long Py_MainThreadId  = -1;
long Py_MainProcessId = -1;
long Py_ParallelContextsEnabled = -1;

void *Heap_Malloc(size_t);
void *_PyHeap_Malloc(Context *c, size_t n, size_t align);

__inline
PyThreadState *
get_main_thread_state(void)
{
    return (PyThreadState *)_Py_atomic_load_relaxed(&_PyThreadState_Current);
}

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

#define Px_SIZEOF_HEAP        Px_CACHE_ALIGN(sizeof(Heap))
#define Px_USEABLE_HEAP_SIZE (Px_PAGE_ALIGN_SIZE - Px_SIZEOF_HEAP)
#define Px_NEW_HEAP_SIZE(n)  Px_PAGE_ALIGN((Py_MAX(n, Px_USEABLE_HEAP_SIZE)))

void *
Heap_Init(Context *c, size_t n)
{
    Heap  *h;
    Stats *s = &(c->stats);
    size_t size;
    int flags;

    if (n < Px_DEFAULT_HEAP_SIZE)
        size = Px_DEFAULT_HEAP_SIZE;
    else
        size = n;

    size = Px_PAGE_ALIGN(size);

    if (!c->h)
        /* First init. */
        h = &(c->heap);
    else {
        h = c->h->sle_next;
        h->sle_prev = c->h;
    }

    assert(h);

    h->size = size;
    flags = HEAP_ZERO_MEMORY;
    h->base = h->next = HeapAlloc(c->heap_handle, flags, h->size);
    if (!h->base)
        return PyErr_SetFromWindowsErr(0);
    h->remaining = size;
    s->remaining += size;
    s->size += size;
    s->heaps++;
    c->h = h;
    h->sle_next = (Heap *)_PyHeap_Malloc(c, sizeof(Heap), Px_SIZEOF_HEAP);
    assert(h->sle_next);
    return h;
}

__inline
void *
_PyHeap_Init(Context *c, Py_ssize_t n)
{
    return Heap_Init(c, n);
}

void *
Heap_LocalMalloc(Context *c, size_t n, size_t align)
{
    void *next;
    wchar_t *fmt;
    size_t alignment_diff;
    size_t alignment = align;
    size_t requested_size = n;
    size_t aligned_size;

    if (!alignment)
        alignment = Px_PTR_ALIGN_SIZE;

    if (alignment > c->tbuf_last_alignment)
        alignment_diff = Px_PTR_ALIGN(alignment - c->tbuf_last_alignment);
    else
        alignment_diff = 0;

    aligned_size = Px_ALIGN(n, alignment);

    if (aligned_size < (c->tbuf_remaining-alignment_diff)) {
        if (alignment_diff) {
            c->tbuf_remaining -= alignment_diff;
            c->tbuf_allocated += alignment_diff;
            c->tbuf_alignment_mismatches++;
            c->tbuf_bytes_wasted += alignment_diff;
            c->tbuf_next = Px_PTRADD(c->tbuf_next, alignment_diff);
            assert(Px_PTRADD(c->tbuf_base, c->tbuf_allocated) == c->tbuf_next);
        }

        c->tbuf_mallocs++;
        c->tbuf_allocated += aligned_size;
        c->tbuf_remaining -= aligned_size;

        c->tbuf_bytes_wasted += (aligned_size - requested_size);

        c->tbuf_last_alignment = alignment;

        next = c->tbuf_next;
        c->tbuf_next = Px_PTRADD(c->tbuf_next, aligned_size);
        assert(Px_PTRADD(c->tbuf_base, c->tbuf_allocated) == c->tbuf_next);

    } else {
        next = (void *)malloc(aligned_size);
        if (!next)
            return PyErr_NoMemory();

        memset(next, 0, aligned_size);

        c->leak_count++;
        c->leaked_bytes += aligned_size;
        c->last_leak = next;

        fmt = L"Heap_LocalMalloc: local buffer exhausted ("    \
              L"requested: %lld, available: %lld).  Resorted " \
              L"to malloc() -- note that memory will not be "  \
              L"freed!\n";
        fwprintf_s(stderr, fmt, aligned_size, c->tbuf_remaining);
    }

    return next;
}

void *
_PyHeap_Malloc(Context *c, size_t n, size_t align)
{
    void  *next;
    Heap  *h;
    Stats *s = &c->stats;
    size_t alignment_diff;
    size_t alignment = align;
    size_t requested_size = n;
    size_t aligned_size;

    if (!alignment)
        alignment = Px_PTR_ALIGN_SIZE;

begin:
    h = c->h;

    if (alignment > h->last_alignment)
        alignment_diff = Px_PTR_ALIGN(alignment - h->last_alignment);
    else
        alignment_diff = 0;

    aligned_size = Px_ALIGN(n, alignment);

    if (aligned_size < (h->remaining-alignment_diff)) {
        if (alignment_diff) {
            h->remaining -= alignment_diff;
            s->remaining -= alignment_diff;
            h->allocated += alignment_diff;
            s->allocated += alignment_diff;
            h->alignment_mismatches++;
            s->alignment_mismatches++;
            h->bytes_wasted += alignment_diff;
            s->bytes_wasted += alignment_diff;
            h->next = Px_PTRADD(h->next, alignment_diff);
            assert(Px_PTRADD(h->base, h->allocated) == h->next);
        }

        h->allocated += aligned_size;
        s->allocated += aligned_size;

        h->remaining -= aligned_size;
        s->remaining -= aligned_size;

        h->mallocs++;
        s->mallocs++;

        h->bytes_wasted += (aligned_size - requested_size);
        s->bytes_wasted += (aligned_size - requested_size);

        h->last_alignment = alignment;

        next = h->next;
        h->next = Px_PTRADD(h->next, aligned_size);
        assert(Px_PTRADD(h->base, h->allocated) == h->next);
        return next;
    }

    /* Force a resize. */
    if (!_PyHeap_Init(c, Px_NEW_HEAP_SIZE(aligned_size)))
        return Heap_LocalMalloc(c, aligned_size, alignment);

    goto begin;
}

__inline
void *
_PyHeap_MemAlignedMalloc(Context *c, size_t n)
{
    return _PyHeap_Malloc(c, n, Px_MEM_ALIGN_SIZE);
}

void *
Heap_Malloc(size_t n)
{
    return _PyHeap_Malloc(ctx, n, 0);
}

void *
_PyHeap_Realloc(Context *c, void *p, size_t n)
{
    void  *r;
    Heap  *h = c->h;
    Stats *s = &c->stats;
    r = _PyHeap_Malloc(c, n, 0);
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

__inline
void
_PyHeap_FastFree(Heap *h, Stats *s, void *p)
{
    h->frees++;
    s->frees++;
}

void
Heap_Free(void *p)
{
    _PyHeap_Free(ctx, p);
}

__inline
PxListHead *
_PyHeap_NewList(Context *c)
{
    PxListHead *l;

    l = (PxListHead *)_PyHeap_MemAlignedMalloc(c, sizeof(PxListHead));
    if (l)
        InitializeSListHead(l);

    return l;
}

__inline
PxListItem *
_PyHeap_NewListItem(Context *c)
{
    return (PxListItem *)_PyHeap_MemAlignedMalloc(c, sizeof(PxListItem));
}

__inline
PyObject *
Object_Init(PyObject *op, PyTypeObject *tp, Context *c)
{
    Stats  *s;
    Object *o;

/* Make sure we're not called for PyVarObjects... */
#ifdef Py_DEBUG
    assert(tp->tp_itemsize == 0);
#endif

    s = &c->stats;
    o = (Object *)_PyHeap_Malloc(c, sizeof(Object), 0);

    Py_TYPE(op) = tp;
    Py_REFCNT(op) = 1;
    Py_PX(op) = c;

    o->op = op;
    append_object(&c->objects, o);
    s->objects++;

    return op;
}

__inline
PyObject *
Object_New(PyTypeObject *tp, Context *c)
{
    return Object_Init((PyObject *)Heap_Malloc(_PyObject_SIZE(tp)), tp, c);
}

/* VarObjects (PyVarObjects) */
__inline
PyVarObject *
VarObject_Init(PyVarObject *op, PyTypeObject *tp, Py_ssize_t size, Context *c)
{
    Stats  *s;
    Object *o;

/* Make sure we're not called for PyObjects... */
#ifdef Py_DEBUG
    assert(tp->tp_itemsize > 0);
#endif

    s = &c->stats;
    o = (Object *)_PyHeap_Malloc(c, sizeof(Object), 0);

    Py_SIZE(op) = size;
    Py_TYPE(op) = tp;
    Py_REFCNT(op) = 1;
    Py_PX(op) = c;
    o->op = (PyObject *)op;
    append_object(&c->varobjs, o);
    s->varobjs++;

    return op;
}

__inline
PyVarObject *
VarObject_New(PyTypeObject *tp, Py_ssize_t nitems, Context *c)
{
    register const size_t sz = _PyObject_VAR_SIZE(tp, nitems);
    register PyVarObject *v = (PyVarObject *)_PyHeap_Malloc(c, sz, 0);
    return VarObject_Init(v, tp, nitems, c);
}

__inline
PyVarObject *
VarObject_Resize(PyObject *op, Py_ssize_t n, Context *c)
{
    register const int was_resize = 1;
    register const size_t sz = _PyObject_VAR_SIZE(Py_TYPE(op), n);
    PyVarObject *r = (PyVarObject *)_PyHeap_Malloc(c, sz, 0);
    if (!r)
        return NULL;
    memcpy(r, op, n);
    c->h->resizes++;
    c->stats.resizes++;
    op = (PyObject *)r;
    return r;
}

__inline
PyObject *
_PyHeap_NewTuple(Context *c, Py_ssize_t nitems)
{
    return (PyObject *)VarObject_New(&PyTuple_Type, nitems, c);
}

__inline
PyObject *
_PyHeap_ResizeTuple(Context *c, PyObject *op, Py_ssize_t nitems)
{
    return (PyObject *)VarObject_Resize(op, nitems, c);
}

__inline
int
null_with_exc_or_non_none_return_type(PyObject *op)
{
    PyThreadState *pstate;
    Px_GUARD

    pstate = ctx->pstate;

    if (!op && pstate->curexc_type)
        return 1;

    assert(!pstate->curexc_type);

    if ((!op && !pstate->curexc_type) || op == Py_None)
        return 0;

    Py_DECREF(op);
    PyErr_SetString(PyExc_ValueError, "non-None return value detected");
    return 1;
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

void
_PxState_ReleaseContext(PxState *px, Context *c)
{
    register Context *last;
    assert(c->refcnt == 0);
    assert(c->ttl >= 1 && c->ttl <= 4);
    assert(c->next == NULL);
    assert(c->prev == NULL);
    if (!px->ctx_first) {
        assert(!px->ctx_last);
        px->ctx_first = c;
        px->ctx_last = c;
        c->next = NULL;
    } else {
        assert(!px->ctx_first->prev);
        assert(!px->ctx_last->next);
        last = px->ctx_last;
        last->next = c;
        c->prev = last;
        c->next = NULL;
        px->ctx_last = c;
    }
}

__inline
long
Px_DECCTX(Context *c)
{
    register PxState *px = c->px;
    InterlockedDecrement(&(c->refcnt));
    assert(c->refcnt >= 0);

    if (c->refcnt > 0)
        return c->refcnt;

    assert(c->refcnt == 0);
    _PxState_ReleaseContext(px, c);
    return 0;
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

    /*
    px->freelist = PxList_New();
    if (!px->freelist)
        goto free_finished;

    px->singles = PxList_New();
    if (!px->singles)
        goto free_freelist;
    */

    px->wakeup = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!px->wakeup)
        goto free_finished;

    InitializeCriticalSectionAndSpinCount(&(px->cs), 12);

    tstate->px = px;

    tstate->is_parallel_thread = 0;
    px->ctx_ttl = 1;

    goto done;

    /*
free_singles:
    PxList_FreeListHead(px->singles);

free_freelist:
    PxList_FreeListHead(px->freelist);
    */

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

void
NTAPI
_PyParallel_SimpleWorkCallback(PTP_CALLBACK_INSTANCE instance, void *context)
{
    Context  *c = (Context *)context;
    Stats    *s;
    PxState  *px;
    PyObject *r;
    PyObject *args;
    PyThreadState *pstate;

    assert(c->tstate);
    assert(c->heap_handle);

    px = (PxState *)c->tstate->px;
    //InterlockedIncrement(&(px->active));
    InterlockedDecrement(&(px->pending));
    InterlockedIncrement(&(px->inflight));

    ctx = c;

    c->error = _PyHeap_NewListItem(c);
    c->errback_completed = _PyHeap_NewListItem(c);
    c->callback_completed = _PyHeap_NewListItem(c);

    c->outgoing = _PyHeap_NewList(c);

    c->pstate = (PyThreadState *)_PyHeap_Malloc(c, sizeof(PyThreadState), 0);

    assert(
        c->error                &&
        c->pstate               &&
        c->outgoing             &&
        c->errback_completed    &&
        c->callback_completed
    );

    c->instance = instance;
    pstate = c->pstate;

    pstate->px = ctx;
    pstate->is_parallel_thread = 1;
    pstate->interp = c->tstate->interp;
    pstate->thread_id = _Py_get_current_thread_id();

    c->tbuf_next = c->tbuf_base = (void *)c->tbuf[0];
    c->tbuf_remaining = _PX_TMPBUF_SIZE;

    s = &(c->stats);
    s->startup_size = s->allocated;

    s->start = _Py_rdtsc();
    c->result = PyObject_Call(c->func, c->args, c->kwds);
    s->end = _Py_rdtsc();

    if (c->result) {
        assert(!pstate->curexc_type);
        if (c->callback) {
            args = Py_BuildValue("(O)", c->result);
            r = PyObject_CallObject(c->callback, args);
            if (null_with_exc_or_non_none_return_type(r))
                goto errback;
        }
        c->callback_completed->from = c;
        PxList_TimestampItem(c->callback_completed);
        InterlockedExchange(&(c->done), 1);
        InterlockedIncrement64(&(px->done));
        InterlockedDecrement(&(px->inflight));
        PxList_Push(px->completed_callbacks, c->callback_completed);
        SetEvent(px->wakeup);
        return;
    }

errback:
    if (c->errback) {
        PyObject *exc;
        assert(pstate->curexc_type);
        exc = PyTuple_Pack(3, pstate->curexc_type,
                              pstate->curexc_value,
                              pstate->curexc_traceback);
        if (!exc)
            goto error;

        PyErr_Clear();
        args = Py_BuildValue("(O)", exc);
        r = PyObject_CallObject(c->errback, args);
        if (!null_with_exc_or_non_none_return_type(r)) {
            c->errback_completed->from = c;
            PxList_TimestampItem(c->errback_completed);
            InterlockedExchange(&(c->done), 1);
            InterlockedIncrement64(&(px->done));
            InterlockedDecrement(&(px->inflight));
            PxList_Push(px->completed_errbacks, c->errback_completed);
            SetEvent(px->wakeup);
            return;
        }
    }

error:
    assert(pstate->curexc_type != NULL);
    PxList_TimestampItem(c->error);
    c->error->from = c;
    c->error->p1 = pstate->curexc_type;
    c->error->p2 = pstate->curexc_value;
    c->error->p3 = pstate->curexc_traceback;
    InterlockedExchange(&(c->done), 1);
    InterlockedIncrement64(&(px->done));
    InterlockedDecrement(&(px->inflight));
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
_is_active_ex(void)
{
    PyThreadState *tstate = get_main_thread_state();
    PxState *px = (PxState *)tstate->px;
    int rv;

    if (!TryEnterCriticalSection(&(px->cs)))
        return 1;

    rv = !(px->ctx_first == NULL &&
           px->done == px->last_done_count &&
           px->submitted == px->last_submitted_count &&
           px->pending == 0 &&
           px->inflight == 0 &&
           px->sync_wait_submitted == px->last_sync_wait_submitted_count &&
           px->sync_wait_pending == 0 &&
           px->sync_wait_inflight == 0 &&
           px->sync_wait_done == px->last_sync_wait_done_count &&
           px->sync_nowait_submitted == px->last_sync_nowait_submitted_count &&
           px->sync_nowait_pending == 0 &&
           px->sync_nowait_inflight == 0 &&
           px->sync_nowait_done == px->last_sync_nowait_done_count &&
           PxList_QueryDepth(px->errors)   == 0 &&
           PxList_QueryDepth(px->finished) == 0 &&
           PxList_QueryDepth(px->incoming) == 0 &&
           PxList_QueryDepth(px->completed_errbacks) == 0 &&
           PxList_QueryDepth(px->completed_callbacks) == 0);

    LeaveCriticalSection(&(px->cs));
    return rv;
}

__inline
PxState *
PXSTATE(void)
{
    return (PxState *)get_main_thread_state()->px;
}

__inline
long
_is_active(void)
{
    return PXSTATE()->active;
}

PyObject *
_async_is_active(PyObject *self, PyObject *args)
{
    PyObject *r = (PyObject *)(_is_active() ? Py_True : Py_False);
    Py_INCREF(r);
    return r;
}

PyObject *
_async_is_active_ex(PyObject *self, PyObject *args)
{
    PyObject *r = (PyObject *)(_is_active_ex() ? Py_True : Py_False);
    Py_INCREF(r);
    return r;
}


PyObject *
_async_active_count(PyObject *self, PyObject *args)
{
    return PyLong_FromLong(PXSTATE()->active);
}

PyObject *
_async_active_contexts(PyObject *self, PyObject *args)
{
    return PyLong_FromLong(PXSTATE()->contexts_active);
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

int
_PxState_PurgeContexts(PxState *px)
{
    Heap *h;
    Stats *s;
    register Context *c;
    Context *prev, *next;
    int destroyed = 0;

    if (!px->ctx_first)
        return 0;

    c = px->ctx_first;
    while (c) {
        if (c->ttl > 0) {
            --(c->ttl);
            c = c->next;
            continue;
        }
        assert(c->ttl == 0);

        /* xxx todo: check refcnts of func/args/kwds etc? */
        decref_args(c);

        h = c->h;
        s = &(c->stats);
        _PyHeap_FastFree(h, s, c->error);
        _PyHeap_FastFree(h, s, c->errback_completed);
        _PyHeap_FastFree(h, s, c->callback_completed);
        _PyHeap_FastFree(h, s, c->outgoing);

        if (c->last_leak)
            free(c->last_leak);

        if (c->errors_tuple)
            _PyHeap_FastFree(h, s, c->errors_tuple);

        /* xxx todo: iterate over objects and check for any __dels__? */
        prev = c->prev;
        next = c->next;
        if (px->ctx_first == c)
            px->ctx_first = next;

        if (prev)
            prev->next = next;

        if (next)
            next->prev = prev;

        HeapDestroy(c->heap_handle);
        free(c);
        destroyed++;
        c = next;
        InterlockedDecrement(&(px->active));
    }

    if (destroyed) {
        px->contexts_destroyed += destroyed;
        px->contexts_active -= destroyed;
    }
    return destroyed;
}

__inline
int
_is_nowait_item(PxListItem *item)
{
    Context *c = (Context *)item->from;
    if (c->error != item || (void *)item->when == item->p4) {
        assert(c->error != item);
        assert((void *)item->when == item->p4);
        return 1;
    }
    return 0;
}


PyObject *
_async_run_once(PyObject *self, PyObject *args)
{
    int err = 0;
    int wait = -1;
    int purged = 0;
    unsigned short depth_hint = 0;
    unsigned int waited = 0;
    unsigned int depth = 0;
    unsigned int events = 0;
    unsigned int errors = 0;
    unsigned int processed_errors = 0;
    unsigned int processed_finished = 0;
    unsigned int processed_incoming = 0;
    unsigned int processed_errbacks = 0;
    unsigned int processed_callbacks = 0;
    PyObject *result = NULL;
    Context *c;
    PxState *px;
    PxListItem *item = NULL;
    PyThreadState *tstate;
    Py_GUARD

    tstate = get_main_thread_state();

    px = (PxState *)tstate->px;

    if (px->submitted == 0 && px->persistent == 0) {
        PyErr_SetNone(PyExc_AsyncRunCalledWithoutEventsError);
        return NULL;
    }

    px->last_done_count = px->done;
    px->last_submitted_count = px->submitted;

    px->last_sync_wait_done_count = px->sync_wait_done;;
    px->last_sync_wait_submitted_count = px->sync_wait_submitted;

    px->last_sync_nowait_done_count = px->sync_nowait_done;;
    px->last_sync_nowait_submitted_count = px->sync_nowait_submitted;

    purged = _PxState_PurgeContexts(px);

    item = PxList_Flush(px->finished);
    while (item) {
        ++processed_finished;
        c = (Context *)item->from;
        c->times_finished++;

        item = (Px_DECCTX(c) ?
            PxList_Transfer(px->finished, item) :
            PxList_SeverFromNext(item)
        );
    }

start:
    errors = 0;
    item = PxList_FlushWithDepthHint(px->errors, &depth_hint);
    if (item) {
        Context *f = NULL;
        PxListItem *i;
        size_t depth = 0;
        size_t newdepth = 0;
        PyObject *e;
        PyObject *t;
        PxListHead *transfer = NULL;

        assert(depth_hint != 0);

        /* Oh the hackery... */
        i = item;
        do {
            if (_is_nowait_item(i))
                continue;
            f = (Context *)i->from;
            break;
        } while (i = PxList_Next(i));

        if (!f) {
            Context *x;
            i = item;
            do {
                x = (Context *)i->from;
                _Py_clflush(&x->done);
                if (_is_nowait_item(i) && !x->done)
                    continue;
                f = x;
                break;
            } while (i = PxList_Next(i));

            if (!f)
                Py_FatalError("couldn't find a context to hijack");
        }

        f->size_before_hijack = c->stats.allocated;
        f->errors_tuple = t = _PyHeap_NewTuple(f, (Py_ssize_t)depth_hint);

        if (!t) {
            PxList_Push(px->errors, item);
            return PyErr_NoMemory();
        }
        f->hijacked_for_errors_tuple = 1;
        f->ttl++;

        do {
            c = (Context *)item->from;
            depth++;
            if ((depth_hint && depth > depth_hint)) {
                newdepth = depth + PxList_CountItems(item);
                if (!_PyHeap_ResizeTuple(f, f->errors_tuple, newdepth)) {
                    PxList_Push(px->errors, item);
                    return PyErr_NoMemory();
                }
                t = f->errors_tuple;
                depth_hint = 0;
            }
            e = _PyHeap_NewTuple(c, 3);
            if (!e) {
                PxList_Push(px->errors, item);
                return PyErr_NoMemory();
            }

            PyTuple_SET_ITEM(e, 0, (PyObject *)item->p1);
            PyTuple_SET_ITEM(e, 1, (PyObject *)item->p2);
            PyTuple_SET_ITEM(e, 2, (PyObject *)item->p3);

            PyTuple_SET_ITEM(t, depth-1, e);
            ++processed_errors;

            /*
             * Errors can originate from one of two places: from the parallel
             * context, or the logic below that processes no-wait calls-from-
             * main-thread.  Each no-wait call incremented the context refcnt,
             * so we need to decrement it here if we're dealing with a no-wait
             * call.  Additionally, no-wait items don't get added to the free-
             * list -- they'll be added as part of the normal parallel context
             * cleanup.
             *
             * We detect if an error is due to a failed no-wait callback by
             * comparing c->error to the item; if they don't match, it's a no-
             * wait error.  As an extra precaution, the code below sets the
             * p4 member to the item->when timestamp -- so we check that as
             * well.
             */
            if (_is_nowait_item(item)) {
                PxListItem *next = PxList_Next(item);
                //InterlockedDecrement(&(px->active));
                Px_DECCTX(c);
                _PyHeap_Free(c, item);
                item = next;
            } else {
                item = PxList_Transfer(px->finished, item);
                InterlockedIncrement64(&(px->done));
            }
        } while (item);

        return t;
    }

    /* Process 'incoming' work items. */
    item = PxList_Flush(px->incoming);
    if (item) {
        PxListHead *transfer;

        do {
            HANDLE wait;
            PyObject *func, *args, *kwds, *result, *tmp;

            func = (PyObject *)item->p1;
            args = (PyObject *)item->p2;
            kwds = (PyObject *)item->p3;
            wait = (HANDLE)item->p4;
            c = (Context *)item->from;

            if (!args) {
                if (kwds)
                    args = _PyHeap_NewTuple(c, 0);
                else
                    args = Py_None;
            } else {
                if (!PyTuple_Check(args)) {
                    tmp = _PyHeap_NewTuple(c, 1);
                    PyTuple_SET_ITEM(tmp, 0, args);
                    args = tmp;
                }
            }

            //Py_INCREF(func);
            //Py_INCREF(args);
            //Py_XINCREF(kwds);

            if (wait) {
                InterlockedDecrement(&(px->sync_wait_pending));
                InterlockedIncrement(&(px->sync_wait_inflight));
            } else {
                InterlockedDecrement(&(px->sync_nowait_pending));
                InterlockedIncrement(&(px->sync_nowait_inflight));
            }

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
                item = PxList_SeverFromNext(item);
            } else {
                InterlockedDecrement(&(px->sync_nowait_inflight));
                InterlockedIncrement64(&(px->sync_nowait_done));

                if (null_or_non_none_return_type(result)) {
                    assert (tstate->curexc_type != NULL);
                    item->p1 = tstate->curexc_type;
                    item->p2 = tstate->curexc_value;
                    item->p3 = tstate->curexc_traceback;
                    /* See error handling comments above for an explanation
                     * of the next line. */
                    item->p4 = (void *)item->when;
                    item = PxList_Transfer(px->errors, item);
                    errors++;
                } else {
                    PxListItem *next = PxList_SeverFromNext(item);
                    assert(result == Py_None);
                    //InterlockedDecrement(&(px->active));
                    c = (Context *)item->from;
                    Px_DECCTX(c);
                    _PyHeap_Free(c, item);
                    item = next;
                }
            }
            ++processed_incoming;
        } while (item);
    }
    if (errors)
        goto start;

    /* Process completed items. */
    item = PxList_Flush(px->completed_callbacks);
    if (item) {
        do {
            /* XXX TODO: update stats. */
            ++processed_callbacks;
            item = PxList_Transfer(px->finished, item);
        } while (item);
    }

    item = PxList_Flush(px->completed_errbacks);
    if (item) {
        do {
            /* XXX TODO: update stats. */
            ++processed_errbacks;
            item = PxList_Transfer(px->finished, item);
        } while (item);
    }

    if (px->active == 0 || purged)
        Py_RETURN_NONE;

    /* Return if we've done something useful... */
    if (processed_errors    ||
        processed_finished  ||
        processed_incoming  ||
        processed_errbacks  ||
        processed_callbacks)
            Py_RETURN_NONE;

    /* ...and wait for a second if we haven't. */
    err = WaitForSingleObject(px->wakeup, 1000);
    switch (err) {
        case WAIT_OBJECT_0:
            goto start;
        case WAIT_TIMEOUT:
            Py_RETURN_NONE;
        case WAIT_ABANDONED:
            PyErr_SetString(PyExc_SystemError, "wait abandoned");
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

    if (c->args && c->args != Py_None && !PyTuple_Check(c->args)) {
        PyObject *tmp = c->args;
        c->args = Py_BuildValue("(O)", c->args);
        Py_DECREF(tmp);
    }

    return 1;
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
    PxState  *px;

    if (!c)
        return PyErr_NoMemory();

    memset((void *)c, 0, sizeof(Context));

    if (!extract_args(args, c))
        goto free_context;

    c->heap_handle = HeapCreate(HEAP_NO_SERIALIZE, Px_DEFAULT_HEAP_SIZE, 0);
    if (!c->heap_handle) {
        PyErr_SetFromWindowsErr(0);
        goto free_context;
    }

    if (!_PyHeap_Init(c, 0))
        goto free_heap;

    c->tstate = get_main_thread_state();

    assert(c->tstate);
    px = c->px = (PxState *)c->tstate->px;
    InterlockedIncrement64(&(px->submitted));
    InterlockedIncrement(&(px->pending));
    InterlockedIncrement(&(px->active));
    c->stats.submitted = _Py_rdtsc();

    incref_args(c);

    c->refcnt = 1;
    c->ttl = px->ctx_ttl;

    if (!submit_work(c))
        goto decref_args;

    c->px->contexts_created++;
    c->px->contexts_active++;
    result = (Py_INCREF(Py_None), Py_None);
    goto done;

decref_args:
    decref_args(c);

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
    item = _PyHeap_NewListItem(c);
    if (!item)
        return PyErr_NoMemory();

    if (!PyArg_UnpackTuple(args, "", 1, 3, &(item->p1),
                           &(item->p2), &(item->p3)))
    {
        Heap_Free(item);
        return NULL;
    }

    if (wait) {
        item->p4 = (void *)CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!item->p4) {
            Heap_Free(item);
            return PyErr_SetFromWindowsErr(0);
        }
    }

    px = c->px;

    if (wait) {
        InterlockedIncrement64(&(px->sync_wait_submitted));
        InterlockedIncrement(&(px->sync_wait_pending));
    } else {
        Px_INCCTX(c);
        PxList_Push(c->outgoing, item);
        InterlockedIncrement64(&(px->sync_nowait_submitted));
        InterlockedIncrement(&(px->sync_nowait_pending));
    }

    //InterlockedIncrement(&(px->active));
    item->from = c;
    PxList_TimestampItem(item);
    PxList_Push(px->incoming, item);
    SetEvent(px->wakeup);
    if (!wait)
        return Py_None;

    _PyParallel_DisassociateCurrentThreadFromCallback();
    err = WaitForSingleObject(item->p4, INFINITE);
    switch (err) {
        case WAIT_ABANDONED:
            PyErr_SetString(PyExc_SystemError, "wait abandoned");
            goto cleanup;
        case WAIT_TIMEOUT:
            PyErr_SetString(PyExc_SystemError, "infinite wait timed out?");
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
    //InterlockedDecrement(&(px->active));
    InterlockedDecrement(&(px->sync_wait_inflight));
    InterlockedIncrement64(&(px->sync_wait_done));

    CloseHandle(item->p4);
    item->p4 = NULL;
    Heap_Free(item);

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
PyDoc_STRVAR(_async_is_active_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_submit_io_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_submit_work_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_submit_wait_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_is_active_ex_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_active_count_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_submit_timer_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_submit_class_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_submit_client_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_submit_server_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_active_contexts_doc, "XXX TODO\n");
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
    _ASYNC_N(is_active),
    _ASYNC_V(submit_io),
    _ASYNC_V(submit_work),
    _ASYNC_V(submit_wait),
    _ASYNC_N(is_active_ex),
    _ASYNC_N(active_count),
    _ASYNC_V(submit_timer),
    _ASYNC_O(submit_class),
    _ASYNC_O(submit_client),
    _ASYNC_O(submit_server),
    _ASYNC_N(active_contexts),
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


/* And now for the exported symbols... */
PyThreadState *
_PyParallel_GetThreadState(void)
{
#ifdef Py_DEBUG
    PyThreadState *tstate;
    Px_GUARD
    assert(ctx->pstate);
    assert(ctx->pstate != ctx->tstate);
#endif
    return ctx->pstate;
}

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
    return Object_New(tp, ctx);
}

PyVarObject *
_PxObject_NewVar(PyTypeObject *tp, Py_ssize_t nitems)
{
    return VarObject_New(tp, nitems, ctx);
}

PyObject *
_PxObject_Init(PyObject *op, PyTypeObject *tp)
{
    return Object_Init(op, tp, ctx);
}

PyVarObject *
_PxObject_InitVar(PyVarObject *op, PyTypeObject *tp, Py_ssize_t nitems)
{
    return VarObject_Init(op, tp, nitems, ctx);
}

PyVarObject *
_PxObject_Resize(PyVarObject *op, Py_ssize_t nitems)
{
    return VarObject_Resize((PyObject *)op, nitems, ctx);
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

/* vim:set ts=8 sw=4 sts=4 tw=78 et: */

