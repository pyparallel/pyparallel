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
__declspec(thread) static PyParallelContext ctx = { NULL };

__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
long Py_MainThreadId  = -1;
long Py_MainProcessId = -1;
long Py_ParallelContextsEnabled = -1;

void *Heap_Malloc(size_t);

static void
Heap_Init(Heap *h, size_t n)
{
    Stats *s = &ctx.stats;
    size_t size;

    if (n < Px_DEFAULT_HEAP_SIZE)
        size = Px_DEFAULT_HEAP_SIZE;
    else
        size = n;

    size = Px_CACHE_ALIGN(n);

    memset((void *)h, 0, sizeof(Heap));

    if (!ctx.heap_handle) {
        ctx.heap_handle = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
        if (!ctx.heap_handle)
            Py_FatalError("HeapCreate");
    }

    h->size = size;
    h->base = h->next = HeapAlloc(ctx.heap_handle, 0, h->size);
    if (!h->base)
        Py_FatalError("HeapAlloc");
    h->remaining = size;
    s->remaining += size;
    s->size += size;
    s->heaps++;
    h->sle_next = (Heap *)Heap_Malloc(Px_CACHE_ALIGN(sizeof(Heap)));
    ctx.h = h;
}

static __inline void
Heap_Extend(void)
{
    Stats *s = &ctx.stats;
    Heap *oldh = ctx.h;
    Heap *newh = oldh->sle_next;
    Heap_Init(newh, oldh->size * 2);
    assert(newh == ctx.h);
    newh->sle_prev = oldh;
}

void *
Heap_Malloc(size_t n)
{
    void  *next;
    Heap  *h;
    Stats *s = &ctx.stats;

    size_t size = Px_ALIGN(n);

begin:
    h = ctx.h;
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
    }

    Heap_Extend();
    goto begin;
}

void *
Heap_Realloc(void *p, size_t n)
{
    void  *r;
    Heap  *h = ctx.h;
    Stats *s = &ctx.stats;
    r = Heap_Malloc(n);
    h->reallocs++;
    s->reallocs++;
    memcpy(r, p, n);
    return r;
}

void
Heap_Free(void *p)
{
    Heap  *h = ctx.h;
    Stats *s = &ctx.stats;

    h->frees++;
    s->frees++;
}

void
_PyParallel_EnteredParallelContext(void *p)
{
    Stats    *s;
    Callback *c;
    UNREFERENCED_PARAMETER(s);
    UNREFERENCED_PARAMETER(c);

    memset((void *)&ctx, 0, sizeof(Context));
    Heap_Init(&ctx.heap, 0);
 
}

void
_PyParallel_LeavingParallelContext(void)
{
    Stats    *s;
    Callback *c;

    UNREFERENCED_PARAMETER(s);
    UNREFERENCED_PARAMETER(c);    
}


void
NTAPI
_PyParallel_Callback(void *instance, void *context, void *work)
{



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

/* Objects (PyObjects) */
static __inline
PyObject *
Object_Init(PyObject *op, PyTypeObject *tp)
{
    Stats  *s;
    Object *o;

/* Make sure we're not called for PyVarObjects... */
#ifdef Py_DEBUG
    assert(tp->tp_itemsize == 0);
#endif

    s = &ctx.stats;
    o = (Object *)Heap_Malloc(sizeof(Object));

    Py_TYPE(op) = tp;
    op->ob_refcnt = 1;
    o->op = op;
    append_object(&ctx.objects, o);
    s->objects++;

    return op;
}

static __inline
PyObject *
Object_New(PyTypeObject *tp)
{
    return Object_Init((PyObject *)Heap_Malloc(_PyObject_SIZE(tp)), tp);
}

/* VarObjects (PyVarObjects) */
static __inline
PyVarObject *
VarObject_Init(PyVarObject *op, PyTypeObject *tp, Py_ssize_t size)
{
    Stats  *s;
    Object *o;

/* Make sure we're not called for PyObjects... */
#ifdef Py_DEBUG
    assert(tp->tp_itemsize > 0);
#endif

    s = &ctx.stats;
    o = (Object *)Heap_Malloc(sizeof(Object));

    Py_SIZE(op) = size;
    Py_TYPE(op) = tp;
    ((PyObject *)op)->ob_refcnt = 1;
    o->op = (PyObject *)op;
    append_object(&ctx.varobjs, o);
    s->varobjs++;

    return op;
}

static __inline
PyVarObject *
VarObject_New(PyTypeObject *tp, Py_ssize_t nitems)
{
    register const size_t sz = _PyObject_VAR_SIZE(tp, nitems);
    register PyVarObject *v = (PyVarObject *)Heap_Malloc(sz);
    return VarObject_Init(v, tp, nitems);
}

static __inline
PyVarObject *
VarObject_Resize(PyVarObject *op, Py_ssize_t n)
{
    register const int was_resize = 1;
    register const size_t sz = _PyObject_VAR_SIZE(Py_TYPE(op), n);
    PyVarObject *r = (PyVarObject *)Heap_Malloc(sz);
    ctx.h->resizes++;
    ctx.stats.resizes++;
    return r;
}

/* And now for the exported symbols... */
void
_Px_NewReference(PyObject *op)
{
    op->ob_refcnt = 1;
    ctx.stats.newrefs++;
}

void
_Px_ForgetReference(PyObject *op)
{
    ctx.stats.forgetrefs++;
}

void
_Px_Dealloc(PyObject *op)
{
    ctx.h->deallocs++;
    ctx.stats.deallocs++;
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

















































