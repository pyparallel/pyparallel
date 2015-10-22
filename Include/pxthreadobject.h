/* PxThread.  Copyright 2015, Trent Nelson <trent@trent.me> */
#ifndef PXTHREADOBJECT_H
#define PXTHREADOBJECT_H

#ifdef __cpplus
extern "C" {
#endif

#include "Python.h"
#include "../Python/pyparallel_private.h"
#include <Windows.h>
#include <Avrt.h>
#include "../contrib/windows/_avrt.h"

#define PxThread_FATAL() do {                                                  \
    assert(PyErr_Occurred());                                                  \
    PxContext_HandleException(c, "", 1);                                       \
    goto end;                                                                  \
} while (0)


#define PxThread_EXCEPTION() do {                                              \
    assert(PyErr_Occurred());                                                  \
    PxContext_HandleException(c, "", 1);                                       \
    goto end;                                                                  \
} while (0)

#define PxThread_SYSERROR(n) do {                                              \
    PyErr_SetFromWindowsErr(0);                                                \
    PxContext_HandleException(c, n, 1);                                        \
    goto end;                                                                  \
} while (0)

#define Px_THREADFLAGS(t) (((PxThreadObject *)(t))->flags)
#define Px_THREADFLAGS_VALID                             (1)
#define Px_THREADFLAGS_START_REQUESTED                   (1ULL <<  1)
#define Px_THREADFLAGS_STARTED                           (1ULL <<  2)
#define Px_THREADFLAGS_STOP_REQUESTED                    (1ULL <<  3)
#define Px_THREADFLAGS_STOPPED                           (1ULL <<  4)
#define Px_THREADFLAGS_RUNNING                           (1ULL <<  5)
#define Px_THREADFLAGS_SHUTDOWN_REQUESTED                (1ULL <<  6)
#define Px_THREADFLAGS_SHUTDOWN                          (1ULL <<  7)
#define Px_THREADFLAGS_CLEANED_UP                        (1ULL <<  8)
#define Px_THREADFLAGS_                                  (1ULL << 63)

#define PxThread_VALID(t)                                                      \
    (Px_THREADFLAGS(t) & Px_THREADFLAGS_VALID)

#define PxThread_START_REQUESTED(t)                                            \
    (Px_THREADFLAGS(t) & Px_THREADFLAGS_START_REQUESTED)

#define PxThread_STARTED(t)                                                    \
    (Px_THREADFLAGS(t) & Px_THREADFLAGS_STARTED)

#define PxThread_STOP_REQUESTED(t)                                             \
    (Px_THREADFLAGS(t) & Px_THREADFLAGS_STOP_REQUESTED)

#define PxThread_STOPPED(t)                                                    \
    (Px_THREADFLAGS(t) & Px_THREADFLAGS_STOPPED)

#define PxThread_RUNNING(t)                                                    \
    (Px_THREADFLAGS(t) & Px_THREADFLAGS_RUNNING)

#define PxThread_SHUTDOWN_REQUESTED(t)                                         \
    (Px_THREADFLAGS(t) & Px_THREADFLAGS_SHUTDOWN_REQUESTED)

#define PxThread_CLEANED_UP(t)                                                 \
    (Px_THREADFLAGS(t) & Px_THREADFLAGS_CLEANED_UP)

#define PxThread_SHUTDOWN(t)                                                   \
    (Px_THREADFLAGS(t) & Px_THREADFLAGS_SHUTDOWN)

#define PxThread_XSET(thread, flag)                                            \
    InterlockedExchange(thread->flags, thread->flags | flag)

#define PxThread_XUNSET(thread, flag)                                          \
    InterlockedExchange(thread->flags, thread->flags & ~flag)

#define PxThread_SET_FLAG(thread, flag)   Px_THREADFLAGS(t) |= flag
#define PxThread_UNSET_FLAG(thread, flag) Px_THREADFLAGS(t) &= ~flag

#define PxThread_SET_VALID(t)                                                  \
    PxThread_SET_FLAG(t, Px_THREADFLAGS_VALID)

#define PxThread_SET_START_REQUESTED(t)                                        \
    PxThread_SET_FLAG(t, Px_THREADFLAGS_START_REQUESTED)

#define PxThread_UNSET_START_REQUESTED(t)                                      \
    PxThread_UNSET_FLAG(t, Px_THREADFLAGS_START_REQUESTED)

#define PxThread_SET_STARTED(t)                                                \
    PxThread_SET_FLAG(t, Px_THREADFLAGS_STARTED)

#define PxThread_SET_STOPPED(t)                                                \
    PxThread_SET_FLAG(t, Px_THREADFLAGS_STOPPED)

#define PxThread_SET_STOP_REQUESTED(t)                                         \
    PxThread_SET_FLAG(t, Px_THREADFLAGS_STOP_REQUESTED)

#define PxThread_UNSET_STOP_REQUESTED(t)                                       \
    PxThread_UNSET_FLAG(t, Px_THREADFLAGS_STOP_REQUESTED)

#define PxThread_SET_SHUTDOWN(t)                                               \
    PxThread_SET_FLAG(t, Px_THREADFLAGS_SHUTDOWN)

#define PxThread_SET_SHUTDOWN_REQUESTED(t)                                     \
    PxThread_SET_FLAG(t, Px_THREADFLAGS_SHUTDOWN_REQUESTED)

#define PxThread_SET_STOPPED(t)                                                \
    PxThread_SET_FLAG(t, Px_THREADFLAGS_STOPPED)

#define PxThread_SET_RUNNING(t)                                                \
    PxThread_SET_FLAG(t, Px_THREADFLAGS_RUNNING)

#define PxThread_UNSET_RUNNING(t)                                              \
    PxThread_UNSET_FLAG(t, Px_THREADFLAGS_RUNNING)

#define PxThread_SET_CLEANED_UP(t)                                             \
    PxThread_SET_FLAG(t, Px_THREADFLAGS_CLEANED_UP)

typedef struct _PxThreadObject {
    PyObject_HEAD
    Context *ctx;
    Heap  heap;
    Heap  snapshot;
    Heap *last_ctx_heap;
    Heap *last_thread_heap;
    volatile LONGLONG count;
    volatile LONG count_wrapped; /* this isn't a good name */
    volatile LONG flags;
    FILETIME creationtime;
    FILETIME exittime;
    FILETIME kerneltime;
    FILETIME usertime;
    SRWLOCK times_srwlock;
    DWORD thread_id;
    LPTHREAD_START_ROUTINE start_address;
    LPVOID start_parameter;
    HANDLE iocp;
    HANDLE avrt_handle;
    HANDLE thread_handle;
    DWORD task_index;
    union {
        TASK_ID task_id;
        LPCWSTR task_name;
        struct {
            TASK_ID first_task_id;
            LPCWSTR first_task_name;
            TASK_ID second_task_id;
            LPCWSTR second_task_name;
        };
    };
    AVRT_PRIORITY priority;
    ULONG system_responsiveness;
    DWORD interval;
    LIST_ENTRY px_link;
    CRITICAL_SECTION cs;
    volatile LONG cs_contention;
    PyObject *data;
    INIT_ONCE start_once;
    INIT_ONCE shutdown_once;
    PTP_WORK shutdown_work;
    SRWLOCK data_srwlock;
    LARGE_INTEGER duration;
    SRWLOCK duration_srwlock;
    HANDLE shutdown_event;
} PxThreadObject;

PyAPI_FUNC(int) PxThread_Valid(PxThreadObject *t);

PyAPI_FUNC(PxThreadObject *) PxThread_New();
PyAPI_FUNC(int) PxThread_Start(PxThreadObject *t);
PyAPI_FUNC(int) PxThread_Resume(PxThreadObject *t);
PyAPI_FUNC(int) PxThread_Suspend(PxThreadObject *t);
PyAPI_FUNC(int) PxThread_Shutdown(PxThreadObject *t);
PyAPI_FUNC(int) PxThread_UpdateTimes(PxThreadObject *t);

int pxthread_set_data(PxThreadObject *t, PyObject *data, void *closure);
PyObject *pxthread_get_data(PxThreadObject *t, void *closure);

PyObject *pxthread_start(PyObject *self);
PyObject *pxthread_resume(PyObject *self);
PyObject *pxthread_suspend(PyObject *self);
PyObject *pxthread_shutdown(PyObject *self);

void pxthread_dealloc(PxThreadObject *self);

DWORD WINAPI PxThread_Main(LPVOID param);

#ifdef __cpplus
}
#endif

#endif /* PXTHREADOBJECT_H */

/* vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                  */
