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

#define Px_ALIGN(n) (                                              \
    (((Px_UINTPTR)n) + (((Px_UINTPTR)Px_ALIGN_SIZE)-1ULL)) &       \
    ~(((Px_UINTPTR)Px_ALIGN_SIZE)-1ULL)                            \
)

#define xPx_CACHE_ALIGN(n) (                                       \
    (((Px_UINTPTR)n) + (((Px_UINTPTR)Px_CACHE_ALIGN_SIZE)-1ULL)) & \
    ~(((Px_UINTPTR)Px_CACHE_ALIGN_SIZE)-1ULL)                      \
)
#define Px_CACHE_ALIGN(n) ((n + 63ULL) & ~(63ULL))

#define Px_PTRADD(p, n)                                            \
        (void *)((Px_UINTPTR)(p) +                                 \
                 (Px_UINTPTR)(n))

#define Px_DEFAULT_HEAP_SIZE (1024 * 1024) /* 1MB */

#include "pxlist.h"

typedef struct _cpuinfo {
    struct _core {
        int logical;
        int physical;
    } core;
    struct _cache {
        int l1;
        int l2;
    } cache;
} cpuinfo;

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


typedef struct _PyParallelHeap PyParallelHeap, Heap;
typedef struct _PyParallelContext PyParallelContext, Context;
typedef struct _PyParallelContextStats PyParallelContextStats, Stats;
typedef struct _PyParallelCallback PyParallelCallback, Callback;

typedef struct _PxState {
    PxListHead *errors;
    PxListHead *completed;
    PxListHead *incoming;
} PxState;

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

    size_t tbuf_mallocs;
    size_t tbuf_allocated;
    size_t tbuf_remaining;
} PyParallelContextStats, Stats;

#define _PX_TMPBUF_SIZE 1024

typedef struct _PyParallelContext {
    PyObject *func;
    PyObject *args;
    PyObject *kwds;
    PyObject *callback;
    PyObject *errback;
    PyObject *result;

    PyThreadState *tstate;

    PxListItem *work;
    PxListItem *error;
    PxListItem *completed;

    HANDLE heap_handle;
    Heap   heap;
    Heap  *h;

    Stats  stats;

    Objects objects;
    Objects varobjs;

    char *tbuf[_PX_TMPBUF_SIZE];
    void *tbuf_base;
    void *tbuf_next;
    size_t tbuf_mallocs;
    size_t tbuf_allocated;
    size_t tbuf_remaining;

} PyParallelContext, Context;

/*
typedef struct _PxInterlockedList {
    SLIST_ENTRY  entry;
    void        *p;
} PxInterlockedList;

static __inline
void
PxInterlockedList_Init(PxInterlockedList *l)
{

    l = (PxInterlockedList *)_aligned_malloc(
        sizeof(PxInterlockedList),
        MEMORY_ALLOCATION_ALIGNMENT
    );
    if (!l)
        Py_FatalError("PxInterlockedList_Init:_aligned_malloc");
    InitializeSListHead(l);
}

static __inline
void
PxInterlockedList_Init(PxInterlockedList *l)
{

    l = (PxInterlockedList *)_aligned_malloc(
        sizeof(PxInterlockedList),
        MEMORY_ALLOCATION_ALIGNMENT
    );
    if (!l)
        Py_FatalError("PxInterlockedList_Init:_aligned_malloc");
    InitializeSListHead(l);
}

static __inline
void *
_PxThreadHeap_SysAlignedMalloc(HANDLE h, size_t n)
{
    register size_t aligned = (
        (n + (MEMORY_ALLOCATION_ALIGNMENT-1)) &
        ~(MEMORY_ALLOCATION_ALIGNMENT-1)
    );
    register void *p = HeapAlloc(h, 0, aligned);
    if (!p)
        Py_FatalError("_PxThreadHeap_SysAlignedMalloc:HeapAlloc");

}
*/

/*
typedef struct _pxis {

} PxInterpreterState;

typedef struct _pxts {

} PxThreadState;
*/

#endif /* PYPARALLEL_PRIVATE_H */
