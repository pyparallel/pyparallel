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

PyAPI_DATA(long) Py_MainThreadId;
PyAPI_DATA(long) Py_MainProcessId;
PyAPI_DATA(long) Py_ParallelContextsEnabled;

/*
PyAPI_FUNC(PxList *)        PxList_New(void);

PyAPI_FUNC(void)            PxList_Clear(PxListHead *);
PyAPI_FUNC(void)            PxList_Delete(PxListHead *);
PyAPI_FUNC(void)            PxList_FreeList(PxListHead *);

PyAPI_FUNC(PxListItem *)    PxList_Push(PxListHead *head, PxListItem *item);
PyAPI_FUNC(PxListItem *)    PxList_Pop(PxListHead *head);

PyAPI_FUNC(void)            PxList_DeleteItem(PxListItem *item);
PyAPI_FUNC(void)            PxList_FreeListItem(PxListItem *item);

PyAPI_FUNC(PxListItem *)    PxList_Flush(PxListHead *head);
PyAPI_FUNC(unsigned short)  PxList_QueryDepth(PxListHead *head);

PyAPI_FUNC(PxListItem *)    PxList_PushList(PxListHead  *head,
                                            PxListItem  *start,
                                            PxListItem  *end,
                                            Py_ssize_t   count);
*/

/*
 * _PyParallel_Init() can be called safely multiple times.  It *must* be
 * called as early as possible, before any object allocations (and thus,
 * Py_INCREF/DECREF calls).  It is currently called in two places, Py_Main
 * and _PyInitializeEx_Private().  The latter is necessary for code that
 * embeds the interpreter (as Py_Main (probably) won't be called).
 */
PyAPI_FUNC(void) _PyParallel_Init(void);

PyAPI_FUNC(void) _PyParallel_CreatedGIL(void);
PyAPI_FUNC(void) _PyParallel_DestroyedGIL(void);
PyAPI_FUNC(void) _PyParallel_AboutToDropGIL(void);
PyAPI_FUNC(void) _PyParallel_JustAcquiredGIL(void);

PyAPI_FUNC(void) _PyParallel_ClearMainThreadId(void);
PyAPI_FUNC(void) _PyParallel_ClearMainProcessId(void);
PyAPI_FUNC(void) _PyParallel_RestoreMainProcessId(void);

PyAPI_FUNC(void) _PyParallel_EnableParallelContexts(void);
PyAPI_FUNC(void) _PyParallel_DisableParallelContexts(void);

PyAPI_FUNC(void) _PyParallel_BeginAllowThreads(void);
PyAPI_FUNC(void) _PyParallel_EndAllowThreads(void);

PyAPI_FUNC(void) _PyParallel_EnteredParallelContext(void *c);
PyAPI_FUNC(void) _PyParallel_LeavingParallelContext(void);

PyAPI_FUNC(void) _PyParallel_ContextGuardFailure(const char *function,
                                                 const char *filename,
                                                 int lineno,
                                                 int was_px_ctx);

#ifdef Py_DEBUG
static int
_Py_PXCTX(void)
{
    int active = (int)(Py_MainThreadId != _Py_get_current_thread_id());
    assert(Py_MainThreadId > 0);
    assert(Py_MainProcessId != -1);
    assert(Py_ParallelContextsEnabled != -1);
    if (Py_ParallelContextsEnabled)
        assert(active);
    else
        assert(!active);
    return active;
}
#define Py_PXCTX _Py_PXCTX()
#define Py_CTX (!_Py_PXCTX)
#define PY (Py_CTX)
#define PX Py_PXCTX
#else
#define Py_PXCTX (Py_MainThreadId != _Py_get_current_thread_id())
#endif /* Py_DEBUG */

#define Px_GUARD            \
    if (!Py_PXCTX)          \
        _PyParallel_ContextGuardFailure(__FUNCTION__, __FILE__, __LINE__, 1);

#define Py_GUARD            \
    if (Py_PXCTX)           \
        _PyParallel_ContextGuardFailure(__FUNCTION__, __FILE__, __LINE__, 0);

#define Px_RETURN(arg)      \
    if (Py_PXCTX)           \
        return (arg);

#define Px_VOID             \
    if (Py_PXCTX)           \
        return;

#define Px_RETURN_VOID(arg) \
    if (Py_PXCTX) {         \
        (arg);              \
        return;             \
    }

#define Px_RETURN_NULL      \
    if (Py_PXCTX)           \
        return NULL;

#else /* WITH_PARALLEL */
#define Py_GUARD
#define Px_GUARD
#define Px_VOID
#define Px_RETURN(a)
#define Px_RETURN_VOID(a)
#define Px_RETURN_NULL
#define Py_PXCTX 0
#define Py_CTX 0
#endif

#ifdef __cplusplus
}
#endif
#endif
/* vim:set ts=8 sw=4 sts=4 tw=78 et: */
