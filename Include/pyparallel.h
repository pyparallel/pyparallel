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
#else
#define Py_PXCTX (Py_MainThreadId != _Py_get_current_thread_id())
#endif /* Py_DEBUG */

#define Py_PYCTX              \
     (Py_MainThreadId <= 0 || \
      Py_MainThreadId == _Py_get_current_thread_id())

#endif /* WITH_PARALLEL */

#ifdef __cplusplus
}
#endif
#endif
/* vim:set ts=8 sw=4 sts=4 tw=78 et: */
