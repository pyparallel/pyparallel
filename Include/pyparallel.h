#ifndef Py_PARALLEL_H
#define Py_PARALLEL_H
#ifdef __cplusplus
extern "C" {
#endif

#ifdef WITH_PARALLEL

#ifndef WITH_INTRINSICS
#define WITH_INTRINSICS 1
#endif
#include "pyintrinsics.h"

#define Py_PXFLAGS(o)   (((PyObject *)(o))->px_flags)

#define Py_PXFLAGS_INVALID              (0)
#define Py_PXFLAGS_ISPY                 (1)
#define Py_PXFLAGS_ISPX                 (1UL <<  1)
#define Py_PXFLAGS_RWLOCK               (1UL <<  2)
#define Py_PXFLAGS_EVENT                (1UL <<  3)
#define Py_PXFLAGS_WASPX                (1UL <<  4)
#define Py_PXFLAGS_PERSISTED            (1UL <<  5)
#define Py_PXFLAGS_PROMOTED             (1UL <<  6)
#define Py_PXFLAGS_CLONED               (1UL <<  7)
#define Py_PXFLAGS_CV_WAITERS           (1UL <<  8)
#define Py_PXFLAGS_MIMIC                (1UL <<  9)
#define Py_PXFLAGS_DEALLOC              (1UL << 10)

#define Py_HAS_RWLOCK(o)    (Py_PXFLAGS((o)) & Py_PXFLAGS_RWLOCK)
#define Py_HAS_EVENT(o)     (Py_PXFLAGS((o)) & Py_PXFLAGS_EVENT)
#define Py_WASPX(o)         (Py_PXFLAGS((o)) & Py_PXFLAGS_WASPX)

#define Px_ISPY(o)          (Py_PXFLAGS((o)) & Py_PXFLAGS_ISPY)
#define Px_ISPX(o)          (Py_PXFLAGS((o)) & Py_PXFLAGS_ISPX)
#define Px_WASPX(o)         (Py_PXFLAGS((o)) & Py_PXFLAGS_WASPX)
#define Px_PERSISTED(o)     (Py_PXFLAGS((o)) & Py_PXFLAGS_PERSISTED)
#define Px_PROMOTED(o)      (Py_PXFLAGS((o)) & Py_PXFLAGS_PROMOTED)
#define Px_CLONED(o)        (Py_PXFLAGS((o)) & Py_PXFLAGS_CLONED)
#define Px_CV_WAITERS(o)    (Py_PXFLAGS((o)) & Py_PXFLAGS_CV_WAITERS)
#define Px_ISMIMIC(o)       (Py_PXFLAGS((o)) & Py_PXFLAGS_MIMIC)

#define Px_ISPROTECTED(o)   (Py_PXFLAGS((o)) & Py_PXFLAGS_RWLOCK)

#define Px_DEALLOC(o) (                                 \
    (Py_TYPE((o))->tp_flags & Py_TPFLAGS_PX_DEALLOC) || \
    (Py_PXFLAGS((o)) & Py_PXFLAGS_DEALLOC)              \
)

PyAPI_DATA(long) Py_MainThreadId;
PyAPI_DATA(long) Py_MainProcessId;
PyAPI_DATA(long) Py_ParallelContextsEnabled;

PyAPI_FUNC(void)       _PyParallel_IncRef(void *);
PyAPI_FUNC(void)       _PyParallel_DecRef(void *);
PyAPI_FUNC(Py_ssize_t) _PyParallel_RefCnt(void *);

PyAPI_FUNC(void) _PyParallel_Init(void);
PyAPI_FUNC(void) _PyParallel_Finalize(void);
PyAPI_FUNC(void) _PyParallel_BlockingCall(void);
PyAPI_FUNC(int)  _PyParallel_IsFinalized(void);

PyAPI_FUNC(void) _PyParallel_CreatedGIL(void);
PyAPI_FUNC(void) _PyParallel_DestroyedGIL(void);
PyAPI_FUNC(void) _PyParallel_AboutToDropGIL(void);
PyAPI_FUNC(void) _PyParallel_JustAcquiredGIL(void);

PyAPI_FUNC(void) _PyParallel_ClearMainThreadId(void);
PyAPI_FUNC(void) _PyParallel_ClearMainProcessId(void);
PyAPI_FUNC(void) _PyParallel_RestoreMainProcessId(void);

PyAPI_FUNC(void) _PyParallel_EnableParallelContexts(void);
PyAPI_FUNC(void) _PyParallel_DisableParallelContexts(void);

PyAPI_FUNC(int)  _PyParallel_IsTLSHeapActive(void);
PyAPI_FUNC(int)  _PyParallel_GetTLSHeapDepth(void);
PyAPI_FUNC(void) _PyParallel_EnableTLSHeap(void);
PyAPI_FUNC(void) _PyParallel_DisableTLSHeap(void);

PyAPI_FUNC(int) _PyParallel_IsHeapOverrideActive(void);

PyAPI_FUNC(int)  _PyParallel_DoesContextHaveActiveHeapSnapshot(void);

PyAPI_FUNC(void) _PyParallel_BeginAllowThreads(void);
PyAPI_FUNC(void) _PyParallel_EndAllowThreads(void);

PyAPI_FUNC(void) _PyParallel_EnteredParallelContext(void *c);
PyAPI_FUNC(void) _PyParallel_LeavingParallelContext(void);

PyAPI_FUNC(void) _PyParallel_SchedulePyNoneDecref(long);

PyAPI_FUNC(int)  _PyParallel_ExecutingCallbackFromMainThread(void);

PyAPI_FUNC(void) _PyParallel_ContextGuardFailure(const char *function,
                                                 const char *filename,
                                                 int lineno,
                                                 int was_px_ctx);

PyAPI_FUNC(int) _PyParallel_Guard(const char *function,
                                  const char *filename,
                                  int lineno,
                                  void *m,
                                  unsigned int flags);

PyAPI_FUNC(int)     _Px_TEST(void *p);
PyAPI_FUNC(int)     _Py_ISPY(void *ob);

PyAPI_FUNC(void)    _PyParallel_SetDebugbreakOnNextException(void);
PyAPI_FUNC(void)    _PyParallel_ClearDebugbreakOnNextException(void);
PyAPI_FUNC(int)     _PyParallel_IsDebugbreakOnNextExceptionSet(void);

PyAPI_FUNC(int)     _PyParallel_IsParallelContext(void);

PyAPI_FUNC(int)     PyPx_EnableTLSHeap(void);
PyAPI_FUNC(int)     PyPx_DisableTLSHeap(void);

#define PyExc_MAYBE_BREAK() \
    do { \
        if (_PyParallel_IsDebugbreakOnNextExceptionSet()) { \
            __debugbreak(); \
            _PyParallel_ClearDebugbreakOnNextException(); \
        } \
    } while (0)

static __inline
int
__PyParallel_IsParallelContext(void)
{
    return (Py_MainThreadId != _Py_get_current_thread_id());
}

#ifdef Py_DEBUG
#define Py_PXCTX() (_PyParallel_IsParallelContext())
#else
#define Py_PXCTX() (__PyParallel_IsParallelContext())
#endif

#define Py_PX(ob)   ((((PyObject*)(ob))->px))

/* There are a couple of places at the moment within the non-PyParallel parts
 * of CPython (such as ceval.c and unicodeobject.c) where we've put in custom
 * bits of code to handle certain parallel context scenarios as a stop-gap
 * measure.  For example: the dodgy hack we made to insertdict() in dictobject.c
 * in order to support persisting memory/objects after a parallel context
 * returns.
 *
 * In other words, the current hacks should be formalized into proper API
 * calls, or, alternatively, the underlying structures that are being returned
 * via the void pointer (e.g. PyParallelContext) should be made public to the
 * rest of the interpreter.  Or some combination of both.  Either way, we
 * wouldn't be returning opaque pointers to the structs of PyParallel's
 * innards.
 *
 * But eh, it works for now.
 */

/* Returns the HANDLE to the active heap override for the current parallel
 * context.  Should only be called after _PyParallel_IsHeapOverrideActive()
 * is checked.  Or rather, behavior when the heap override isn't active is
 * undefined. */
PyAPI_FUNC(void *) _PyParallel_GetHeapOverride(void);

/* Update: switched dictobject.c to use the method below instead. */
PyAPI_FUNC(void)   _PyParallel_MaybeFreeObject(void *);

/* Returns a pointer to the active PyParallelContext, which will only have
 * a value if the parallel thread has been initialized via the threadpool
 * callback mechanics (in pyparallel.c).  This provides an alternate way
 * to check if we're in a parallel context (instead of using Py_PXCTX()/
 * _PyParallel_IsParallelContext()).
 *
 * The only place you would ever need to use this method instead of Py_PXCTX()
 * is if you could potentially be called from a thread that isn't holding a
 * GIL.
 *
 * The only place this is *currently* being used is PyEval_AcquireThread(),
 * by way of the Py_GUARD_AGAINST_PX_ONLY() macro (described below). */
PyAPI_FUNC(void *) _PyParallel_GetActiveContext(void);

#define Px_GUARD()                       \
    if (!Py_PXCTX())                     \
        _PyParallel_ContextGuardFailure( \
            __FUNCTION__,                \
            __FILE__,                    \
            __LINE__,                    \
            1                            \
        );

#define Py_GUARD()                       \
    if (Py_PXCTX())                      \
        _PyParallel_ContextGuardFailure( \
            __FUNCTION__,                \
            __FILE__,                    \
            __LINE__,                    \
            0                            \
        );

/* The following specialization of Py_GUARD is intended to be called from
   the functions in ceval.c (like PyEval_AcquireThread) that could be called
   from a thread that doesn't hold the GIL (which will appear as a parallel
   thread, as Py_PXCTX() only tests (main thread id == current thread id)). */
#define Py_GUARD_AGAINST_PX_ONLY()                      \
    do {                                                \
        if (_PyParallel_GetActiveContext() != NULL) {   \
            assert(tstate->is_parallel_thread);         \
            assert(tstate->px);                         \
            _PyParallel_ContextGuardFailure(            \
                __FUNCTION__,                           \
                __FILE__,                               \
                __LINE__,                               \
                0                                       \
            );                                          \
        }                                               \
    } while (0)


/* PY tests should be odd, PX tests even. */
/* Object guards should be less than mem guards. */
#define _PYOBJ_TEST     (1UL <<  1)
#define _PXOBJ_TEST     (1UL <<  2)
/* Note the absence of (1UL << 3) (to maintain odd/even) */
#define _PY_ISPX_TEST   (1UL <<  4)
#define _PYOBJ_GUARD    (1UL <<  5)
#define _PXOBJ_GUARD    (1UL <<  6)

#define _PYMEM_TEST     (1UL <<  7)
#define _PXMEM_TEST     (1UL <<  8)
#define _PYMEM_GUARD    (1UL <<  9)
#define _PXMEM_GUARD    (1UL << 10)

#ifdef _WIN64
#define _px_bitscan_fwd        _BitScanForward64
#define _px_bitscan_rev        _BitScanReverse64
#define _px_interlocked_or     _InterlockedOr64
#define _px_interlocked_and    _InterlockedAnd64
#define _px_popcnt             _Py_popcnt_u64
#else
#define _px_bitscan_fwd        _BitScanForward
#define _px_bitscan_rev        _BitScanReverse
#define _px_interlocked_or     _InterlockedOr
#define _px_interlocked_and    _InterlockedAnd
#define _px_popcnt             _Py_popcnt_u32
#endif

static __inline
int
_px_bitpos_uint32(unsigned int f)
{
    unsigned long i = 0;
    _px_bitscan_fwd(&i, f);
    return i;
}

#define _ONLY_ONE_BIT(f) _Py_UINT32_BITS_SET(f)
#define _OBJTEST(f) (_ONLY_ONE_BIT(f) && f >= _PYOBJ_TEST && f <= _PXOBJ_GUARD)
#define _MEMTEST(f) (_ONLY_ONE_BIT(f) && f >= _PYMEM_TEST && f <= _PXMEM_GUARD)
#define _PYTEST(f)  (_ONLY_ONE_BIT(f) &&  (_px_bitpos_uint32(f) % 2))
#define _PXTEST(f)  (_ONLY_ONE_BIT(f) && !(_px_bitpos_uint32(f) % 2))

#if defined(Py_DEBUG) && 0
/* Erm, disabling these for now, they don't appear to be working. */
#define Py_ISPY(pointer)           \
    _PyParallel_Guard(             \
        __FUNCTION__,              \
        __FILE__,                  \
        __LINE__,                  \
        pointer,                   \
        _PYOBJ_TEST                \
    )

#define Py_ISPX(pointer)           \
    _PyParallel_Guard(             \
        __FUNCTION__,              \
        __FILE__,                  \
        __LINE__,                  \
        pointer,                   \
        _PY_ISPX_TEST              \
    )
#else
#define Py_ISPY(op) (                         \
    (((void *)(((PyObject *)(op))->is_px)) == \
     ((void *)(_Py_NOT_PARALLEL)))            \
)
#define Py_ISPX(op) (                         \
    (((void *)(((PyObject *)(op))->is_px)) == \
     ((void *)(_Py_IS_PARALLEL)))             \
)
#endif

#define Py_TEST_OBJ(m)             \
    _PyParallel_Guard(             \
        __FUNCTION__,              \
        __FILE__,                  \
        __LINE__,                  \
        m,                         \
        _PYOBJ_TEST                \
    )
#define Py_PYOBJ(m) Py_TEST_OBJ(m)

#define Px_TEST_OBJ(m)             \
    _PyParallel_Guard(             \
        __FUNCTION__,              \
        __FILE__,                  \
        __LINE__,                  \
        m,                         \
        _PXOBJ_TEST                \
    )
#define Py_PXOBJ(m) Px_TEST_OBJ(m)

#define Py_GUARD_OBJ(m)            \
    _PyParallel_Guard(             \
        __FUNCTION__,              \
        __FILE__,                  \
        __LINE__,                  \
        m,                         \
        _PYOBJ_GUARD               \
    )

#define Px_GUARD_OBJ(m)            \
    _PyParallel_Guard(             \
        __FUNCTION__,              \
        __FILE__,                  \
        __LINE__,                  \
        m,                         \
        _PXOBJ_GUARD               \
    )

#define Py_TEST_MEM(m)             \
    _PyParallel_Guard(             \
        __FUNCTION__,              \
        __FILE__,                  \
        __LINE__,                  \
        m,                         \
        _PYMEM_TEST                \
    )

#define Px_TEST_MEM(m)             \
    _PyParallel_Guard(             \
        __FUNCTION__,              \
        __FILE__,                  \
        __LINE__,                  \
        m,                         \
        _PXMEM_TEST                \
    )

#define Py_GUARD_MEM(m)            \
    _PyParallel_Guard(             \
        __FUNCTION__,              \
        __FILE__,                  \
        __LINE__,                  \
        m,                         \
        _PYMEM_GUARD               \
    )

#define Px_GUARD_MEM(m)            \
    _PyParallel_Guard(             \
        __FUNCTION__,              \
        __FILE__,                  \
        __LINE__,                  \
        m,                         \
        _PXMEM_GUARD               \
    )

#define PyPx_GUARD_OBJ(o)          \
    do {                           \
        if (Py_PXCTX())            \
            Px_GUARD_OBJ(o);       \
        else                       \
            Py_GUARD_OBJ(o);       \
    } while (0)

#define PyPx_GUARD_MEM(m)          \
    do {                           \
        if (Py_PXCTX())            \
            Px_GUARD_MEM(m);       \
        else                       \
            Py_GUARD_MEM(m);       \
} while (0)

#define Px_BREAK()                 \
    if (Py_PXCTX())                \
        break

#define Px_RETURN(arg)             \
    do {                           \
        if (Py_PXCTX())            \
            return (arg);          \
    } while (0)

#define Px_VOID()                  \
    if (Py_PXCTX())                \
        return

#define Px_RETURN_VOID(arg)        \
    do {                           \
        if (Py_PXCTX()) {          \
            (arg);                 \
            return;                \
        }                          \
    } while (0)

#define Px_RETURN_NULL()           \
    if (Py_PXCTX())                \
        return NULL

#define Px_RETURN_OP(op, arg)      \
    if (Py_ISPX(op))               \
        return (arg)

#define Px_VOID_OP(op)             \
    if (Py_ISPX(op))               \
        return

#define Px_RETURN_VOID_OP(op, arg) \
    do {                           \
        if (Py_ISPX(op)) {         \
            (arg);                 \
            return;                \
        }                          \
    } while (0)

#define Px_RETURN_NULL_OP(op)      \
    if (Py_ISPX(op))               \
        return NULL

#define Px_CLEARFREELIST()         \
    do {                           \
        if (Py_PXCTX()) {          \
            numfree = 0;           \
            return ret;            \
        }                          \
    } while (0)

#else /* WITH_PARALLEL */
#define Py_GUARD()
#define Py_GUARD_AGAINST_PX_ONLY()
#define Px_GUARD()
#define Py_GUARD_OBJ(o)
#define Py_GUARD_MEM(o)
#define PyPx_GUARD_OBJ(o)
#define PyPx_GUARD_MEM(o)
#define Px_BREAK()
#define Px_VOID
#define Px_RETURN(a)
#define Px_RETURN_VOID(a)
#define Px_RETURN_NULL
#define Px_VOID_OP(op)
#define Px_RETURN_OP(op, arg)
#define Px_RETURN_VOID_OP(op, arg)
#define Px_RETURN_NULL_OP(op)
#define Py_PXCTX() 0
#define Py_CTX 0
#define Py_ISPX(o) 0
#define Py_ISPY(o) 1

#define _PyParallel_EnableTLSHeap()
#define _PyParallel_DisableTLSHeap()

#endif

#ifdef __cplusplus
}
#endif
#endif

