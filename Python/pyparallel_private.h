#ifndef PYPARALLEL_PRIVATE_H
#define PYPARALLEL_PRIVATE_H

#include <Windows.h>
#include "pyparallel.h"

#ifdef _WIN64
#define Px_PTR_ALIGN_SIZE 8U
#define Px_UINTPTR unsigned long long
#else
#define Px_PTR_ALIGN_SIZE 4U
#define Px_UINTPTR unsigned long
#endif
#define Px_MEM_ALIGN_SIZE ((Px_UINTPTR)MEMORY_ALLOCATION_ALIGNMENT)
#define Px_PAGE_ALIGN_SIZE ((Px_UINTPTR)4096ULL)
#define Px_CACHE_ALIGN_SIZE ((Px_UINTPTR)SYSTEM_CACHE_ALIGNMENT_SIZE)

#define Px_ALIGN(n, a) (                                    \
    (((Px_UINTPTR)(n)) + (((Px_UINTPTR)(a))-1ULL)) &            \
    ~(((Px_UINTPTR)(a))-1ULL)                                 \
)

#define Px_PTR_ALIGN(n)     (Px_ALIGN((n), Px_PTR_ALIGN_SIZE))
#define Px_MEM_ALIGN(n)     (Px_ALIGN((n), Px_MEM_ALIGN_SIZE))
#define Px_CACHE_ALIGN(n)   (Px_ALIGN((n), Px_CACHE_ALIGN_SIZE))
#define Px_PAGE_ALIGN(n)    (Px_ALIGN((n), Px_PAGE_ALIGN_SIZE))

#define Px_PTRADD(p, n)     ((void *)((Px_UINTPTR)(p) + (Px_UINTPTR)(n)))

#define Px_ALIGNED_MALLOC(n)                                \
    (Py_PXCTX ? _PyHeap_Malloc(ctx, n, Px_MEM_ALIGN_SIZE) : \
                _aligned_malloc(n, MEMORY_ALLOCATION_ALIGNMENT))

#define Px_ALIGNED_FREE(n)                                  \
    (Py_PXCTX ? _PyHeap_Malloc(ctx, n, Px_MEM_ALIGN_SIZE) : \
                _aligned_malloc(n, MEMORY_ALLOCATION_ALIGNMENT))

#define Px_MAX(a, b) ((a > b) ? a : b)

#define Px_DEFAULT_HEAP_SIZE (Px_PAGE_ALIGN_SIZE) /* 4KB */
#define Px_MAX_SEM (32768)

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

typedef struct _PyParallelHeap {
    Heap   *sle_prev;
    Heap   *sle_next;
    void   *base;
    void   *next;
    size_t  last_alignment;
    size_t  mallocs;
    size_t  deallocs;
    size_t  reallocs;
    size_t  resizes;
    size_t  frees;
    size_t  size;
    size_t  allocated;
    size_t  remaining;
    size_t  alignment_mismatches;
    size_t  bytes_wasted;
} PyParallelHeap, Heap;

typedef struct _PyParallelContextStats {
    unsigned __int64 submitted;
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
    size_t alignment_mismatches;
    size_t bytes_wasted;

    size_t newrefs;
    size_t forgetrefs;

    size_t heaps;

    size_t size;
    size_t allocated;
    size_t remaining;

    size_t objects;
    size_t varobjs;

    size_t startup_size;
} PyParallelContextStats, Stats;

#define _PX_OTHER_FREES_SIZE 4
#define _PX_TMPBUF_SIZE 1024

typedef struct _PxState {
    PxListHead *errors;
    PxListHead *completed_callbacks;
    PxListHead *completed_errbacks;
    PxListHead *incoming;
    PxListHead *finished;
    //PxListHead *freelist;
    //PxListHead *singles;

    Context *ctx_first;
    Context *ctx_last;
    unsigned short ctx_minfree;
    unsigned short ctx_curfree;
    unsigned short ctx_maxfree;
    unsigned short ctx_ttl;

    HANDLE      wakeup;

    CRITICAL_SECTION cs;

    long long contexts_created;
    long long contexts_destroyed;
    long contexts_active;

    volatile long active;
    volatile long persistent;

    //__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
    volatile long long  submitted;
    volatile long       pending;
    volatile long       inflight;
    volatile long long  done;
    //volatile long long  failed;
    //volatile long long  succeeded;

    //__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
    volatile long long  io_submitted;
    volatile long       io_pending;
    volatile long       io_inflight;
    volatile long long  io_done;

    //__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
    volatile long long  sync_wait_submitted;
    volatile long       sync_wait_pending;
    volatile long       sync_wait_inflight;
    volatile long long  sync_wait_done;

    //__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
    volatile long long  sync_nowait_submitted;
    volatile long       sync_nowait_pending;
    volatile long       sync_nowait_inflight;
    volatile long long  sync_nowait_done;

    //__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
    long long last_done_count;
    long long last_submitted_count;

    long long last_sync_wait_done_count;
    long long last_sync_wait_submitted_count;

    long long last_sync_nowait_done_count;
    long long last_sync_nowait_submitted_count;

} PxState;

typedef struct _PyParallelContext {
    PyObject *func;
    PyObject *args;
    PyObject *kwds;
    PyObject *callback;
    PyObject *errback;
    PyObject *result;

    Context *prev;
    Context *next;

    PyThreadState *tstate;
    PyThreadState *pstate;

    PxState *px;

    PxListItem *error;
    PxListItem *callback_completed;
    PxListItem *errback_completed;

    PxListHead *outgoing;

    volatile long refcnt;

    HANDLE heap_handle;
    Heap   heap;
    Heap  *h;

    void  *instance;

    int disassociated;

    Stats  stats;

    Objects objects;
    Objects varobjs;

    char  tbuf[_PX_TMPBUF_SIZE];
    void *tbuf_base;
    void *tbuf_next;
    size_t tbuf_mallocs;
    size_t tbuf_allocated;
    size_t tbuf_remaining;
    size_t tbuf_bytes_wasted;
    size_t tbuf_last_alignment;
    size_t tbuf_alignment_mismatches;


    size_t leaked_bytes;
    size_t leak_count;
    void *last_leak;

    PyObject *errors_tuple;
    int hijacked_for_errors_tuple;
    size_t size_before_hijack;

    short ttl;

    long done;

    int times_finished;

} PyParallelContext, Context;

#endif /* PYPARALLEL_PRIVATE_H */
