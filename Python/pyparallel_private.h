#ifndef PYPARALLEL_PRIVATE_H
#define PYPARALLEL_PRIVATE_H

#include <Windows.h>
#include "pyparallel.h"

#ifdef _WIN64
#define Px_ALIGN_SIZE 8U
#define Px_UINTPTR unsigned long long
#else
#define Px_ALIGN_SIZE 4U
#define Px_UINTPTR unsigned long
#endif
#define Px_CACHE_ALIGN_SIZE ((Px_UINTPTR)SYSTEM_CACHE_ALIGNMENT_SIZE)

#define Px_ALIGN(n) ( \
    (((Px_UINTPTR)n) + (((Px_UINTPTR)Px_ALIGN_SIZE)-1ULL)) & \
    ~(((Px_UINTPTR)Px_ALIGN_SIZE)-1ULL) \
)

#define xPx_CACHE_ALIGN(n) ( \
    (((Px_UINTPTR)n) + (((Px_UINTPTR)Px_CACHE_ALIGN_SIZE)-1ULL)) & \
    ~(((Px_UINTPTR)Px_CACHE_ALIGN_SIZE)-1ULL) \
)
#define Px_CACHE_ALIGN(n) ((n + 63ULL) & ~(63ULL))

#define Px_PTRADD(p, n)            \
        (void *)((Px_UINTPTR)(p) + \
                 (Px_UINTPTR)(n))

#define Px_DEFAULT_HEAP_SIZE (1024 * 1024) /* 1MB */

typedef struct _PyParallelHeap PyParallelHeap, Heap;
typedef struct _PyParallelContext PyParallelContext, Context;
typedef struct _PyParallelContextStats PyParallelContextStats, Stats;
typedef struct _PyParallelCallback PyParallelCallback, Callback;

typedef struct _PyParallelHeap {
    Heap   *sle_prev;
    Heap   *sle_next;
    void   *base;
    void   *next;
    size_t  mallocs;
    size_t  deallocs;
    size_t  reallocs;
    size_t  resizes;
    size_t  frees;
    size_t  size;
    size_t  allocated;
    size_t  remaining;
} PyParallelHeap, Heap;

typedef struct _PyParallelContextStats {
    unsigned __int64 start;
    unsigned __int64 end;
    double runtime;

    long thread_id;
    long process_id;

    int blocking_calls;

    size_t mallocs;
    size_t reallocs;
    size_t deallocs;
    size_t resizes;
    size_t frees;

    size_t newrefs;
    size_t forgetrefs;

    size_t heaps;

    size_t size;
    size_t allocated;
    size_t remaining;

    size_t objects;
    size_t varobjs;

} PyParallelContextStats, Stats;

typedef struct _PyParallelCallback {
    PyObject *func;
    PyObject *self;
    PyObject *args;
    PyObject **result;
    OVERLAPPED **overlapped;
} PyParallelCallback, Callback;

typedef struct _Object Object;

typedef struct _Object {
    Object   *next;
    PyObject *op;
} Object;

typedef struct _Objects {
    Object *first;
    Object *last;
} Objects;

static __inline
void
append_object(Objects *list, Object *o)
{
    register Object *n;
    if (!list->first) {
        list->first = o;
        list->last = o;
    } else {
        n = list->last;
        n->next = o;
        list->last = o;
    }
    o->next = 0;
}

typedef struct _PyParallelContext {
    HANDLE   heap_handle;
    Heap     heap;
    Stats    stats;
    Callback callback;

    Heap    *h;

    Objects objects;
    Objects varobjs;
} PyParallelContext, Context;

#endif /* PYPARALLEL_PRIVATE_H */
