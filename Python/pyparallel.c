#include "Python.h"

#ifdef __cpplus
extern "C" {
#endif

#include "pyparallel_private.h"

#include <search.h>

__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
__declspec(thread) PyParallelContext *ctx = NULL;
#define _TMPBUF_SIZE 1024

__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
long Py_MainThreadId  = -1;
long Py_MainProcessId = -1;
long Py_ParallelContextsEnabled = -1;
size_t _PxObjectSignature = -1;

void *Heap_Malloc(size_t);
void *_PyHeap_Malloc(Context *c, size_t n, size_t align);

__inline
PyThreadState *
get_main_thread_state(void)
{
    return (PyThreadState *)_Py_atomic_load_relaxed(&_PyThreadState_Current);
}

__inline
PxState *
PXSTATE(void)
{
    register PyThreadState *pstate = get_main_thread_state();
    return (!pstate ? NULL : (PxState *)pstate->px);
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


__inline
void *
_PyHeap_MemAlignedMalloc(Context *c, size_t n)
{
    return _PyHeap_Malloc(c, n, Px_MEM_ALIGN_SIZE);
}

__inline
PxListItem *
_PyHeap_NewListItem(Context *c)
{
    return (PxListItem *)_PyHeap_MemAlignedMalloc(c, sizeof(PxListItem));
}

int
_Py_PXCTX(void)
{
    int active = (int)(Py_MainThreadId != _Py_get_current_thread_id());
    assert(Py_MainThreadId > 0);
    assert(Py_MainProcessId != -1);
    /*assert(Py_ParallelContextsEnabled != -1);
    if (Py_ParallelContextsEnabled)
        assert(active);
    else
        assert(!active);*/
    return active;
}

#ifdef Py_DEBUG
/*
void
_PxState_InitPages(PxState *px)
{
    Py_GUARD

    InitializeSRWLock(&px->pages_srwlock);

    px->pages_seen      = PyDict_New();
    px->pages_freed     = PyDict_New();
    px->pages_active    = PyDict_New();
    px->pages_incoming  = PxList_New();

    assert(
        px->pages_seen      &&
        px->pages_freed     &&
        px->pages_active    &&
        px->pages_incoming
    );
}
*/

void
_PxState_InitPxPages(PxState *px)
{
    Py_GUARD

    InitializeSRWLock(&px->pages_srwlock);
}

/*
__inline
int
_PxPages_Contains(PxState *px, PyObject *dict, PyObject *ptr,  int lock)
{
    int result;

    if (lock)
        AcquireSRWLockShared(&px->pages_srwlock);

    result = PyDict_Contains(dict, ptr);

    if (lock)
        ReleaseSRWLockShared(&px->pages_srwlock);

    assert(result != -1);
    return result;
}

#define _PxPages_FL_NOT_FOUND  (0UL)
#define _PxPages_FL_SEEN       (1UL >> 1)
#define _PxPages_FL_FREED      (1UL >> 2)
#define _PxPages_FL_ACTIVE     (1UL >> 3)

#define _PxPages_SEEN(p)        \
    (!p ? 0 : PyDict_Contains(px->pages_seen, p) == 1)

#define _PxPages_NEVER_SEEN(p)  (!p ? 1 : !_PxPages_SEEN(p))

#define _PxPages_IS_FREED(p)    \
    (!p ? 0 : (PyDict_Contains(px->pages_freed, p) == 1)

#define _PxPages_IS_ACTIVE(p)   \
    (!p ? 0 : PyDict_Contains(px->pages_active, p) == 1)


int
_PxPages_Find(PxState *px, void *m)
{
    PyObject    *ptr;
    Px_UINTPTR   a;
    Py_ssize_t   i = 0;
    int flags;

    assert(m);
    assert(Px_PTR(m) == Px_PTR_ALIGN(m));

    a = Px_PAGE_ALIGN_DOWN(m);
    assert(a == Px_PAGE_ALIGN(a));

    ptr = PyLong_FromVoidPtr((void *)a);

    flags = 0;

    AcquireSRWLockShared(&px->pages_srwlock);

    if (_PxPages_Contains(px, px->pages_seen, ptr, 0))
        flags = _PxPages_FL_SEEN;

    if (_PxPages_Contains(px, px->pages_active, ptr, 0)) {
        assert((flags & _PxPages_FL_SEEN));
        flags |= _PxPages_FL_ACTIVE;
    }

    if (_PxPages_Contains(px, px->pages_freed, ptr, 0)) {
        assert((flags & _PxPages_FL_SEEN));
        assert(!(flags & _PxPages_FL_ACTIVE));
        flags |= _PxPages_FL_FREED;
    }

    ReleaseSRWLockShared(&px->pages_srwlock);

    return flags;
}

__inline
int
_PxPages_NotPxPointer(PxState *px, void *m)
{
    return (!m ? 1 : (_PxPages_Find(px, m) == _PxPages_FL_NOT_FOUND));
}

__inline
int
_PxPages_IsActivePxPointer(PxState *px, void *m)
{
    return (!m ? 0 : ((_PxPages_Find(px, m) & _PxPages_FL_ACTIVE)));
}

__inline
int
_PxPages_NeverSeen(PxState *px, void *m)
{
    return (!m ? 0 : !((_PxPages_Find(px, m) & _PxPages_FL_SEEN)));
}
*/

__inline
void
_PxWarn_RegisteringFreedHeap(int i, void *b, void *p, int s)
{
    PySys_FormatStderr(
        "WARNING! registering previously freed heap "
        "(i: %d, h->base: 0x%llx, base: 0x%llx, seen: %d)\n",
        i, b, p, s
    );
}

void
_PxState_RegisterHeap(PxState *px, Heap *h, Context *c)
{
    int i;
    Py_GUARD

    AcquireSRWLockExclusive(&px->pages_srwlock);

    assert((h->size % Px_PAGE_SIZE) == 0);

    for (i = 0; i < h->pages; i++) {
        void *p;
        Px_UINTPTR lower, upper;

        p = Px_PTR_ADD(h->base, (i * Px_PAGE_SIZE));

        lower = Px_PAGE_ALIGN_DOWN(p);
        upper = Px_PAGE_ALIGN(p);

        _PxPages_AddHeapPage(&(px->pages), &lower, h);
        if (lower != upper)
            _PxPages_AddHeapPage(&(px->pages), &upper, h);
    }
    ReleaseSRWLockExclusive(&px->pages_srwlock);

}

/*
void
_PxState_RegisterHeap_disabled(PxState *px, Heap *h, Context *c)
{
    int i;
    Py_GUARD

    AcquireSRWLockExclusive(&px->pages_srwlock);

    assert((h->size % Px_PAGE_SIZE) == 0);

    for (i = 0; i < h->pages; i++) {
        PyObject *base;
        PyObject *value;
        void     *baseptr;
        int       result;
        int       times_seen;
        int       is_freed;
        int       is_active;

        baseptr = Px_PTR_ADD(h->base, (i * Px_PAGE_SIZE));
        base = PyLong_FromVoidPtr(baseptr);
        assert(base);

        result = PyDict_Contains(px->pages_seen, base);
        assert(result != -1);
        if (result == 1) {
            PyLongObject *l;
            value = PyDict_GetItem(px->pages_seen, base);
            assert(value);
            assert(PyLong_Check(value));
            l = (PyLongObject *)value;
            assert(Py_SIZE(l) == 1);
            times_seen = l->ob_digit[0];
            l->ob_digit[0] += 1;
        } else {
            times_seen = 0;
        }

        result = PyDict_Contains(px->pages_freed, base);
        assert(result != -1);
        is_freed = (result == 1);

        result = PyDict_Contains(px->pages_active, base);
        assert(result != -1);
        is_active = (result == 1);

        if (is_freed) {
            assert(times_seen >= 1);
            assert(!is_active);
            _PxWarn_RegisteringFreedHeap(i, h->base, baseptr, times_seen);
            result = PyDict_DelItem(px->pages_freed, base);
            value = PyDict_GetItem(px->pages_freed, base);
            assert(value);
        }

        if (is_active) {
            assert(times_seen >= 1);
            assert(0 == (void *)"re-registering active heap!");
        }

        result = PyDict_SetItem(px->pages_active, base, Py_None);
        assert(result == 0);

        if (times_seen == 0)
            result = PyDict_SetItem(px->pages_seen, base, PyLong_FromLong(1));
    }
    ReleaseSRWLockExclusive(&px->pages_srwlock);
}

void
_PxContext_RegisterHeap_disabled(Context *c, Heap *h)
{
    if (!Py_PXCTX) {
        _PxState_RegisterHeap(c->px, h, c);
    } else {
        int result;
        PxListItem *item;

        assert(h->remaining >= PxListItem_SIZE);

        _PyParallel_DisassociateCurrentThreadFromCallback();
        item = (PxListItem *)_PyHeap_NewListItem(c);
        item->from = c;
        PxList_TimestampItem(item);
        item->p1 = h;
        item->p2 = CreateEvent(NULL, FALSE, FALSE, NULL);
        PxList_Push(c->px->pages_incoming, item);
        SetEvent(c->px->wakeup);

        result = WaitForSingleObject(item->p2, INFINITE);
        assert(result == WAIT_OBJECT_0);
    }
}

void
_PxContext_UnregisterHeaps_disabled(Context *c)
{
    Heap    *h;
    Stats   *s;
    PxState *px;
    int      i, heap_count = 0;

    Py_GUARD

    px =  c->px;
    s  = &c->stats;

    AcquireSRWLockExclusive(&px->pages_srwlock);

    h = &c->heap;
    while (h) {
        heap_count++;

        assert((h->size % Px_PAGE_SIZE) == 0);

        for (i = 0; i < h->pages; i++) {
            void     *ptr;
            PyObject *base;

            ptr = Px_PTR_ADD(h->base, (i * Px_PAGE_SIZE));
            assert(Px_PTR(ptr) == Px_PTR_ALIGN(ptr));
            assert(Px_PTR(ptr) == Px_PAGE_ALIGN(ptr));

            base = PyLong_FromVoidPtr(ptr);
            assert(base);

            assert(PyDict_Contains(px->pages_active, base) == 1);
            assert(PyDict_Contains(px->pages_freed,  base) == 0);

            assert(PyDict_DelItem(px->pages_active, base) == 0);
            assert(PyDict_SetItem(px->pages_freed, base, Py_None) == 0);

            Py_DECREF(base);
        }

        h = h->sle_next;
    }
    ReleaseSRWLockExclusive(&px->pages_srwlock);
    assert(heap_count == s->heaps);
}
*/

void
_PxContext_UnregisterHeaps(Context *c)
{
    Heap    *h;
    Stats   *s;
    PxState *px;
    int      i, heap_count = 0;

    Py_GUARD

    px =  c->px;
    s  = &c->stats;

    AcquireSRWLockExclusive(&px->pages_srwlock);

    h = &c->heap;
    while (h) {
        heap_count++;

        assert((h->size % Px_PAGE_SIZE) == 0);

        for (i = 0; i < h->pages; i++) {
            void *p;
            Px_UINTPTR lower, upper;

            p = Px_PTR_ADD(h->base, (i * Px_PAGE_SIZE));

            lower = Px_PAGE_ALIGN_DOWN(p);
            upper = Px_PAGE_ALIGN(p);

            _PxPages_RemoveHeapPage(&(px->pages), &lower, h);
            if (lower != upper)
                _PxPages_RemoveHeapPage(&(px->pages), &upper, h);
        }

        h = h->sle_next;
        if (h->size == 0)
            break;
    }
    ReleaseSRWLockExclusive(&px->pages_srwlock);
    assert(heap_count == s->heaps);
}

#define _MEMSIG_INVALID     (0UL)
#define _MEMSIG_NOT_READY   (1UL)
#define _MEMSIG_NULL        (1UL << 1)
#define _MEMSIG_UNKNOWN     (1UL << 2)
#define _MEMSIG_PY          (1UL << 3)
#define _MEMSIG_PX          (1UL << 4)

#define _OBJSIG_INVALID     (0UL)
#define _OBJSIG_NULL        (1UL << 1)
#define _OBJSIG_UNKNOWN     (1UL << 2)  /* 4  */
#define _OBJSIG_PY          (1UL << 3)  /* 8  */
#define _OBJSIG_PX          (1UL << 4)  /* 16 */

__declspec(thread) int _Px_MemorySignature_CallDepth = 0;
__declspec(thread) int _Px_ObjectSignature_CallDepth = 0;
__declspec(thread) int _Px_SafeObjectSignatureTest_CallDepth = 0;

unsigned long
_Px_MemorySignature(void *m)
{
    PxState *px;
    unsigned long signature;

    if (!m)
        return _MEMSIG_NULL;

    if (_PyMem_InRange(m))
        return _MEMSIG_PY;

    px = PXSTATE();
    if (!px)
        return _MEMSIG_NOT_READY;

    assert(_Px_MemorySignature_CallDepth == 0);
    _Px_MemorySignature_CallDepth++;

    signature = _MEMSIG_UNKNOWN;

    AcquireSRWLockShared(&px->pages_srwlock);
    if (PxPages_Find(px->pages, m))
        signature = _MEMSIG_PX;
    ReleaseSRWLockShared(&px->pages_srwlock);

    _Px_MemorySignature_CallDepth--;

    return signature;
}

unsigned long
_Px_ObjectSignature(void *m)
{
    PyObject     *y;
    Py_uintptr_t  s;
    unsigned long signature;

    if (!m)
        return _OBJSIG_NULL;

    assert(_Px_ObjectSignature_CallDepth == 0);
    _Px_ObjectSignature_CallDepth++;

    s = -1;
    y = (PyObject *)m;

    __try {
        s = ((Py_uintptr_t)(y->is_px));
    } __except(
        GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
            EXCEPTION_EXECUTE_HANDLER :
            EXCEPTION_CONTINUE_SEARCH
    ) {
        s = (Py_uintptr_t)NULL;
    }

    assert(s != -1);

    if (!s) {
        signature = _OBJSIG_UNKNOWN;
        goto done;
    }

    if (s == (Py_uintptr_t)_Py_NOT_PARALLEL) {
        assert(y->px == _Py_NOT_PARALLEL);
        signature = _OBJSIG_PY;
        goto done;
    }

    if (s == (Py_uintptr_t)_Py_IS_PARALLEL) {
        assert(y->px != NULL);
        assert(Py_ASPX(y)->signature == _PxObjectSignature);
        signature = _OBJSIG_PX;
        goto done;
    }

    /* We'll hit this if m is a valid pointer (i.e. dereferencing m->is_px
     * doesn't trigger the SEH), but it doesn't point to something with a
     * valid object signature.
     */
    signature = _OBJSIG_UNKNOWN;
done:
    _Px_ObjectSignature_CallDepth--;
    return signature;
}

unsigned long
_Px_SafeObjectSignatureTest(void *m)
{
    PyObject     *y;
    PxObject     *x;
    Py_uintptr_t  s;
    int is_py;
    int is_px;
    unsigned long signature;

    if (!m)
        return _OBJSIG_NULL;

    s = -1;
    y = (PyObject *)m;

    assert(_Px_SafeObjectSignatureTest_CallDepth == 0);
    _Px_SafeObjectSignatureTest_CallDepth++;

    __try {
        s = ((Py_uintptr_t)(y->is_px));
    } __except(
        GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
            EXCEPTION_EXECUTE_HANDLER :
            EXCEPTION_CONTINUE_SEARCH
    ) {
        s = (Py_uintptr_t)NULL;
    }

    assert(s != -1);

    if (!s) {
        signature = _OBJSIG_UNKNOWN;
        goto done;
    }

    is_py = (
        (s == (Py_uintptr_t)_Py_NOT_PARALLEL) &&
        (y->px == _Py_NOT_PARALLEL)
    );

    if (is_py) {
        signature = _OBJSIG_PY;
        goto done;
    }

    is_px = -1;
    x = Py_ASPX(y);
    __try {
        is_px = (x->signature == _PxObjectSignature);
    } __except(
        GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
            EXCEPTION_EXECUTE_HANDLER :
            EXCEPTION_CONTINUE_SEARCH
    ) {
        is_px = 0;
    }

    assert(is_px != -1);

    signature = (is_px ? _OBJSIG_PX : _OBJSIG_UNKNOWN);
done:
    _Px_SafeObjectSignatureTest_CallDepth--;
    return signature;
}

int
_PyParallel_GuardObj(const char *function,
                     const char *filename,
                     int lineno,
                     void *m,
                     unsigned int flags)
{
    unsigned long s, o;

    assert(_OBJTEST(flags));

    if (m) {
        o = _Px_SafeObjectSignatureTest(m);
        s = _Px_MemorySignature(m);
        if (s & _MEMSIG_NOT_READY || (o > s))
            s = o;

        if (s & (_OBJSIG_UNKNOWN))
            s = _OBJSIG_PY;

        assert(s & (_OBJSIG_UNKNOWN | _OBJSIG_PX | _OBJSIG_PY));
    }

    if (flags & (_PXOBJ_TEST | _PY_ISPX_TEST)) {

        if (!m)
            return 0;

        if (flags & _PY_ISPX_TEST) {
            /* Special case for Py_ISPX(o); o must be a valid object. */
            assert(s & (_OBJSIG_PY | _OBJSIG_PX));
            return (Py_PXCTX ? 1 : (s & _OBJSIG_PX));
        }

        return (s & _OBJSIG_PX);

    } else if (flags & _PYOBJ_TEST) {

        if (!m)
            return 0;

        return (s & _OBJSIG_PY);

    } else {
        assert(m);

        assert(flags & (_PYOBJ_GUARD | _PXOBJ_GUARD));

        if (flags & _PYOBJ_GUARD)
            assert(s & _OBJSIG_PY);
        else
            assert(s & _OBJSIG_PX);

        return 0;
    }
}

__inline
void
_PxWarn_PyMemUnknown(void)
{
    PySys_FormatStderr(
        "WARNING! expected _MEMSIG_PY but got _MEMSIG_UNKNOWN\n"
    );
}

int
_PyParallel_GuardMem(const char *function,
                     const char *filename,
                     int lineno,
                     void *m,
                     unsigned int flags)
{
    unsigned long s;
    unsigned long o;

    assert(_MEMTEST(flags));

    if (m) {
        o = _Px_SafeObjectSignatureTest(m);
        s = _Px_MemorySignature(m);
        if (s & _MEMSIG_NOT_READY || (o > s))
            s = o;
    }

    if (flags & (_PYMEM_TEST | _PXMEM_TEST)) {

        if (!m)
            return 0;

        return (flags & _PYMEM_TEST ? s & _MEMSIG_PY : s & _MEMSIG_PX);

    } else {
        assert(m);

        assert(flags & (_PYMEM_GUARD | _PXMEM_GUARD));

        if (flags & _PYMEM_GUARD) {
            if (s & _MEMSIG_UNKNOWN) {
                //printf("expected _MEMSIG_PY but got _MEMSIG_UNKNOWN\n");
                return 0;
            }
            assert(s & _MEMSIG_PY);
        } else {
            if (!(s & _MEMSIG_PX)) {
                printf("\ncouldn't find ptr: 0x%llx\n", m);
                PxPages_Dump((PXSTATE()->pages));
            } else {
                //printf("found ptr 0x%llx\n", m);
            }
            assert(s & _MEMSIG_PX);
        }

        return 0;
    }
}

int
_PyParallel_Guard(const char *function,
                  const char *filename,
                  int lineno,
                  void *m,
                  unsigned int flags)
{
    assert(_Py_UINT32_BITS_SET(flags) == 1);

    if (_OBJTEST(flags))
        return _PyParallel_GuardObj(function, filename, lineno, m, flags);
    else {
        assert(_MEMTEST(flags));
        return _PyParallel_GuardMem(function, filename, lineno, m, flags);
    }
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


#endif

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

    assert((size % Px_PAGE_SIZE) == 0);

    if (!c->h)
        /* First init. */
        h = &(c->heap);
    else {
        h = c->h->sle_next;
        h->sle_prev = c->h;
    }

    assert(h);
    h->pages = size / Px_PAGE_SIZE;

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
#ifdef Py_DEBUG
    _PxState_RegisterHeap(c->px, h, c);
#endif
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
            c->tbuf_next = Px_PTR_ADD(c->tbuf_next, alignment_diff);
            assert(Px_PTR_ADD(c->tbuf_base, c->tbuf_allocated) == c->tbuf_next);
        }

        c->tbuf_mallocs++;
        c->tbuf_allocated += aligned_size;
        c->tbuf_remaining -= aligned_size;

        c->tbuf_bytes_wasted += (aligned_size - requested_size);

        c->tbuf_last_alignment = alignment;

        next = c->tbuf_next;
        c->tbuf_next = Px_PTR_ADD(c->tbuf_next, aligned_size);
        assert(Px_PTR_ADD(c->tbuf_base, c->tbuf_allocated) == c->tbuf_next);

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
            h->next = Px_PTR_ADD(h->next, alignment_diff);
            assert(Px_PTR_ADD(h->base, h->allocated) == h->next);
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
        h->next = Px_PTR_ADD(h->next, aligned_size);
        assert(Px_PTR_ADD(h->base, h->allocated) == h->next);
        return next;
    }

    /* Force a resize. */
    if (!_PyHeap_Init(c, Px_NEW_HEAP_SIZE(aligned_size)))
        return Heap_LocalMalloc(c, aligned_size, alignment);

    goto begin;
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
    Px_GUARD_MEM(p);
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
    Px_GUARD_MEM(p);

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
    Px_GUARD_MEM(p);
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

#define _Px_X_OFFSET(n) (Px_PTR_ALIGN(n))
#define _Px_O_OFFSET(n) \
    (Px_PTR_ALIGN((_Px_X_OFFSET(n)) + (Px_PTR_ALIGN(sizeof(PxObject)))))

#define _Px_X_PTR(p, n) \
    ((PxObject *)(Px_PTR_ALIGN(Px_PTR_ALIGNED_ADD((p), _Px_X_OFFSET((n))))))

#define _Px_O_PTR(p, n) \
    ((Object *)(Px_PTR_ALIGN(Px_PTR_ALIGNED_ADD((p), _Px_O_OFFSET((n))))))

#define _Px_SZ(n) (Px_PTR_ALIGN(      \
    Px_PTR_ALIGN(n)                 + \
    Px_PTR_ALIGN(sizeof(PxObject))  + \
    Px_PTR_ALIGN(sizeof(Object))      \
))

#define _Px_VSZ(t, n) (Px_PTR_ALIGN( \
    ((!((t)->tp_itemsize)) ?         \
        _PyObject_SIZE(t) :          \
        _PyObject_VAR_SIZE(t, n))))

__inline
PyObject *
init_object(Context *c, PyObject *p, PyTypeObject *tp, Py_ssize_t nitems)
{
    register PxObject *x;
    register Object   *o;
    const register size_t sz = _Px_VSZ(tp, nitems);
    const register size_t total = _Px_SZ(sz);

    if (!p) {
        p = (PyObject *)_PyHeap_Malloc(c, total, 0);
        assert(p);
    } else
        Px_GUARD_MEM(p);

    assert(p);
    p->is_px = _Py_IS_PARALLEL;

    Py_TYPE(p)   = tp;
    Py_REFCNT(p) = 1;

    o = _Px_O_PTR(p, sz);
    o->op = p;

    x = _Px_X_PTR(p, sz);
    x->ctx       = c;
    x->size      = sz;
    x->resized   = 0;
    x->signature = _PxObjectSignature;
    Py_PX(p)     = x;

    if (!tp->tp_itemsize) {
        append_object(&c->varobjs, o);
        (c->stats.objects)++;
    } else {
        Py_SIZE(p) = nitems;
        append_object(&c->varobjs, o);
        (c->stats.varobjs)++;
    }

    if (!c->ob_first) {
        c->ob_first = p;
        c->ob_last  = p;
        p->_ob_next = NULL;
        p->_ob_prev = NULL;
    } else {
        PyObject *last;
        assert(!c->ob_first->_ob_prev);
        assert(!c->ob_last->_ob_next);
        last = c->ob_last;
        last->_ob_next = p;
        p->_ob_prev = last;
        p->_ob_next = NULL;
        c->ob_last = p;
    }

    return p;
}

__inline
PyObject *
Object_Init(PyObject *op, PyTypeObject *tp, Context *c)
{
    assert(tp->tp_itemsize == 0);
    return init_object(c, op, tp, 0);
}

__inline
PyObject *
Object_New(PyTypeObject *tp, Context *c)
{
    return init_object(c, NULL, tp, 0);
}

/* VarObjects (PyVarObjects) */
__inline
PyVarObject *
VarObject_Init(PyVarObject *v, PyTypeObject *tp, Py_ssize_t nitems, Context *c)
{
    assert(tp->tp_itemsize > 0);
    return (PyVarObject *)init_object(c, (PyObject *)v, tp, nitems);
}

__inline
PyVarObject *
VarObject_New(PyTypeObject *tp, Py_ssize_t nitems, Context *c)
{
    return (PyVarObject *)init_object(c, NULL, tp, nitems);
}

__inline
PyVarObject *
VarObject_Resize(PyObject *v, Py_ssize_t nitems, Context *c)
{
    PyTypeObject *tp;
    PyVarObject  *r;
    Px_GUARD_OBJ(v);

    tp = Py_TYPE(v);
    r = (PyVarObject *)init_object(c, NULL, tp, nitems);

    if (!r)
        return NULL;

    init_object(c, (PyObject *)r, tp, nitems);

    Py_ASPX(v)->resized = 1;
    memcpy(r, v, Py_ASPX(v)->size);
    c->h->resizes++;
    c->stats.resizes++;
    v = (PyObject *)r;
    return r;
}

__inline
PyObject *
_old_Object_Init(PyObject *op, PyTypeObject *tp, Context *c)
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
_old_Object_New(PyTypeObject *tp, Context *c)
{
    return Object_Init((PyObject *)Heap_Malloc(_PyObject_SIZE(tp)), tp, c);
}

/* VarObjects (PyVarObjects) */
__inline
PyVarObject *
_old_VarObject_Init(PyVarObject *op, PyTypeObject *tp, Py_ssize_t size, Context *c)
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
_old_VarObject_New(PyTypeObject *tp, Py_ssize_t nitems, Context *c)
{
    register const size_t sz = _PyObject_VAR_SIZE(tp, nitems);
    register PyVarObject *v = (PyVarObject *)_PyHeap_Malloc(c, sz, 0);
    return VarObject_Init(v, tp, nitems, c);
}

__inline
PyVarObject *
_old_VarObject_Resize(PyObject *op, Py_ssize_t n, Context *c)
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
null_with_exc_or_non_none_return_type(PyObject *op, PyThreadState *tstate)
{
    if (!op && tstate->curexc_type)
        return 1;

    assert(!tstate->curexc_type);

    if ((!op && !tstate->curexc_type) || op == Py_None)
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

    px->wakeup = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!px->wakeup)
        goto free_finished;

#ifdef Py_DEBUG
    _PxState_InitPxPages(px);
#endif

    InitializeCriticalSectionAndSpinCount(&(px->cs), 12);

    tstate->px = px;

    tstate->is_parallel_thread = 0;
    px->ctx_ttl = 1;

    goto done;

/*
free_wakeup:
    CloseHandle(px->wakeup);
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

Py_TLS static int _PxNewThread = 1;

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
    InterlockedDecrement(&(px->pending));
    InterlockedIncrement(&(px->inflight));

    ctx = c;

    if (_PxNewThread) {
        /* xxx new thread init */
        _PxNewThread = 0;
    } else {
        /* xxx not a new thread */
    }

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
            if (null_with_exc_or_non_none_return_type(r, pstate))
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
        if (!null_with_exc_or_non_none_return_type(r, pstate)) {
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

    _PxObjectSignature = (_Py_rdtsc() ^ Px_PTR(&_PxObjectSignature));

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
_PyParallel_NewThreadState(PyThreadState *tstate)
{
    return;
}

/* mod _parallel */
PyObject *
_parallel_map(PyObject *self, PyObject *args)
{
    return NULL;
}

//PyObject *
//_parallel_seen_pages(PyObject *self, PyObject *args)
//{
//    PxState  *px;
//    PyObject *result;
//    Py_GUARD
//
//    px = PXSTATE();
//
//    /* This needs to be a deepcopy ASAP. */
//    AcquireSRWLockShared(&px->pages_srwlock);
//    result = PyDictProxy_New(PyDict_Copy(px->pages_seen));
//    ReleaseSRWLockShared(&px->pages_srwlock);
//    return result;
//}
//
//PyObject *
//_parallel_freed_pages(PyObject *self, PyObject *args)
//{
//    PxState  *px;
//    PyObject *result;
//    Py_GUARD
//
//    px = PXSTATE();
//
//    /* This needs to be a deepcopy ASAP. */
//    AcquireSRWLockShared(&px->pages_srwlock);
//    result = PyDictProxy_New(PyDict_Copy(px->pages_freed));
//    ReleaseSRWLockShared(&px->pages_srwlock);
//    return result;
//}
//
//PyObject *
//_parallel_active_pages(PyObject *self, PyObject *args)
//{
//    PxState  *px;
//    PyObject *result;
//    Py_GUARD
//
//    px = PXSTATE();
//
//    /* This needs to be a deepcopy ASAP. */
//    AcquireSRWLockShared(&px->pages_srwlock);
//    result = PyDictProxy_New(PyDict_Copy(px->pages_active));
//    ReleaseSRWLockShared(&px->pages_srwlock);
//    return result;
//}

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

//PyDoc_STRVAR(_parallel_seen_pages_doc, "XXX TODO\n");
//PyDoc_STRVAR(_parallel_freed_pages_doc, "XXX TODO\n");
//PyDoc_STRVAR(_parallel_active_pages_doc, "XXX TODO\n");

#define _METHOD(m, n, a) {#n, (PyCFunction)##m##_##n##, a, m##_##n##_doc }
#define _PARALLEL(n, a) _METHOD(_parallel, n, a)
#define _PARALLEL_N(n) _METHOD(_parallel, n, METH_NOARGS)
#define _PARALLEL_O(n) _METHOD(_parallel, n, METH_O)
#define _PARALLEL_V(n) _METHOD(_parallel, n, METH_VARARGS)
static PyMethodDef _parallel_methods[] = {
    _PARALLEL_V(map),
    //_PARALLEL_N(seen_pages),
    //_PARALLEL_N(freed_pages),
    //_PARALLEL_N(active_pages),

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
int
_is_parallel_thread(void)
{
    return PyThreadState_GET()->is_parallel_thread;
}

PyObject *
_async_is_parallel_thread(void)
{
    PyObject *r = (PyObject *)(_is_parallel_thread() ? Py_True : Py_False);
    Py_INCREF(r);
    return r;
}

__inline
unsigned long long
_rdtsc(void)
{
    return _Py_rdtsc();
}

PyObject *
_async_rdtsc(void)
{
    return PyLong_FromUnsignedLongLong(_Py_rdtsc());
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

        if (px->ctx_last == c)
            px->ctx_last = prev;

        if (prev)
            prev->next = next;

        if (next)
            next->prev = prev;

#ifdef Py_DEBUG
        _PxContext_UnregisterHeaps(c);
#endif

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
_handle_parallel_error_item(PxListItem *item)
{
    PxState *px = PXSTATE();

    assert(PyExceptionClass_Check((PyObject *)item->p1));
    PyErr_Restore((PyObject *)item->p1,
                  (PyObject *)item->p2,
                  (PyObject *)item->p3);

    PxList_Transfer(px->finished, item);
    InterlockedIncrement64(&(px->done));

    return NULL;
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

#if defined(Py_DEBUG) && 0
    while (item = PxList_Pop(px->pages_incoming)) {
        _PxState_RegisterHeap(px, (Heap *)item->p1, (Context *)item->from);
        SetEvent((HANDLE)item->p2);
    }
#endif

start:
    /* First error wins. */
    item = PxList_Pop(px->errors);
    if (item) {
        assert(PyExceptionClass_Check((PyObject *)item->p1));
        PyErr_Restore((PyObject *)item->p1,
                      (PyObject *)item->p2,
                      (PyObject *)item->p3);

        PxList_Transfer(px->finished, item);
        InterlockedIncrement64(&(px->done));
        return NULL;
    }

    /* Process incoming work items. */
    while (item = PxList_Pop(px->incoming)) {
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

        ++processed_incoming;

        if (wait) {
            if (!result) {
                PyObject **exc_type, **exc_value, **exc_tb;
                assert(tstate->curexc_type);

                exc_type  = (PyObject **)&item->p1;
                exc_value = (PyObject **)&item->p2;
                exc_tb    = (PyObject **)&item->p3;

                PyErr_Fetch(exc_type, exc_value, exc_tb);
                PyErr_Clear();

                //Py_INCREF(exc_type);
                //Py_XINCREF(exc_value);
                //Py_XINCREF(exc_tb);
            } else {
                Py_INCREF(result);
                item->p1 = NULL;
                item->p2 = result;
                item->p3 = NULL;
            }
            SetEvent(wait);
        } else {
            InterlockedDecrement(&(px->sync_nowait_inflight));
            InterlockedIncrement64(&(px->sync_nowait_done));

            if (!result) {
                assert(tstate->curexc_type != NULL);
            } else if (result != Py_None) {
                char *msg = "async call from main thread returned non-None";
                PyErr_WarnEx(PyExc_RuntimeWarning, msg, 1);
            }

            c = (Context *)item->from;
            Px_DECCTX(c);
            _PyHeap_Free(c, item);

            if (!result)
                return NULL;
        }
    }

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
            break;
        case WAIT_FAILED:
            PyErr_SetFromWindowsErr(0);
            break;
    }
    return NULL;
}

PyObject *
_async_map(PyObject *self, PyObject *args)
{
    PyObject *result = NULL;

    return result;
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

    c->tstate = get_main_thread_state();

    assert(c->tstate);
    px = c->px = (PxState *)c->tstate->px;

    if (!_PyHeap_Init(c, 0))
        goto free_heap;

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
_async_run(PyObject *self, PyObject *args)
{
    PyThreadState *tstate = get_main_thread_state();
    PxState *px = PXSTATE();
    int i = 0;
    long active_contexts = 0;
    do {
        i++;
        active_contexts = px->contexts_active;
        if (Py_VerboseFlag)
            PySys_FormatStdout("_async.run(%d) [%d]\n", i, active_contexts);
        assert(active_contexts >= 0);
        if (active_contexts == 0)
            break;
        if (!_async_run_once(NULL, NULL)) {
            assert(tstate->curexc_type != NULL);
            if (Py_VerboseFlag)
                PySys_FormatStdout("_async.run_once raised "
                                   "exception, returning...\n");
            return NULL;
        }
    } while (1);

    if (Py_VerboseFlag)
        PySys_FormatStdout("_async.run(): no more events, returning...\n");
    Py_RETURN_NONE;
}

PyObject *
_async_submit_wait(PyObject *self, PyObject *args)
{
    PyObject *result = NULL;

    return result;
}

PyObject *
_async_submit_timer(PyObject *self, PyObject *args)
{
    PyObject *result = NULL;

    return result;
}

PyObject *
_async_submit_io(PyObject *self, PyObject *args)
{
    PyObject *result = NULL;

    return result;
}

PyObject *
_async_submit_server(PyObject *self, PyObject *args)
{
    PyObject *result = NULL;

    return result;
}

PyObject *
_async_submit_client(PyObject *self, PyObject *args)
{
    PyObject *result = NULL;

    return result;
}

PyObject *
_async_submit_class(PyObject *self, PyObject *args)
{
    PyObject *result = NULL;

    return result;
}

PyObject *
_call_from_main_thread(PyObject *self, PyObject *args, int wait)
{
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
PyDoc_STRVAR(_async_rdtsc_doc, "XXX TODO\n");
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
PyDoc_STRVAR(_async_is_parallel_thread_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_call_from_main_thread_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_call_from_main_thread_and_wait_doc, "XXX TODO\n");

#define _ASYNC(n, a) _METHOD(_async, n, a)
#define _ASYNC_N(n) _METHOD(_async, n, METH_NOARGS)
#define _ASYNC_O(n) _METHOD(_async, n, METH_O)
#define _ASYNC_V(n) _METHOD(_async, n, METH_VARARGS)
PyMethodDef _async_methods[] = {
    _ASYNC_V(map),
    _ASYNC_N(run),
    _ASYNC_N(rdtsc),
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
    _ASYNC_N(is_parallel_thread),
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
    PyObject *m;

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
    Px_GUARD
    assert(ctx->pstate);
    assert(ctx->pstate != ctx->tstate);
#endif
    return ctx->pstate;
}

void
_Px_NewReference(PyObject *op)
{
    Px_GUARD_OBJ(op);
    Px_GUARD
    op->ob_refcnt = 1;
    Py_ASPX(op)->ctx = ctx;
    ctx->stats.newrefs++;
}

void
_Px_ForgetReference(PyObject *op)
{
    Px_GUARD_OBJ(op);
    Px_GUARD
    assert(Py_ASPX(op)->ctx == ctx);
    ctx->stats.forgetrefs++;
}

void
_Px_Dealloc(PyObject *op)
{
    Px_GUARD_OBJ(op);
    Px_GUARD
    assert(Py_ASPX(op)->ctx == ctx);
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
    Px_GUARD
    return Heap_Malloc(n);
}

void *
_PxMem_Realloc(void *p, size_t n)
{
    Px_GUARD_MEM(p);
    Px_GUARD
    return Heap_Realloc(p, n);
}

void
_PxMem_Free(void *p)
{
    Px_GUARD_MEM(p);
    Px_GUARD
    Heap_Free(p);
}

/*
unsigned long
_Px_MemorySignature(void *m)
{
    void *p, *pp[4], *p1, *p2, *p3, *p4;
    PyObject *next;
    PyObject *prev;
    PxState  *px;
    PyObject *rr[4], *r1, *r2, *r3, *r4;
    int signature;
    int i = 0;
    int x = 0;
    int y = 0;
    int s = 0;

    int is_px;
    int is_py;
    int is_corrupt;
    int is_unknown;

    if (_Px_MemorySignature_CallDepth == 1)
        return _MEMSIG_NOT_READY;
    assert(_Px_MemorySignature_CallDepth == 0);
    _Px_MemorySignature_CallDepth++;

    px = PXSTATE();
    if (!px)
        return _MEMSIG_NOT_READY;

    p1 = p = m;
    p2 = (void *)Px_PTR_ADD(p, 1);
    p3 = (void *)Px_PTR_ADD(p, 2);
    p4 = (void *)Px_PTR_ADD(p, 3);

    pp[0] = p1;
    pp[1] = p2;
    pp[2] = p3;
    pp[3] = p4;

    r1 = PyLong_FromVoidPtr((void *)Px_PAGE_ALIGN_DOWN(p1));
    r2 = PyLong_FromVoidPtr((void *)Px_PAGE_ALIGN_DOWN(p2));
    r3 = PyLong_FromVoidPtr((void *)Px_PAGE_ALIGN_DOWN(p3));
    r4 = PyLong_FromVoidPtr((void *)Px_PAGE_ALIGN_DOWN(p4));

    rr[0] = r1;
    rr[1] = r2;
    rr[2] = r3;
    rr[3] = r4;

    next = (PyObject *)p3;
    prev = (PyObject *)p4;

    is_py = (p1 == _Py_NOT_PARALLEL && p2 == _Py_NOT_PARALLEL);

    if (is_py) {
        assert(_PxPages_NEVER_SEEN(r3));
        assert(_PxPages_NEVER_SEEN(r4));
    }

    is_px = (
        p1 == _Py_IS_PARALLEL   &&
        _PxPages_IS_ACTIVE(r2)  && (
            (
                (
                    (next == NULL && (((PxObject *)p2)->ctx->ob_last == m))   ||
                    (next != NULL && next->is_px == _Py_IS_PARALLEL)
                ) && (
                    (prev == NULL && (((PxObject *)p2)->ctx->ob_first == m))  ||
                    (prev != NULL && prev->is_px == _Py_IS_PARALLEL)
                )
            ) && (
                (next == NULL || _PxPages_IS_ACTIVE(r3)) &&
                (prev == NULL || _PxPages_IS_ACTIVE(r4))
            )
        )
    );

    if (!is_px) {
        assert(p1 != _Py_IS_PARALLEL);
        assert(_PxPages_NEVER_SEEN(r2));
        assert(_PxPages_NEVER_SEEN(r3));
        assert(_PxPages_NEVER_SEEN(r4));
    }

    for (i = 0; i < 4; i++) {
        if (pp[i] == _Py_NOT_PARALLEL)
            y++;

        if (pp[i] == _Py_IS_PARALLEL)
            x++;

        if (_PxPages_SEEN(rr[i]))
            s++;
    }

    is_unknown = (y == 0 && x == 0 && s == 0);

    is_corrupt = (
        (!is_px && !is_py) && (
            (s != 0)                        ||
            (x > 0)                         ||
            (y > 0 && y < 4)                ||
            (y == 1 && p2 == NULL)          ||
            (p2 == _Py_IS_PARALLEL)         ||
            (p1 == _Py_IS_PARALLEL)
        )
    );

    assert(is_py || is_px || is_unknown || is_corrupt);

    if (is_py) {
        assert(!is_px);
        assert(!is_corrupt);
        assert(!is_unknown);
        signature = _MEMSIG_PY;
    } else if (is_px) {
        assert(!is_py);
        assert(!is_corrupt);
        assert(!is_unknown);
        signature = _MEMSIG_PX;
    } else if (is_corrupt) {
        assert(!is_px);
        assert(!is_py);
        assert(!is_unknown);
        signature = _MEMSIG_CORRUPT;
    } else {
        assert(is_unknown);
        assert(!is_px);
        assert(!is_py);
        assert(!is_corrupt);
        signature = _MEMSIG_UNKNOWN;
    }

    Py_DECREF(r1);
    Py_DECREF(r2);
    Py_DECREF(r3);
    Py_DECREF(r4);

    _Px_MemorySignature_CallDepth--;

    return signature;
}
unsigned long
_Px_ObjectSignature(void *m)
{
    void *p, *pp[4], *p1, *p2, *p3, *p4;
    PxObject *px;
    PyObject *next;
    PyObject *prev;
    PxState  *px;
    PyObject *r1, *r2, *r3, *r4;
    int signature;
    int i  = 0;
    int px = 0;
    int py = 0;
    int un = 0;

    int is_px_obj;
    int is_px_mem;
    int is_py_obj;
    int is_py_mem;
    int is_corrupt;
    int is_unknown;

    px = PXSTATE();

    p1 = p = m;
    p2 = ++p;
    p3 = ++p;
    p4 = ++p;

    pp[0] = p1;
    pp[1] = p2;
    pp[2] = p3;
    pp[3] = p4;

    r1 = PyLong_FromVoidPtr(Px_PAGE_ALIGN_DOWN(p1));
    r2 = PyLong_FromVoidPtr(Px_PAGE_ALIGN_DOWN(p2));
    r3 = PyLong_FromVoidPtr(Px_PAGE_ALIGN_DOWN(p3));
    r4 = PyLong_FromVoidPtr(Px_PAGE_ALIGN_DOWN(p4));

    px   = (PxObject *)p2;
    next = (PyObject *)p3;
    prev = (PyObject *)p4;

    is_py = (
        p1 == _Py_NOT_PARALLEL &&
        p2 == _Py_NOT_PARALLEL &&
        p3 == _Py_NOT_PARALLEL &&
        p4 == _Py_NOT_PARALLEL
    );

    is_px_obj = (
        p1 == _Py_IS_PARALLEL &&
        p2 != NULL &&
        p2 != _Py_IS_PARALLEL &&
        p2 != _Py_NOT_PARALLEL &&
        px->ctx != NULL && (
            (next == NULL && ((void *)px->ctx->ob_last == m))   ||
            (next != NULL && next->is_px == _Py_IS_PARALLEL)
        ) && (
            (prev == NULL && ((void *)px->ctx->ob_first == m))  ||
            (prev != NULL && prev->is_px == _Py_IS_PARALLEL)
        )
    );

    for (i = 0; i < 4; i++) {
        if (pp[i] == _Py_NOT_PARALLEL)
            py++;

        if (pp[i] == _Py_IS_PARALLEL)
            px++;
    }

    is_corrupt = (
        (px > 0)                        ||
        (py > 0 && py < 4)              ||
        (py == 1 && p2 == NULL)         ||
        (p2 == _Py_IS_PARALLEL)         ||
        (p1 == _Py_IS_PARALLEL && (
            (px->ctx == NULL)           ||
            (px->ctx != NULL && (
                (next == NULL && ((void *)px->ctx->ob_last != m))   ||
                (next != NULL && next->is_px != _Py_IS_PARALLEL)
            ) && (
                (prev == NULL && ((void *)px->ctx->ob_first != m))  ||
                (prev != NULL && prev->is_px != _Py_IS_PARALLEL)
            ))
        ))
    );

    is_unknown = (
        py == 0 && px == 0
    );

    if (is_py) {
        assert(!is_px);
        assert(!is_corrupt);
        assert(!is_unknown);
        signature = _MEMSIG_PY;
    } else if (is_px) {
        assert(!is_py);
        assert(!is_corrupt);
        assert(!is_unknown);
        signature = _MEMSIG_PY;
    } else if (is_corrupt) {
        assert(!is_px);
        assert(!is_py);
        assert(!is_unknown);
        signature = _MEMSIG_CORRUPT;
    } else {
        assert(is_unknown);
        assert(!is_px);
        assert(!is_py);
        assert(!is_corrupt);
        signature = _MEMSIG_UNKNOWN;
    }

    return signature;
}
*/

#ifdef __cpplus
}
#endif

/* vim:set ts=8 sw=4 sts=4 tw=78 et: */

