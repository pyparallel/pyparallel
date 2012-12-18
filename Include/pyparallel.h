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

PyAPI_DATA(long) Py_MainProcessId;
PyAPI_DATA(long) Py_MainThreadId;

PyAPI_FUNC(void) _PyParallel_Init(void);

#ifdef Py_DEBUG
static int
_Py_PXCTX(void)
{
    assert(Py_MainThreadId != -1);
    assert(Py_MainThreadId == _Py_get_current_thread_id());
    return (Py_MainThreadId != _Py_get_current_thread_id());
}
#define Py_PXCTX _Py_PXCTX()
#else
#define Py_PXCTX (Py_MainThreadId != _Py_get_current_thread_id())
#endif /* Py_DEBUG */

#endif /* WITH_PARALLEL */

#ifdef __cplusplus
}
#endif
#endif
/* vim:set ts=8 sw=4 sts=4 tw=78 et: */
