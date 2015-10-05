#ifndef PYPARALLEL_PRIVATE_H
#define PYPARALLEL_PRIVATE_H

#ifdef __cpplus
extern "C" {
#endif

#ifndef UNICODE
#define UNICODE
#endif

#include "../Modules/socketmodule.h"
#include <Windows.h>
//#include <ntddk.h>
#include "pyparallel.h"
#include "pyparallel_odbc.h"
#include "pyparallel_util.h"
#include "../contrib/pxodbc/src/pxodbccapsule.h"
#include "pyparallel_http.h"

#pragma comment(lib, "ws2_32.lib")
//#pragma comment(lib, "odbc32.lib")
//#pragma comment(lib, "odbccp32.lib")

#if defined(_MSC_VER) && _MSC_VER>1201
  /* Do not include addrinfo.h for MSVC7 or greater. 'addrinfo' and
   * EAI_* constants are defined in (the already included) ws2tcpip.h.
   */
#else
#  include "../Modules/addrinfo.h"
#endif


#ifdef _WIN64
#define Px_PTR_ALIGN_SIZE 8U
#define Px_PTR_ALIGN_RAW 8
#define Px_UINTPTR unsigned long long
#define Px_INTPTR long long
#define Px_UINTPTR_1 (1ULL)
#define Px_INTPTR_BITS 64
#define Px_LARGE_PAGE_SIZE 2 * 1024 * 1024 /* 2MB on x64 */
#else
#define Px_LARGE_PAGE_SIZE 4 * 1024 * 1024 /* 4MB on x86 */
#define Px_PTR_ALIGN_SIZE 4U
#define Px_PTR_ALIGN_RAW 4
#define Px_UINTPTR unsigned long
#define Px_INTPTR long
#define Px_INTPTR_BITS 32
#define Px_UINTPTR_1 (1UL)
#endif
#define Px_PAGE_SIZE (4096)
#define Px_SMALL_PAGE_SIZE Px_PAGE_SIZE
#define Px_PAGE_SHIFT 12ULL
#define Px_MEM_ALIGN_RAW MEMORY_ALLOCATION_ALIGNMENT
#define Px_MEM_ALIGN_SIZE ((Px_UINTPTR)MEMORY_ALLOCATION_ALIGNMENT)
#define Px_PAGE_ALIGN_SIZE ((Px_UINTPTR)Px_PAGE_SIZE)
#define Px_CACHE_ALIGN_SIZE ((Px_UINTPTR)SYSTEM_CACHE_ALIGNMENT_SIZE)

#define Px_ALIGN(n, a) (                             \
    (((Px_UINTPTR)(n)) + (((Px_UINTPTR)(a))-1ULL)) & \
    ~(((Px_UINTPTR)(a))-1ULL)                        \
)

#define Px_ALIGN_DOWN(n, a) (                        \
    (((Px_UINTPTR)(n)) & (-((Px_INTPTR)(a))))        \
)

#define Px_PTR_ALIGN(n)         (Px_ALIGN((n), Px_PTR_ALIGN_SIZE))
#define Px_MEM_ALIGN(n)         (Px_ALIGN((n), Px_MEM_ALIGN_SIZE))
#define Px_CACHE_ALIGN(n)       (Px_ALIGN((n), Px_CACHE_ALIGN_SIZE))
#define Px_PAGE_ALIGN(n)        (Px_ALIGN((n), Px_PAGE_ALIGN_SIZE))
#define Px_PAGE_ALIGN_DOWN(n)   (Px_ALIGN_DOWN((n), Px_PAGE_ALIGN_SIZE))

#define Px_PAGESIZE_ALIGN_UP(n, s)      (Px_ALIGN((n), s))
#define Px_PAGESIZE_ALIGN_DOWN(n, s)    (Px_ALIGN_DOWN((n), s))

#define Px_PTR(p)           ((Px_UINTPTR)(p))
#define Px_PTR_ADD(p, n)    ((void *)((Px_PTR(p)) + (Px_PTR(n))))

#define Px_PTR_ALIGNED_ADD(p, n) \
    (Px_PTR_ALIGN(Px_PTR_ADD(p, Px_PTR_ALIGN(n))))

#define Px_ALIGNED_MALLOC(n)                                  \
    (Py_PXCTX() ? _PxHeap_Malloc(ctx, n, Px_MEM_ALIGN_SIZE) : \
                _aligned_malloc(n, MEMORY_ALLOCATION_ALIGNMENT))

#define Px_ALIGNED_FREE(n)                                    \
    (Py_PXCTX() ? _PxHeap_Malloc(ctx, n, Px_MEM_ALIGN_SIZE) : \
                _aligned_malloc(n, MEMORY_ALLOCATION_ALIGNMENT))

#define Px_MAX(a, b) ((a > b) ? a : b)

#define Px_DEFAULT_HEAP_SIZE (Px_PAGE_SIZE)
#define Px_DEFAULT_TLS_HEAP_SIZE (Px_PAGE_SIZE)
#define Px_MAX_SEM (32768)

#define Px_PTR_IN_HEAP(p, h)                                    \
    (!p || !h ? __debugbreak(), 0 : (                           \
        (Px_PTR((p)) >= Px_PTR(((Heap *)(h))->base)) &&         \
        (Px_PTR((p)) <= Px_PTR(                                 \
            Px_PTR((((Heap *)(h))->base)) +                     \
            Px_PTR((((Heap *)(h))->size))                       \
        ))                                                      \
    ))

#define Px_PTR_IN_HEAP_BEFORE_SNAPSHOT(p, h, s)                 \
    (!p || !h || !s || (h->id != s->id) ? __debugbreak(), 0 : ( \
        (Px_PTR((p)) >= Px_PTR(((Heap *)(s))->base)) &&         \
        (Px_PTR((p)) <= Px_PTR(((Heap *)(s))->next)) &&         \
        (Px_PTR((p)) <= Px_PTR(                                 \
            Px_PTR((((Heap *)(s))->base)) +                     \
            Px_PTR((((Heap *)(s))->size))                       \
        ))                                                      \
))


#define Px_PTR_IN_HEAP_AFTER_SNAPSHOT(p, h, s)                  \
    (!p || !h || !s || (h->id != s->id) ? __debugbreak(), 0 : ( \
        (Px_PTR((p)) >= Px_PTR(((Heap *)(s))->base)) &&         \
        (Px_PTR((p)) >= Px_PTR(((Heap *)(s))->next)) &&         \
        (Px_PTR((p)) <= Px_PTR(                                 \
            Px_PTR((((Heap *)(s))->base)) +                     \
            Px_PTR((((Heap *)(s))->size))                       \
        ))                                                      \
))

static __inline
size_t
Px_GET_ALIGNMENT(void *p)
{
    Px_UINTPTR c = Px_PTR(p);
    unsigned int i = 0;
    if (!p)
        return 0;
    while (!((c >> i) & 1))
        i++;
    return (1ULL << i);
}

static __inline
unsigned int
Px_NEXT_POWER_OF_2(const unsigned int i)
{
    unsigned int v = i-1;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v+1;
}

#define Py_ASPX(ob) ((PxObject *)(((PyObject*)(ob))->px))

#ifdef MS_WINDOWS
#define PyEvent     HANDLE
#define PyEventType HANDLE

#define Py_EVENT(o)         ((PyEventType)(((PyObject *)(o))->event))
#define PyEvent_CREATE(o)   (Py_EVENT(o) = CreateEvent(0, 0, 0, 0))
#define PyEvent_INIT(o)     /* N/A */
#define PyEvent_SIGNAL(o)   (SetEvent(Py_EVENT(o)))
#define PyEvent_DESTROY(o)  (CloseHandle(Py_EVENT(o)))

#define PyRWLock            SRWLOCK
#define Py_RWLOCK(o)        ((PyRWLock *)&(((PyObject *)(o))->srw_lock))

#define PyRWLock_CREATE(o)  /* N/A */
#define PyRWLock_INIT(o)    (InitializeSRWLock((PSRWLOCK)&(o->srw_lock)))
#define PyRWLock_DESTROY(o) /* N/A */
#endif

#define PyAsync_IO_READ      (1UL <<  1)
#define PyAsync_IO_WRITE     (1UL <<  2)

#define Px_CTXTYPE(c)      (((Context *)c)->context_type)

#define Px_CTXTYPE_WORK    (1)
#define Px_CTXTYPE_WAIT    (1UL <<  1)
#define Px_CTXTYPE_SOCK    (1UL <<  2)
#define Px_CTXTYPE_FILE    (1UL <<  3)

#include "pxlist.h"

#ifdef _WIN64
#define Px_NUM_TLS_WSABUFS 64
#else
#define Px_NUM_TLS_WSABUFS 32
#endif


typedef struct _PyParallelHeap PyParallelHeap, Heap;
typedef struct _PyParallelContext PyParallelContext, WorkContext, Context;
typedef struct _PyParallelIOContext PyParallelIOContext, IOContext;
typedef struct _PyParallelContextStats PyParallelContextStats, Stats;
typedef struct _PyParallelIOContextStats PyParallelIOContextStats, IOStats;
typedef struct _PyParallelCallback PyParallelCallback, Callback;

typedef struct _PxSocket PxSocket;
typedef struct _PxSocketBuf PxSocketBuf;
typedef struct _PxHeap PxHeap;

typedef struct _PxThreadLocalState TLS;

typedef struct _PxState PxState;

typedef struct _TLSBUF {
    char            bitmap_index;
    TLS            *tls;
    Heap           *snapshot;
    PxSocket       *s;
    OVERLAPPED      ol;
    WSABUF          w;
} TLSBUF;

#define T2W(b)      (_Py_CAST_FWD(b, WSABUF *, TLSBUF, w))
#define W2T(b)      (_Py_CAST_BACK(b, TLSBUF *, TLSBUF, w))
#define OL2T(b)     (_Py_CAST_BACK(b, TLSBUF *, TLSBUF, ol))
#define T2OL(b)     (_Py_CAST_FWD(b, OVERLAPPED *, TLSBUF, ol))

typedef struct _SBUF {
    PxSocket       *s;
    Context        *ctx;
    Heap           *snapshot;
    DWORD           last_thread_id;
    DWORD           last_core_id;
    OVERLAPPED      ol;
    WSABUF          w;
} SBUF;

#define SBUF_ALIGNED_SIZE (Px_PTR_ALIGN(sizeof(SBUF)))

#define S2W(b)      (_Py_CAST_FWD(b, WSABUF *, SBUF, w))
#define W2S(b)      (_Py_CAST_BACK(b, SBUF *, SBUF, w))
#define OL2S(b)     (_Py_CAST_BACK(b, SBUF *, SBUF, ol))
#define S2OL(b)     (_Py_CAST_FWD(b, OVERLAPPED *, SBUF, ol))

typedef struct _RBUF RBUF;
typedef struct _RBUF {
    /* mirror sbuf */
    PxSocket       *s;
    Context        *ctx;
    Heap           *snapshot;
    DWORD           last_thread_id;
    DWORD           last_core_id;
    OVERLAPPED      ol;
    WSABUF          w;
    /* end of sbuf */
    RBUF           *prev;
    RBUF           *next;
    size_t          signature;
    /* mimic PyBytesObject herein */
    PyObject_VAR_HEAD
    Py_hash_t ob_shash;
    char ob_sval[1];
} RBUF;

#define RBUF_ALIGNED_SIZE (Px_PTR_ALIGN(sizeof(RBUF)))

#define R2W(p)      (_Py_CAST_FWD(p, WSABUF *, RBUF, w))
#define W2R(p)      (_Py_CAST_BACK(p, RBUF *, RBUF, w))
#define OL2R(p)     (_Py_CAST_BACK(p, RBUF *, RBUF, ol))
#define R2OL(p)     (_Py_CAST_FWD(p, OVERLAPPED *, RBUF, ol))
#define R2B(p)      (_Py_CAST_FWD(p, PyBytesObject *, RBUF, ob_base))
#define B2S(p)      (_Py_CAST_BACK(p, size_t, RBUF, signature))
/*
#define B2R(p)                              \
    (B2S(p) == _PxSocket_RBUF_Signature ?   \
        (_Py_CAST_BACK(p, RBUF *, RBUF, ob_base

#define PxSocketBuf2PyBytesObject(s) \
    (_Py_CAST_FWD(s, PyBytesObject *, PxSocketBuf, ob_base))

#define PyBytesObject2PxSocketBuf(b)                                  \
    (PyBytesObject2PxSocketBufSignature(b) == _PxSocketBufSignature ? \
        (_Py_CAST_BACK(b, PxSocketBuf *, PxSocketBuf, ob_base)) :     \
        (PxSocketBuf *)NULL                                           \
    )

#define PyBytesObject2PxSocketBufSignature(b) \
    (_Py_CAST_BACK(b, size_t, PxSocketBuf, ob_base))
*/


#define usize_t unsigned size_t

/* 29 = len('Tue, 15 Nov 2010 08:12:31 GMT') */
#define GMTIME_STRLEN 29+1 /* 1 = '\0' */

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
    Object   *prev;
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
    Object *n;
    if (!list->first) {
        list->first = o;
        list->last = o;
        o->prev = NULL;
    } else {
        n = list->last;
        n->next = o;
        o->prev = n;
        list->last = o;
    }
    o->next = NULL;
}

static __inline
void
remove_object(Objects *list, Object *o)
{
    Object *prev = o->prev;
    Object *next = o->next;

    if (list->first == o)
        list->first = next;

    if (list->last == o)
        list->last = prev;

    if (prev)
        prev->next = next;

    if (next)
        next->prev = prev;
}

#define _PxHeap_HEAD_EXTRA                  \
    Heap       *sle_prev;                   \
    Heap       *sle_next;                   \
    void       *base;                       \
    void       *next;                       \
    int         page_size;                  \
    size_t      pages;                      \
    size_t      next_alignment;             \
    size_t      size;                       \
    size_t      allocated;                  \
    size_t      remaining;                  \
    int         id;                         \
    int         flags;                      \
    Context    *ctx;                        \
    TLS        *tls;

#define PxHeap_HEAD PxHeap heap_base;

typedef struct _PxHeap {
    _PxHeap_HEAD_EXTRA
} PxHeap;

typedef struct _PyParallelHeap {
    _PxHeap_HEAD_EXTRA
    Objects px_deallocs;
    int     num_px_deallocs;
    int     px_deallocs_skipped;
    size_t  mallocs;
    size_t  deallocs;
    size_t  mem_reallocs;
    size_t  obj_reallocs;
    size_t  resizes;
    size_t  frees;
    size_t  alignment_mismatches;
    size_t  bytes_wasted;
} PyParallelHeap, Heap;

typedef struct _PyParallelContextStats {
    unsigned __int64 submitted;
    unsigned __int64 entered;
    unsigned __int64 exited;
    unsigned __int64 start;
    unsigned __int64 end;
    double runtime;

    long thread_id;
    long process_id;

    int blocking_calls;

    size_t mallocs;
    size_t mem_reallocs;
    size_t obj_reallocs;
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

typedef struct _PxThreadLocalState {
    Heap       *h;
    Heap       *ctx_heap;
    Heap        heap;
    HANDLE      handle;
    int         heap_depth;
    DWORD       thread_id;
    DWORD       thread_seq_id;
    PxState    *px;
    Stats       stats;

    /*
    CRITICAL_SECTION riobuf_bitmap_cs;
    RTL_BITMAP       riobuf_bitmap;
    WSABUF           riobuf;
    */

} PxThreadLocalState, TLS;

#define HASH_DEBUG
#include "uthash.h"

#define _PxPages_MAX_HEAPS 2
typedef struct _PxPages {
    Px_UINTPTR  base;
    Heap       *heaps[_PxPages_MAX_HEAPS];
    short       count;
    UT_hash_handle hh;
} PxPages;

typedef struct _PxParents {
    PyObject *ob;
    UT_hash_handle hh;
} PxParents;

#define PyAsync_IO_BUFSIZE (64 * 1024)

#define PyAsync_NUM_BUFS (32)

#define PxIO_PREALLOCATED (0)
#define PxIO_ONDEMAND     (1)

#define PxIO_FLAGS(i) (((PxIO *)i)->flags)
#define PxIO_IS_PREALLOC(i) (PxIO_FLAGS(i) == PxIO_PREALLOCATED)
#define PxIO_IS_ONDEMAND(i) (PxIO_FLAGS(i) == PxIO_ONDEMAND)

#define Px_IOTYPE_FILE      (1)
#define Px_IOTYPE_SOCKET    (1UL <<  1)

typedef struct _PxIO PxIO;

typedef struct _PxIO {
    __declspec(align(Px_MEM_ALIGN_RAW))
    PxListEntry entry;
    OVERLAPPED  overlapped;
    PyObject   *obj;
    ULONG       size;
    int         flags;
    __declspec(align(Px_PTR_ALIGN_RAW))
    ULONG       len;
    char FAR   *buf;
} PxIO;

#define PxIO2WSABUF(io) (_Py_CAST_FWD(io, LPWSABUF, PxIO, len))
#define OL2PxIO(ol)     (_Py_CAST_BACK(ol, PxIO *, PxIO, overlapped))

typedef struct _PxState {
    PxListHead *retired_contexts;
    PxListHead *errors;
    PxListHead *completed_callbacks;
    PxListHead *completed_errbacks;
    PxListHead *new_threadpool_work;
    PxListHead *incoming;
    PxListHead *finished;
    PxListHead *finished_sockets;
    PxListHead *shutdown_server;

    PxListHead *work_ready;

    PxListHead *io_ondemand;
    PxListHead *io_free;
    HANDLE      io_free_wakeup;

    Context    *iob_ctx;

    SRWLOCK     pages_srwlock;
    PxPages    *pages;

    SRWLOCK     parents_srwlock;
    PxParents  *parents;

    PyThreadState *tstate;

    HANDLE      low_memory_resource_notification;

    PTP_POOL ptp;
    PTP_CLEANUP_GROUP ptp_cg;
    PTP_CLEANUP_GROUP_CANCEL_CALLBACK ptp_cgcb;
    TP_CALLBACK_ENVIRON tp_cbe;
    PTP_CALLBACK_ENVIRON ptp_cbe;

    PTP_TIMER ptp_timer_gmtime;
    SRWLOCK   gmtime_srwlock;
    char      gmtime_buf[GMTIME_STRLEN];

    /* List head */
    LIST_ENTRY contexts;
    CRITICAL_SECTION contexts_cs;
    volatile long num_contexts;

    PxListHead *free_contexts;
    unsigned short max_free_contexts;

    unsigned short ctx_ttl;

    HANDLE wakeup;

    CRITICAL_SECTION cs;

    int processing_callback;

    long long contexts_created;
    long long contexts_destroyed;

    volatile long contexts_active;
    volatile long contexts_persisted;

    volatile long long io_stalls;

    volatile long active;
    volatile long persistent;

    volatile long incoming_pynone_decrefs;

    //__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
    volatile long long  submitted;
    volatile long       pending;
    volatile long       inflight;
    volatile long long  done;
    //volatile long long  failed;
    //volatile long long  succeeded;

    //__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
    volatile long long  waits_submitted;
    volatile long       waits_pending;
    volatile long       waits_inflight;
    volatile long long  waits_done;
    //volatile long long  failed;
    //volatile long long  succeeded;

    //__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
    volatile long long  timers_submitted;
    volatile long       timers_pending;
    volatile long       timers_inflight;
    volatile long long  timers_done;
    //volatile long long  failed;
    //volatile long long  succeeded;

    //__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
    volatile long long  io_submitted;
    volatile long       io_pending;
    volatile long       io_inflight;
    volatile long long  io_done;

    volatile long long  async_writes_completed_synchronously;
    volatile long long  async_reads_completed_synchronously;

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

    volatile long tls_buf_mismatch;
    volatile long tls_buf_match;
    volatile long tls_heap_rollback_mismatch;
    volatile long tls_heap_rollback_match;

} PxState;

int _PyParallel_InitPxState(PyThreadState *tstate, int destroy);

#define _PxContext_HEAD_EXTRA       \
    __declspec(align(16))           \
    SLIST_ENTRY slist_entry;        \
    HANDLE  heap_handle;            \
    Heap    heap;                   \
    Heap   *h;                      \
    Heap    snapshot;               \
    Heap   *s;                      \
    PxState *px;                    \
    PyThreadState *tstate;          \
    PyThreadState *pstate;          \
    PTP_CALLBACK_INSTANCE instance; \
    int flags;

#define PxContext_HEAD  PxContext ctx_base;

typedef struct _PxContext {
    _PxContext_HEAD_EXTRA
} PxContext;

typedef struct _PyParallelContext {
    _PxContext_HEAD_EXTRA
    Stats     stats;
    Stats     stats_snapshot;

    PyObject *waitobj;
    PyObject *waitobj_timeout;
    PyObject *func;
    PyObject *args;
    PyObject *kwds;
    PyObject *callback;
    PyObject *errback;
    PyObject *result;

    PyObject *tstate_dict;

    Context  *tp_ctx;

    PTP_POOL ptp;
    PTP_CLEANUP_GROUP ptp_cg;
    PTP_CLEANUP_GROUP_CANCEL_CALLBACK ptp_cgcb;
    TP_CALLBACK_ENVIRON tp_cbe;
    PTP_CALLBACK_ENVIRON ptp_cbe;

    // Wait pool -- we keep this separate from the thread pool above such that
    // we can set the max thread count much higher (as each thread can wait on
    // 63 unique events).
    Context *tpw_ctx;
    PTP_POOL ptpw;
    PTP_CLEANUP_GROUP ptpw_cg;
    PTP_CLEANUP_GROUP_CANCEL_CALLBACK ptpw_cgcb;
    TP_CALLBACK_ENVIRON tpw_cbe;
    PTP_CALLBACK_ENVIRON ptpw_cbe;

    TP_WAIT         *tp_wait;
    TP_WAIT_RESULT   wait_result;

    /* Link to PxState contexts list head. */
    LIST_ENTRY px_link;

    int         io_type;
    TP_IO      *tp_io;
    DWORD       io_status;
    ULONG       io_result;
    ULONG_PTR   io_nbytes;
    PxIO       *io;
    PyObject   *io_obj;

    /* This is a generic overlapped member that can be used when an OVERLAPPED
     * struct is needed, but not within the context of an async send/recv (as
     * the SBUF/RBUF 'ol' member can be used for that).
     *
     * This is currently leveraged for calls like ConnectEx, TransmitFile,
     * etc.
     */
    OVERLAPPED  overlapped;

    /* The `ol` pointer, however, is orthogonal to the overlapped struct
     * above.  It will be set to whatever overlapped struct is in effect
     * for a given context.  Depending on what's going on at any time, it
     * may be:
     *      a) null
     *      b) pointing at an sbuf->ol or an rbuf->ol
     *      c) pointing at the overlapped struct above
     */
    OVERLAPPED *ol;

    LARGE_INTEGER filesize;
    LARGE_INTEGER next_read_offset;

    TP_TIMER *tp_timer;

    PyObject    *exc_type;
    PyObject    *exc_value;
    PyObject    *exc_traceback;

    Context *prev;
    Context *next;

    PyObject *ob_first;
    PyObject *ob_last;

    PxListItem *error;
    PxListItem *callback_completed;
    PxListItem *errback_completed;

    PxListHead *outgoing;
    PxListHead *decrefs;
    PxListItem *decref;

    volatile long refcnt;

    Objects objects;
    Objects varobjs;
    Objects events;

    size_t leaked_bytes;
    size_t leak_count;
    void *last_leak;

    PyObject *errors_tuple;
    int hijacked_for_errors_tuple;
    size_t size_before_hijack;

    short ttl;

    long done;

    int times_reused;
    int times_recycled;
    int times_finished;
    char is_persisted;
    char was_persisted;
    int persisted_count;

} PyParallelContext, Context;

int PxContext_Snapshot(Context *c);
int PxContext_Restore(Context *c);

typedef struct _PyParallelIOContext {
    PyObject        *o;
    WorkContext     *work_ctx;

} PyParallelIOContext, IOContext;


typedef struct _PxObject {
    Context     *ctx;
    size_t       size;
    PyObject    *resized_to;
    PyObject    *resized_from;
    INIT_ONCE    persist;
    size_t       signature;
} PxObject;

#define Px_CTXFLAGS(c)      (((Context *)c)->flags)

#define Px_CTXFLAGS_IS_PERSISTED    (1)
#define Px_CTXFLAGS_WAS_PERSISTED   (1UL <<  1)
#define Px_CTXFLAGS_REUSED          (1UL <<  2)
#define Px_CTXFLAGS_IS_WORK_CTX     (1UL <<  3)
#define Px_CTXFLAGS_DISASSOCIATED   (1UL <<  4)
#define Px_CTXFLAGS_HAS_STATS       (1UL <<  5)
#define Px_CTXFLAGS_TLS_HEAP_ACTIVE (1UL <<  6)

#define Px_CTX_IS_PERSISTED(c)   (Px_CTXFLAGS(c) & Px_CTXFLAGS_IS_PERSISTED)
#define Px_CTX_WAS_PERSISTED(c)  (Px_CTXFLAGS(c) & Px_CTXFLAGS_WAS_PERSISTED)
#define Px_CTX_REUSED(c)         (Px_CTXFLAGS(c) & Px_CTXFLAGS_REUSED)
#define Px_IS_WORK_CTX(c)        (Px_CTXFLAGS(c) & Px_CTXFLAGS_IS_WORK_CTX)
#define Px_CTX_IS_DISASSOCIATED(c) (Px_CTXFLAGS(c) & Px_CTXFLAGS_DISASSOCIATED)
#define Px_CTX_HAS_STATS(c)      (Px_CTXFLAGS(c) & Px_CTXFLAGS_HAS_STATS)

#define STATS(c) \
    (Px_CTX_HAS_STATS(c) ? ((Stats *)(&(((Context *)c)->stats))) : 0)

#define Px_SOCKFLAGS(s)     (((PxSocket *)s)->flags)

#define Px_SOCKFLAGS_CLIENT                     (1)
#define Px_SOCKFLAGS_SERVER                     (1ULL <<  1)
#define Px_SOCKFLAGS_HOG                        (1ULL <<  2)
#define Px_SOCKFLAGS_RECV_MORE                  (1ULL <<  3)
#define Px_SOCKFLAGS_CONNECTED                  (1ULL <<  4)
#define Px_SOCKFLAGS_CLEAN_DISCONNECT           (1ULL <<  5)
#define Px_SOCKFLAGS_THROUGHPUT                 (1ULL <<  6)
#define Px_SOCKFLAGS_throughput                 (1ULL <<  6)
#define Px_SOCKFLAGS_SERVERCLIENT               (1ULL <<  7)
#define Px_SOCKFLAGS_INITIAL_BYTES              (1ULL <<  8)
#define Px_SOCKFLAGS_INITIAL_BYTES_STATIC       (1ULL <<  9)
#define Px_SOCKFLAGS_INITIAL_BYTES_CALLABLE     (1ULL << 10)
#define Px_SOCKFLAGS_CONCURRENCY                (1ULL << 11)
#define Px_SOCKFLAGS_concurrency                (1ULL << 11)
#define Px_SOCKFLAGS_CHECKED_DR_UNREACHABLE     (1ULL << 12)
#define Px_SOCKFLAGS_SENDING_INITIAL_BYTES      (1ULL << 13)
#define Px_SOCKFLAGS_LINES_MODE_ACTIVE          (1ULL << 14)
#define Px_SOCKFLAGS_lines_mode_active          (1ULL << 14)
#define Px_SOCKFLAGS_SHUTDOWN_SEND              (1ULL << 15)
#define Px_SOCKFLAGS_shutdown_send              (1ULL << 15)
#define Px_SOCKFLAGS_CAN_RECV                   (1ULL << 16)
#define Px_SOCKFLAGS_can_recv                   (1ULL << 16)
#define Px_SOCKFLAGS_SEND_SHUTDOWN              (1ULL << 17)
#define Px_SOCKFLAGS_RECV_SHUTDOWN              (1ULL << 18)
#define Px_SOCKFLAGS_BOTH_SHUTDOWN              (1ULL << 19)
#define Px_SOCKFLAGS_SEND_SCHEDULED             (1ULL << 20)
#define Px_SOCKFLAGS_MAX_SYNC_SEND_ATTEMPTS     (1ULL << 21)
#define Px_SOCKFLAGS_max_sync_send_attempts     (1ULL << 21)
#define Px_SOCKFLAGS_CLOSE_SCHEDULED            (1ULL << 22)
#define Px_SOCKFLAGS_CLOSED                     (1ULL << 23)
#define Px_SOCKFLAGS_TIMEDOUT                   (1ULL << 24)
#define Px_SOCKFLAGS_CALLED_CONNECTION_MADE     (1ULL << 25)
#define Px_SOCKFLAGS_IS_WAITING_ON_FD_ACCEPT    (1ULL << 26)
#define Px_SOCKFLAGS_ACCEPT_CALLBACK_SEEN       (1ULL << 27)
#define Px_SOCKFLAGS_MAX_SYNC_RECV_ATTEMPTS     (1ULL << 29)
#define Px_SOCKFLAGS_max_sync_recv_attempts     (1ULL << 29)
#define Px_SOCKFLAGS_SENDFILE_SCHEDULED         (1ULL << 30)
#define Px_SOCKFLAGS_WRITEFILE_SCHEDULED        (1ULL << 31)
#define Px_SOCKFLAGS_READFILE_SCHEDULED         (1ULL << 32)
#define Px_SOCKFLAGS_LOW_LATENCY                (1ULL << 33)
#define Px_SOCKFLAGS_low_latency                (1ULL << 33)
#define Px_SOCKFLAGS_NEXT_BYTES                 (1ULL << 34)
#define Px_SOCKFLAGS_NEXT_BYTES_STATIC          (1ULL << 35)
#define Px_SOCKFLAGS_NEXT_BYTES_CALLABLE        (1ULL << 36)
#define Px_SOCKFLAGS_SENDING_NEXT_BYTES         (1ULL << 37)
#define Px_SOCKFLAGS_TUNNEL                     (1ULL << 38)
#define Px_SOCKFLAGS_REVERSE_TUNNEL             (1ULL << 39)
#define Px_SOCKFLAGS_ODBC                       (1ULL << 40)
#define Px_SOCKFLAGS_odbc                       (1ULL << 40)
#define Px_SOCKFLAGS_CONNECTION_STRING          (1ULL << 41)
#define Px_SOCKFLAGS_connection_string          (1ULL << 41)
#define Px_SOCKFLAGS_OTHER_ASYNC_SCHEDULED      (1ULL << 42)
#define Px_SOCKFLAGS_CLEANED_UP                 (1ULL << 43)
#define Px_SOCKFLAGS_HTTP11                     (1ULL << 44)
#define Px_SOCKFLAGS_http11                     (1ULL << 44)
#define Px_SOCKFLAGS_CLIENT_CREATED             (1ULL << 45)
#define Px_SOCKFLAGS_client_created             (1ULL << 45)
#define Px_SOCKFLAGS_CALLED_CLIENT_CREATED      (1ULL << 46)
#define Px_SOCKFLAGS_SNAPSHOT_UPDATE_SCHEDULED  (1ULL << 47)
#define Px_SOCKFLAGS_rate_limit                 (1ULL << 48)
#define Px_SOCKFLAGS_RATE_LIMIT                 (1ULL << 48)
#define Px_SOCKFLAGS_                           (1ULL << 63)

#define PxSocket_CBFLAGS(s) (((PxSocket *)s)->cb_flags)

#define PxSocket_CBFLAGS_SEND_FAILED            (1)
#define PxSocket_CBFLAGS_SEND_COMPLETE          (1UL <<  1)
#define PxSocket_CBFLAGS_CONNECTION_CLOSED      (1UL <<  2)
#define PxSocket_CBFLAGS_SHUTDOWN_SEND          (1UL <<  3)
#define PxSocket_CBFLAGS_RECV_FAILED            (1UL <<  4)

#define PxSocket_IS_HOG(s)      (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_HOG)
#define PxSocket_IS_CLIENT(s)   (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CLIENT)
#define PxSocket_IS_SERVER(s)   (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_SERVER)
#define PxSocket_IS_BOUND(s)    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_BOUND)
#define PxSocket_IS_CONNECTED(s) (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CONNECTED)
#define PxSocket_IS_SENDFILE_SCHEDULED(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_SENDFILE_SCHEDULED)
#define PxSocket_IS_OTHER_ASYNC_SCHEDULED(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_OTHER_ASYNC_SCHEDULED)
#define PxSocket_IS_CLOSE_SCHEDULED(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CLOSE_SCHEDULED)


#define PxSocket_IS_PERSISTENT(s) (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_PERSISTENT)

#define PxSocket_LINES_MODE_ACTIVE(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_LINES_MODE_ACTIVE)

#define PxSocket_THROUGHPUT(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_THROUGHPUT)

#define PxSocket_CONCURRENCY(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CONCURRENCY)

#define PxSocket_LOW_LATENCY(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_LOW_LATENCY)

#define PxSocket_CAN_RECV(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CAN_RECV)

#define PxSocket_SHUTDOWN_SEND(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_SHUTDOWN_SEND)

#define PxSocket_MAX_SYNC_SEND_ATTEMPTS(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_MAX_SYNC_SEND_ATTEMPTS)

#define PxSocket_MAX_SYNC_RECV_ATTEMPTS(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_MAX_SYNC_RECV_ATTEMPTS)

#define PxSocket_HAS_INITIAL_BYTES(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_INITIAL_BYTES)

#define PxSocket_HAS_NEXT_BYTES(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_NEXT_BYTES)

#define PxSocket_HAS_SEND_COMPLETE(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_HAS_SEND_COMPLETE)

#define PxSocket_HAS_DATA_RECEIVED(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_HAS_DATA_RECEIVED)

#define PxSocket_HAS_LINES_RECEIVED(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_HAS_LINES_RECEIVED)

#define PxSocket_HAS_SEND_FAILED(s) \
    (PxSocket_CBFLAGS(s) & PxSocket_CBFLAGS_SEND_FAILED)

#define PxSocket_HAS_RECV_FAILED(s) \
    (PxSocket_CBFLAGS(s) & PxSocket_CBFLAGS_RECV_FAILED)

#define PxSocket_SET_SEND_FAILED(s) \
    (PxSocket_CBFLAGS(s) |= PxSocket_CBFLAGS_SEND_COMPLETE)

#define PxSocket_HAS_CONNECTION_CLOSED(s) \
    (PxSocket_CBFLAGS(s) & PxSocket_CBFLAGS_CONNECTION_CLOSED)

#define PxSocket_SET_CONNECTION_CLOSED(s) \
    (PxSocket_CBFLAGS(s) |= PxSocket_CBFLAGS_CONNECTION_CLOSED)

#define PxSocket_HAS_SHUTDOWN_SEND(s) \
    (PxSocket_CBFLAGS(s) & PxSocket_CBFLAGS_SHUTDOWN_SEND)

#define PxSocket_SET_SHUTDOWN_SEND(s) \
    (PxSocket_CBFLAGS(s) |= PxSocket_CBFLAGS_SHUTDOWN_SEND)

#define PxSocket_IS_SERVERCLIENT(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_SERVERCLIENT)

#define PxSocket_IS_PENDING_DISCONNECT(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_PENDING_DISCONNECT)

#define PxSocket_IS_DISCONNECTED(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_DISCONNECTED)

#define PxSocket_ODBC(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_ODBC)

#define PxSocket_HAS_CONNECTION_STRING(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CONNECTION_STRING)

#define PxSocket_IS_CLEANED_UP(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CLEANED_UP)

#define PxSocket_IS_HTTP11(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_HTTP11)

#define PxSocket_HAS_CLIENT_CREATED(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CLIENT_CREATED)

#define PxSocket_CALLED_CLIENT_CREATED(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CALLED_CLIENT_CREATED)

#define PxSocket_SNAPSHOT_UPDATE_SCHEDULED(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_SNAPSHOT_UPDATE_SCHEDULED)

#define PxSocket_IS_RATE_LIMITED(s) \
    (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_RATE_LIMIT)

#define PxSocket_RECV_MORE(s)   (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_RECV_MORE)

#define PxSocket_CB_CONNECTION_MADE     (1)

#define PxSocket_CB_LINES_RECEIVED      (1UL <<  2)
#define PxSocket_CB_EOF_RECEIVED        (1UL <<  3)
#define PxSocket_CB_CONNECTION_LOST     (1UL <<  4)

/* Types of socket I/O operations, used for the s->*_io_op members. */
#define PxSocket_IO_CONNECT             (1)
#define PxSocket_IO_ACCEPT              (2)
#define PxSocket_IO_RECV                (3)
#define PxSocket_IO_SEND                (4)
#define PxSocket_IO_DISCONNECT          (5)
#define PxSocket_IO_CLOSE               (6)
#define PxSocket_IO_SENDFILE            (7)
#define PxSocket_IO_SHUTDOWN            (8)
#define PxSocket_IO_LOW_MEMORY          (9)
#define PxSocket_IO_CLIENT_CONNECTED    (10)
#define PxSocket_IO_SENDMSG             (11)
#define PxSocket_IO_RECVMSG             (12)
#define PxSocket_IO_LISTEN              (13)
#define PxSocket_IO_GETADDRINFOEX       (14)
#define PxSocket_IO_READFILE            (15)
#define PxSocket_IO_WRITEFILE           (16)
#define PxSocket_IO_WAIT                (17)
#define PxSocket_IO_TIMER               (18)
#define PxSocket_IO_DB_CONNECT          (19)
#define PxSocket_IO_RATE_LIMIT          (20)

#define PxSocket_SET_NEXT_OP(s, op)     \
    do {                                \
        s->last_io_op = 0;              \
        s->this_io_op = 0;              \
        s->next_io_op = op;             \
    } while (0)

typedef struct _PxSocketProfileRecord {
    FILETIME        start_timestamp;    /* microsecond UTC */
    FILETIME        elapsed;            /* microseconds */
    ULONGLONG       bytes;
    ULONGLONG       sock_seq_id;
    ULONG           local_ipv4;
    ULONG           remote_ipv4;
    USHORT          local_port;
    USHORT          remote_port;
    USHORT          start_thread_seq_id;
    USHORT          end_thread_seq_id;
    BYTE            io_op;
    BYTE            unused1;
    ULONG           send_seq_id;
    ULONG           recv_seq_id;
    USHORT          heap_id;
    ULONG           allocated;
} PxSocketProfileRecord;

/*
np.dtype([
    ('start_timestamp',         np.uint64),     #   8       8
    ('elapsed',                 np.uint64),     #   8       16
    ('bytes',                   np.uint64),     #   8       24
    ('sock_seq_id',             np.uint64),     #   8       32
    ('local_ipv4',              np.uint32),     #   4       36
    ('remote_ipv4',             np.uint32),     #   4       40
    ('local_port',              np.uint16),     #   2       42
    ('remote_port',             np.uint16),     #   2       44
    ('start_thread_seq_id',     np.uint16),     #   2       46
    ('end_thread_seq_id',       np.uint16),     #   2       48
    ('io_op',                   np.uint8),      #   1       49
    ('unused1',                 np.uint8),      #   1       50
    ('send_seq_id',             np.uint32),     #   4       54
    ('recv_seq_id',             np.uint32),     #   4       58
    ('heap_id',                 np.uint16),     #   2       60
    ('allocated',               np.uint32),     #   4       64
])
*/


typedef struct _PxSocketBuf PxSocketBuf;
typedef struct _PxSocketBufList PxSocketBufList;

typedef struct _PxSocketBuf {
    /*WSAOVERLAPPED ol;*/
    WSABUF w;
    PxSocketBuf *prev;
    PxSocketBuf *next;
    size_t signature;
    /* mimic PyBytesObject herein */
    PyObject_VAR_HEAD
    Py_hash_t ob_shash;
    char ob_sval[1];
} PxSocketBuf;


typedef struct _PxSocketBufList {
    PyObject_VAR_HEAD
    /* mimic PyListObject */
    PyObject **ob_item;
    Py_ssize_t allocated;
    WSABUF **wsabufs;
    int nbufs;
    int flags;
} PxSocketBufList;

typedef void (*sockcb_t)(Context *c);

#define PxSocket2WSABUF(s) (_Py_CAST_FWD(s, LPWSABUF, PxSocket, len))
#define PxSocketBuf2PyBytesObject(s) \
    (_Py_CAST_FWD(s, PyBytesObject *, PxSocketBuf, ob_base))

#define PyBytesObject2PxSocketBuf(b)                                  \
    (PyBytesObject2PxSocketBufSignature(b) == _PxSocketBufSignature ? \
        (_Py_CAST_BACK(b, PxSocketBuf *, PxSocketBuf, ob_base)) :     \
        (PxSocketBuf *)NULL                                           \
    )

#define PyBytesObject2PxSocketBufSignature(b) \
    (_Py_CAST_BACK(b, size_t, PxSocketBuf, ob_base))

#define IS_SBUF(b)

typedef struct _PxSocketListItem {
    __declspec(align(16)) SLIST_ENTRY slist_entry;
    SOCKET_T sock_fd;
} PxSocketListItem;

typedef struct _PxSocket PxSocket;

typedef struct _PxSocket {
    PyObject_HEAD
    /* Mirror PySocketSockObject. */
    SOCKET_T sock_fd;           /* Socket file descriptor */
    int sock_family;            /* Address family, e.g., AF_INET */
    int sock_type;              /* Socket type, e.g., SOCK_STREAM */
    int sock_proto;             /* Protocol type, usually 0 */
    double sock_timeout;        /* Operation timeout in seconds; */
    long sock_seq_id;

    struct addrinfo local_addrinfo;
    struct addrinfo remote_addrinfo;

    sock_addr_t  local_addr;
    int          local_addr_len;
    sock_addr_t  remote_addr;
    int          remote_addr_len;

    ULONGLONG    flags;

    int   cb_flags;
    int   error_occurred;

    Context *ctx;
    int last_thread_id;
    int this_thread_id;
    unsigned int last_cpuid;
    unsigned int this_cpuid;
    int ioloops;

    PTP_WORK shutdown_server_tp_work;
    PTP_WORK preallocate_children_tp_work;
    PTP_TIMER slowloris_protection_tp_timer;
    PTP_TIMER ratelimit_timer;

    HANDLE heap_override;
    PyObject *last_getattr_name;
    PyObject *last_getattr_value;

    /* The bit of each thread_seq_id is set each time the socket is scheduled
       on a given thread. */
    ULONGLONG thread_seq_id_bitmap;

    /* Link from child -> parent via link_child/unlink_child. */
    //__declspec(align(MEMORY_ALLOCATION_ALIGNMENT))
    PxListItem link;

    Heap     *connectex_snapshot;

    /* Start-up snapshots. */
    Heap     *startup_heap_snapshot;
    PxSocket *startup_socket_snapshot;
    Stats    *startup_context_stats_snapshot;
    /* Ugh, we need to store the "socket flags set at startup" separately
     * because of our dodgy overloading of 'flags' with both static protocol
     * information (that will never change) like whether concurrency is set,
     * versus stateful information like "close scheduled" or "sendfile
     * scheduled".
     */
    ULONGLONG startup_socket_flags;

    volatile int ready_for_dealloc;

    int reused;
    int recycled;
    int no_tcp_nodelay;
    int no_exclusive_addr_use;

    int was_accepting;
    int was_connecting;
    int was_connected;
    int was_disconnecting;

    /* endpoint */
    char  ip[16];
    char *host;
    int   hostlen;
    int   port;

    CRITICAL_SECTION cs;

    int       recvbuf_size;
    int       sendbuf_size;

    size_t send_id;
    size_t recv_id;

    LARGE_INTEGER stopwatch_frequency;

    LARGE_INTEGER stopwatch_start;
    LARGE_INTEGER stopwatch_stop;
    LARGE_INTEGER stopwatch_elapsed;

    FILETIME utc_start;
    FILETIME utc_stop;

    TP_WAIT            *tp_wait;
    PTP_WAIT_CALLBACK   tp_wait_callback;
    TP_WAIT_RESULT      wait_result;
    FILETIME            wait_timeout;
    HANDLE              wait_event;
    PVOID               wait_callback_context;
    int                 wait_next_io_op;


    /* Total bytes sent/received */
    Py_ssize_t  total_bytes_sent;
    Py_ssize_t  total_bytes_received;

    /* Used by the parent's "children" list. */
    LIST_ENTRY child_link;

    ULONGLONG total_send_size;
    LPWSABUF send_buffers;
    DWORD num_send_buffers;
    Heap *send_snapshot;

    DWORD num_bytes_just_sent;
    DWORD num_bytes_just_received;

    PyObject *protocol_type;
    PyObject *protocol;

    PyObject *lines_mode;
    PyObject *send_failed;
    PyObject *recv_failed;
    PyObject *send_shutdown;
    PyObject *recv_shutdown;
    PyObject *send_complete;
    PyObject *data_received;
    PyObject *lines_received;
    PyObject *client_created;
    PyObject *connection_made;
    PyObject *connection_closed;
    PyObject *exception_handler;
    PyObject *initial_bytes_to_send;
    PyObject *initial_bytes_callable;
    WSABUF    initial_bytes;
    PyObject *next_bytes_to_send;
    PyObject *next_bytes_callable;
    WSABUF    next_bytes;
    PyObject *odbc;
    PyObject *connection_string;
    PyObject *http11;
    PyObject *json_dumps;
    PyObject *json_loads;
    PyObject *rate_limit;
    FILETIME  rate_limit_ft;

    PyObject *methods;

    //PyObject *cnxn; // pxodbc.Connection
    //PyObject *db_connection_made;
    //PyObject *db_execute_complete;
    //PyObject *db_fetch_complete;
    /* Wait events. */
    //HANDLE    db_connect_event;
    //HANDLE    db_event;

    HENV      henv;
    //HDBC      hdbc;
    //int       has_odbc;
    int       odbc_initialized;

    int       max_sync_send_attempts;
    int       max_sync_recv_attempts;
    int       max_sync_connectex_attempts;
    int       max_sync_acceptex_attempts;

    int       lines_mode_active;

    int       client_disconnected;
    int       reused_socket;
    int       in_overlapped_callback;

    BOOL is_low_memory;

    int slowloris_protection_seconds;
    volatile long memory_failures;
    volatile long negative_child_connect_time_count;
    volatile long num_times_sloworis_protection_triggered;

    volatile long tp_cleanups;
    volatile long tpw_cleanups;

    /* sendfile stuff */
    int    sendfile_flags;
    DWORD  sendfile_wsa_error;
    DWORD  sendfile_nbytes;
    HANDLE sendfile_handle;
    Heap  *sendfile_snapshot;
    TRANSMIT_FILE_BUFFERS sendfile_tfbuf;
    DWORD     sendfile_bytes_per_send;
    DWORD     sendfile_num_bytes_to_send;
    ULONGLONG sendfile_offset;

    int     last_io_op;
    int     this_io_op;
    int     next_io_op;

    TP_IO  *tp_io;

    TLSBUF *tls_buf;

    SBUF   *sbuf;
    RBUF   *rbuf;
    int     num_rbufs;

    DWORD wsa_error;
    DWORD recv_wsa_error;
    DWORD send_wsa_error;
    DWORD connectex_wsa_error;
    DWORD acceptex_wsa_error;
    DWORD disconnectex_wsa_error;

    OVERLAPPED overlapped_acceptex;
    OVERLAPPED overlapped_connectex;
    OVERLAPPED overlapped_disconnectex;
    OVERLAPPED overlapped_sendfile;
    OVERLAPPED overlapped_wsasend;
    OVERLAPPED overlapped_wsarecv;

    OVERLAPPED overlapped_getaddrinfoex;
    PADDRINFOEX   getaddrinfoex_results;
    HANDLE        getaddrinfoex_handle;
    HANDLE        getaddrinfoex_cancel;
    int          *getaddrinfoex_results_addrlen;
    sock_addr_t  *getaddrinfoex_results_addr;
    int           next_io_op_after_getaddrinfoex;

    int   disconnectex_flags;

    int connect_time; /* seconds */

    DWORD connectex_sent_bytes;

    /* HTTP header stuff */
    union {
        HttpRequest *request;
        HttpResponse *response;
    } http;
    PyObject *http_header;
    int keep_alive;
    /*
    union {
        PyObject *header;
        PyObject *request;
        PyObject *response;
    } pyhttp;
    */

    /* Server-specific stuff. */

    /* Used for overlapped AcceptEx() calls. */
    DWORD acceptex_bufsize;
    DWORD acceptex_addr_len;
    DWORD acceptex_recv_bytes;

    //__declspec(align(MEMORY_ALLOCATION_ALIGNMENT))
    PxListHead link_child;

    volatile int num_accepts_to_post;

    /* Target number of posted AcceptEx() calls the server will try and
     * maintain.  Defaults to 2 * NCPU. */
    volatile int target_accepts_posted;

    /* How many times we submitted threadpool work for creating new client
     * sockets and posting accepts.  Won't necessarily correlate to how many
     * successful accepts we were able to post -- just that we submitted the
     * work to attempt posting.
     */
    volatile int total_accepts_attempted;

    /* How many clients can we create (i.e. create contexts for and then post
     * accepts) in the space of a single percent of memory load?  This is
     * automatically filled in the first time we allocate client sockets and
     * post accepts. */
    float clients_per_1pct_mem_load;

    Context  *wait_ctx;

    /* parent could be either the listen socket if we're an accept socket,
     * or whatever the active socket was (if there was one) when we were
     * created.
     */
    PxSocket *parent;
    /* And child will be a backref in the above case; i.e.
     *  <child_socket>->parent == <parent_socket>
     *  <parent_socket>->child == <child_socket>
     */
    PxSocket *child;

    /* Keep at least a cache line away from link_child. */
    //__declspec(align(MEMORY_ALLOCATION_ALIGNMENT))
    PxListHead unlink_child;

    int child_id;
    volatile int next_child_id;

    /* Doubly-linked list of all children. */
    LIST_ENTRY children;
    CRITICAL_SECTION children_cs;
    /* compare to num_children, which is interlocked */
    volatile int num_children_entries;

    /* List entry to the above list for children, also private/owned by
     * server. */
    LIST_ENTRY child_entry;

    /* Number of AcceptEx() calls posted that haven't yet been accepted.
     * (This differs from total_accepts_attempted above in that this value
     * reflects the number of times we were able to successfully submit an
     * overlapped AcceptEx() call, whereas the other one is just the number of
     * times we submitted threadpool work for creation of a new client socket
     * and subsequent AcceptEx() attempt.
     */
    volatile unsigned long accepts_posted;

    /* Number of sockets that have been accepted and are currently assumed to
     * be connected.  This may not necessarily correlate with the *actual*
     * state of the socket based on the TCP stack... but it eventually will.
     */
    volatile unsigned long clients_connected;

    /* Number of sockets in an overlapped DisconnectEx() state. */
    volatile unsigned long clients_disconnecting;

    /* Number of sockets that were active at then one point, and then free'd
     * up after completion because the server didn't have any outstanding
     * accepts needed. */
    volatile unsigned long retired_clients;

    volatile long num_children;

    volatile long recycled_unlinked_child;

    volatile long total_clients_reused;
    volatile long total_clients_recycled;

    volatile long accepts_wanted;

    volatile long sem_acquired;
    volatile long sem_released;
    volatile long sem_timeout;
    volatile long sem_count;
    volatile long sem_release_err;

    HANDLE accepts_sem;

    int child_seh_eav_count;

    int listen_backlog;

    WSAEVENT  fd_accept;
    HANDLE    client_connected;
    HANDLE    free_children;
    HANDLE    low_memory;
    HANDLE    shutdown;
    HANDLE    high_memory;
    HANDLE    wait_handles[6];

    /* Counters for above events. */
    int fd_accept_count;
    int client_connected_count;
    int low_memory_count;
    volatile int shutdown_count;
    int high_memory_count;
    int wait_timeout_count;

    int negative_accepts_to_post_count;

    BOOL shutting_down;
    BOOL shutdown_immediate;

    /* Misc debug/helper stuff. */
    int break_on_iocp_enter;
    int was_status_pending;

} PxSocket;

void PxContext_CallbackComplete(Context *);
void PxContext_ErrbackComplete(Context *);
void PxSocket_CallbackComplete(PxSocket *);
void PxSocket_ErrbackComplete(PxSocket *);
void PxSocketServer_LinkChild(PxSocket *child);
void PxSocketServer_UnlinkChild(PxSocket *child);
void PxSocketServer_Shutdown(PxSocket *s);
void PxSocket_Cleanup(PxSocket *s);

void
CALLBACK
PxSocketServer_ShutdownCallback(
    PTP_CALLBACK_INSTANCE instance,
    PVOID context,
    PTP_WORK work
);

#define I2S(i) (_Py_CAST_BACK(i, PxSocket *, PyObject, slist_entry))

#define PxSocket_GET_ATTR(n)                     \
    (PyObject_HasAttrString(s->protocol, n) ?    \
        PyObject_GetAttrString(s->protocol, n) : \
        Py_None)

static __inline
void
PxSocket_StopwatchStart(PxSocket *s)
{
    QueryPerformanceFrequency(&s->stopwatch_frequency);
    QueryPerformanceCounter(&s->stopwatch_start);
    //GetSystemTimePreciseAsFileTime(&s->utc_start);
}

static __inline
ULONGLONG
PxSocket_StopwatchStop(PxSocket *s)
{
    QueryPerformanceCounter(&s->stopwatch_stop);
    s->stopwatch_elapsed.QuadPart = (
        s->stopwatch_stop.QuadPart -
        s->stopwatch_start.QuadPart
    );
    s->stopwatch_elapsed.QuadPart *= 1000000;
    s->stopwatch_elapsed.QuadPart /= s->stopwatch_frequency.QuadPart;
    //GetSystemTimePreciseAsFileTime(&s->utc_stop);
    return s->stopwatch_elapsed.QuadPart;
}

/*
static __inline
void
PxList_PushSocket(PxListHead *head, PxSocket *s)
{
    SLIST_ENTRY *entry = (SLIST_ENTRY *)(&(s->ob_base.slist_entry));
    InterlockedPushEntrySList(head, entry);
}

static __inline
PxSocket *
PxList_PopSocket(PxListHead *head)
{
    PxSocket *s;
    SLIST_ENTRY *entry = InterlockedPopEntrySList(head);

    if (!entry)
        return NULL;

    s = I2S(entry);

    return s;
}
*/

static __inline
int
PxSocket_HasAttr(PxSocket *s, const char *callback)
{
    return PyObject_HasAttrString(s->protocol, callback);
}

PyObject *
PyObject_Clone(PyObject *src, const char *errmsg);

void PxSocket_TrySendScheduled(Context *c);

void PxSocket_HandleError(Context *c,
                          int op,
                          const char *syscall,
                          int errcode);

int PxSocket_ConnectionClosed(PxSocket *s, int op);
int PxSocket_ConnectionLost(PxSocket *s, int op, int errcode);
int PxSocket_ConnectionTimeout(PxSocket *s, int op);
int PxSocket_ConnectionError(PxSocket *s, int op, int errcode);
int PxSocket_ConnectionDone(PxSocket *s);

void PxSocket_TryRecv(Context *c);

void
PxSocket_HandleCallback(
    Context *c,
    const char *name,
    const char *format,
    ...
);

int PxSocket_ScheduleBufForSending(Context *c, PxSocketBuf *b);
PxSocketBuf *PxSocket_GetInitialBytes(PxSocket *);
PxSocketBuf *_try_extract_something_sendable_from_object(Context *c,
                                                         PyObject *o,
                                                         int depth);

void
NTAPI
PxSocketClient_Callback(
    PTP_CALLBACK_INSTANCE instance,
    void *context,
    void *overlapped,
    ULONG io_result,
    ULONG_PTR nbytes,
    TP_IO *tp_io
);

void PxSocket_HandleException(Context *c, const char *syscall, int fatal);

int PxSocket_LoadInitialBytes(PxSocket *s);


__inline
PyObject *
_read_lock(PyObject *obj)
{
    AcquireSRWLockShared((PSRWLOCK)&(obj->srw_lock));
    return obj;
}
#define READ_LOCK(o) (_read_lock((PyObject *)o))

__inline
PyObject *
_read_unlock(PyObject *obj)
{
    ReleaseSRWLockShared((PSRWLOCK)&(obj->srw_lock));
    return obj;
}
#define READ_UNLOCK(o) (_read_unlock((PyObject *)o))

__inline
char
_try_read_lock(PyObject *obj)
{
    return TryAcquireSRWLockShared((PSRWLOCK)&(obj->srw_lock));
}
#define TRY_READ_LOCK(o) (_try_read_lock((PyObject *)o))

__inline
PyObject *
_write_lock(PyObject *obj)
{
    AcquireSRWLockExclusive((PSRWLOCK)&(obj->srw_lock));
    return obj;
}
#define WRITE_LOCK(o) (_write_lock((PyObject *)o))

__inline
PyObject *
_write_unlock(PyObject *obj)
{
    ReleaseSRWLockExclusive((PSRWLOCK)&(obj->srw_lock));
    return obj;
}
#define WRITE_UNLOCK(o) (_write_unlock((PyObject *)o))

__inline
char
_try_write_lock(PyObject *obj)
{
    return TryAcquireSRWLockExclusive((PSRWLOCK)&(obj->srw_lock));
}
#define TRY_WRITE_LOCK(o) (_try_write_lock((PyObject *)o))


#define DO_SEND_COMPLETE() do {                                          \
    PxSocket_HandleCallback(c, "send_complete", "(On)", s, s->send_id);  \
    if (PyErr_Occurred())                                                \
        goto end;                                                        \
} while (0)

#define MAYBE_DO_SEND_COMPLETE() do {                                    \
    if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_HAS_SEND_COMPLETE)                \
        DO_SEND_COMPLETE();                                              \
} while (0)

#define DO_CONNECTION_MADE() do {                                        \
    assert(!(Px_SOCKFLAGS(s) & Px_SOCKFLAGS_SEND_SCHEDULED));            \
    PxSocket_HandleCallback(c, "connection_made", "(O)", s);             \
    if (PyErr_Occurred())                                                \
        goto end;                                                        \
} while (0)

#define DO_DATA_RECEIVED() do {                                          \
    const char *f = PxSocket_GetRecvCallback(s);                         \
    PxSocketBuf   *sbuf;                                                 \
    PyBytesObject *pbuf;                                                 \
    sbuf = c->rbuf_first;                                                \
    sbuf->ob_base.ob_size = c->io_nbytes;                                \
    pbuf = PxSocketBuf2PyBytesObject(sbuf);                              \
    PxSocket_HandleCallback(c, f, "(OO)", s, pbuf);                      \
    if (PyErr_Occurred())                                                \
        goto end;                                                        \
} while (0)

#define MAYBE_DO_CONNECTION_MADE() do {                                  \
    if ((Px_SOCKFLAGS(s) & Px_SOCKFLAGS_HAS_CONNECTION_MADE) &&          \
       !(Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CALLED_CONNECTION_MADE))         \
        DO_CONNECTION_MADE();                                            \
} while (0)

#define MAYBE_DO_SEND_FAILED() do {                                      \
    if ((s->io_op == PxSocket_IO_SEND) &&                                \
        (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_HAS_SEND_FAILED))                \
    {                                                                    \
        PyObject *args, *func;                                           \
        args = Py_BuildValue("(Oni)", s, s->send_id, c->io_result);      \
        if (!args)                                                       \
            PxSocket_EXCEPTION();                                        \
        READ_LOCK(s);                                                    \
        func = PxSocket_GET_ATTR("send_failed");                         \
        READ_UNLOCK(s);                                                  \
        assert(func);                                                    \
        result = PyObject_CallObject(func, args);                        \
        if (null_with_exc_or_non_none_return_type(result, c->pstate))    \
            PxSocket_EXCEPTION();                                        \
    }                                                                    \
} while (0)

#define _m_MAYBE_CLOSE() do {                                            \
    if ((Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CLOSE_SCHEDULED) ||              \
       !(Px_SOCKFLAGS(s) & Px_SOCKFLAGS_HAS_DATA_RECEIVED))              \
    {                                                                    \
        char error = 0;                                                  \
                                                                         \
        assert(!(Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CLOSED));                \
                                                                         \
        s->io_op = PxSocket_IO_CLOSE;                                    \
                                                                         \
        if (closesocket(s->sock_fd) == SOCKET_ERROR) {                   \
            if (WSAGetLastError() == WSAEWOULDBLOCK)                     \
                Py_FatalError("closesocket() -> WSAEWOULDBLOCK!");       \
            else                                                         \
                error = 1;                                               \
        }                                                                \
                                                                         \
        Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_CLOSE_SCHEDULED;                \
        Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_CONNECTED;                      \
        Px_SOCKFLAGS(s) |=  Px_SOCKFLAGS_CLOSED;                         \
                                                                         \
        if (error)                                                       \
            PxSocket_HandleException(c, "closesocket", 0);               \
        else                                                             \
            PxSocket_HandleCallback(c, "connection_closed", "(O)", s);   \
        goto end;                                                        \
    }                                                                    \
} while (0)

#define MAYBE_SEND() do {                                                \
    if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_SEND_SCHEDULED) {                 \
        PxSocket_TrySendScheduled(c);                                    \
        goto end;                                                        \
    }                                                                    \
} while (0)

#define MAYBE_RECV() do {                                                \
    if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_HAS_DATA_RECEIVED) {              \
        PxSocket_TryRecv(c);                                             \
        goto end;                                                        \
    }                                                                    \
} while (0)

#define MAYBE_SHUTDOWN_SEND_OR_RECV() do {                               \
    if (!(Px_SOCKFLAGS(s) & Px_SOCKFLAGS_HAS_DATA_RECEIVED)) {           \
        if (shutdown(s->sock_fd, SD_RECEIVE) == SOCKET_ERROR)            \
            PxSocket_WSAERROR("shutdown(SD_RECEIVE)");                   \
    } else if (PxSocket_HAS_SHUTDOWN_SEND(s)) {                          \
        if (shutdown(s->sock_fd, SD_SEND) == SOCKET_ERROR)               \
            PxSocket_WSAERROR("shutdown(SD_SEND)");                      \
    }                                                                    \
} while (0)

#ifdef Py_DEBUG
#define CHECK_SEND_RECV_CALLBACK_INVARIANTS() do {                       \
    if (!c->io_nbytes)                                                   \
        assert(c->io_result != NO_ERROR);                                \
                                                                         \
    if (c->io_result == NO_ERROR)                                        \
        assert(c->io_nbytes > 0);                                        \
    else                                                                 \
        assert(!c->io_nbytes);                                           \
} while (0)
#else
#define CHECK_SEND_RECV_CALLBACK_INVARIANTS() /* no-op */
#endif

#define PxSocket_RECYCLE(s) do {                                         \
    PxSocket_Recycle(&s, FALSE);                                         \
    if (s) {                                                             \
        if (PxSocket_IS_SERVERCLIENT(s))                                 \
            goto do_accept;                                              \
        else                                                             \
            __debugbreak();                                              \
    } else                                                               \
        goto end;                                                        \
} while (0)

#define PxSocket_FATAL() do {                                            \
    assert(PyErr_Occurred());                                            \
    PxSocket_HandleException(c, "", 1);                                  \
    goto end;                                                            \
} while (0)


#define PxSocket_EXCEPTION() do {                                        \
    assert(PyErr_Occurred());                                            \
    PxSocket_HandleException(c, "", 0);                                  \
    goto end;                                                            \
} while (0)

#define PxSocket_SYSERROR(n) do {                                        \
    PyErr_SetFromWindowsErr(0);                                          \
    PxSocket_HandleException(c, n, 1);                                   \
    goto end;                                                            \
} while (0)

#define PxSocket_WSAERROR(n) do {                                        \
    DWORD wsa_error = WSAGetLastError();                                 \
    PyErr_SetFromWindowsErr(wsa_error);                                  \
    PxSocket_HandleException(c, n, 1);                                   \
    goto end;                                                            \
} while (0)

#define PxSocket_OVERLAPPED_ERROR(n) do {                                \
    PyErr_SetFromWindowsErr(s->wsa_error);                               \
    PxSocket_HandleException(c, n, 1);                                   \
    goto end;                                                            \
} while (0)

#define PxSocket_SOCKERROR(n) do {                                       \
    PxSocket_HandleError(c, op, n, WSAGetLastError());                   \
    goto end;                                                            \
} while (0)

/* Usage:
 *      PxSocket_CALL(PyObject_CallObject(func, args));
 */

#define PxSocket_CALL(exp) {                \
    result = exp;                           \
    error_occurred = PyErr_Occurred();      \
    if (result && !error_occurred)          \
        __debugbreak();                     \
    if (error_occurred && !result)          \
        __debugbreak();                     \
    if (!result)                            \
        PxSocket_EXCEPTION();               \
}

#define PxSocket_CHECK_RESULT(result) {     \
    error_occurred = PyErr_Occurred();      \
    if (result && !error_occurred)          \
        __debugbreak();                     \
    if (error_occurred && !result)          \
        __debugbreak();                     \
    if (!result)                            \
        PxSocket_EXCEPTION();               \
}


#define PxSocket2WSABUF(s) (_Py_CAST_FWD(s, LPWSABUF, PxSocket, len))

#define OL2PxSocket(ol) (_Py_CAST_BACK(ol, PxSocket *, PxSocket, overlapped))

#define PxSocket_SET_DISCONNECTED(s) do {                                \
    Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_DISCONNECTED;                        \
    Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_CONNECTED;                          \
} while (0)

#define PxSocket_CLOSE(s) do {                                           \
    (void)closesocket((SOCKET)s->sock_fd);                               \
} while (0)

#define Px_CLOSE_HANDLE(h) do {                                         \
    if (h) {                                                            \
        CloseHandle(h);                                                 \
        h = NULL;                                                       \
    }                                                                   \
} while (0)

#define Px_CLOSE_SOCKET(s) do {                                         \
    if (s) {                                                            \
        closesocket(s);                                                 \
        s = INVALID_SOCKET;                                             \
    }                                                                   \
} while (0)

#define Px_CLOSE_THREADPOOL_WORK(w) do {                                \
    if (w) {                                                            \
        CloseThreadpoolWork(w);                                         \
        w = NULL;                                                       \
    }                                                                   \
} while (0)

#define Px_CLOSE_THREADPOOL_TIMER(t) do {                               \
    if (t) {                                                            \
        CloseThreadpoolTimer(t);                                        \
        t = NULL;                                                       \
    }                                                                   \
} while (0)

#define Px_FREE(p) do {                                                 \
    if (p) {                                                            \
        free(p);                                                        \
        p = NULL;                                                       \
    }                                                                   \
} while (0)

#define Px_HEAP_DESTROY(h) do {                                         \
    if (h) {                                                            \
        HeapDestroy(h);                                                 \
        h = NULL;                                                       \
    }                                                                   \
} while (0)




static PyTypeObject PxSocket_Type;
static PyTypeObject PxSocketBuf_Type;
static PyTypeObject PxClientSocket_Type;
static PyTypeObject PxServerSocket_Type;

static PySocketModule_APIObject PySocketModule;
//static PxOdbcModule_APIObject PxOdbcModule;

#define PySocket_Type           PySocketModule.Sock_Type
#define getsockaddrarg          PySocketModule.getsockaddrarg
#define getsockaddrlen          PySocketModule.getsockaddrlen
#define makesockaddr            PySocketModule.makesockaddr
#define AcceptEx                PySocketModule.AcceptEx
#define ConnectEx               PySocketModule.ConnectEx
#define WSARecvMsg              PySocketModule.WSARecvMsg
#define WSASendMsg              PySocketModule.WSASendMsg
#define DisconnectEx            PySocketModule.DisconnectEx
#define TransmitFile            PySocketModule.TransmitFile
#define TransmitPackets         PySocketModule.TransmitPackets
#define GetAcceptExSockaddrs    PySocketModule.GetAcceptExSockaddrs

#define RIOReceive PySocketModule.rio.RIOReceive
#define RIOReceiveEx PySocketModule.rio.RIOReceiveEx
#define RIOSend PySocketModule.rio.RIOSend
#define RIOSendEx PySocketModule.rio.RIOSendEx
#define RIOCloseCompletionQueue PySocketModule.rio.RIOCloseCompletionQueue
#define RIOCreateCompletionQueue PySocketModule.rio.RIOCreateCompletionQueue
#define RIOCreateRequestQueue PySocketModule.rio.RIOCreateRequestQueue
#define RIODequeueCompletion PySocketModule.rio.RIODequeueCompletion
#define RIODeregisterBuffer PySocketModule.rio.RIODeregisterBuffer
#define RIONotify PySocketModule.rio.RIONotify
#define RIORegisterBuffer PySocketModule.rio.RIORegisterBuffer
#define RIOResizeCompletionQueue PySocketModule.rio.RIOResizeCompletionQueue
#define RIOResizeRequestQueue PySocketModule.rio.RIOResizeRequestQueue

#define PxSocket_Check(v)         (     \
    Py_TYPE(v) == &PxSocket_Type ||     \
    Py_ORIG_TYPE(v) == &PxSocket_Type   \
)
#define PxClientSocket_Check(v)   (Py_TYPE(v) == &PxClientSocket_Type)
#define PxServerSocket_Check(v)   (Py_TYPE(v) == &PxServerSocket_Type)

#define PXS2S(s) ((PySocketSockObject *)s)

#define Py_RETURN_BOOL(expr) return (              \
    ((expr) ? (Py_INCREF(Py_True), Py_True) :      \
              (Py_INCREF(Py_False), Py_False))     \
)

#define Px_PROTECTION_GUARD(o)                     \
    do {                                           \
        if (!_protected(o)) {                      \
            PyErr_SetNone(PyExc_ProtectionError);  \
            return NULL;                           \
        }                                          \
    } while (0)

#define Px_PERSISTENCE_GUARD(o)                    \
    do {                                           \
        if (!_persistent(o)) {                     \
            PyErr_SetNone(PyExc_PersistenceError); \
            return NULL;                           \
        }                                          \
    } while (0)

#define ENTERED_IO_CALLBACK()                 \
    _PyParallel_EnteredIOCallback(c,          \
                                  instance,   \
                                  overlapped, \
                                  io_result,  \
                                  nbytes,     \
                                  tp_io)

#define ENTERED_CALLBACK() _PyParallel_EnteredCallback(c, instance)

static const char *pxsocket_kwlist[] = {
    "host",
    "port",

    /* inherited from socket */
    "family",
    "type",
    "proto",

    /* socket opts */
    "no_tcp_nodelay",
    "no_exclusive_addr_use",

    /*
    "connection_made",
    "data_received",
    "lines_received",
    "eof_received",
    "connection_lost",
    "connection_closed",
    "connection_timeout",
    "connection_done",

    "exception_handler",
    "initial_connection_error",

    "initial_bytes_to_send",
    "initial_words_to_expect",
    "initial_regex_to_expect",

    "duplex",
    "line_mode",
    "wait_for_eol",
    "auto_reconnect",
    "max_line_length",
    */

    NULL
};

static const char *pxsocket_protocol_attrs[] = {
    "connection_made",
    "data_received",
    "lines_received",
    "eof_received",
    "connection_lost",
    "connection_closed",
    "connection_timeout",
    "connection_done",

    "exception_handler",
    "initial_connection_error",

    "initial_bytes_to_send",
    "initial_words_to_expect",
    "initial_regex_to_expect",

    "line_mode",
    "wait_for_eol",
    "auto_reconnect",
    "max_line_length",
    NULL
};

static const char *pxsocket_kwlist_formatstring = \
    /* optional below */
    "|"

    /* endpoint */
    "s#"    /* host + len */
    "i"     /* port */

    /* base */
    "i"     /* family */
    "i"     /* type */
    "i"     /* proto */

    /* socket options */
    "p"     /* no_tcp_nodelay */
    "p"     /* no_exclusive_addr_use */

    ":socket";

    /* extensions */

//    "O"     /* connection_made */
//    "O"     /* data_received */
//    "O"     /* lines_received */
//    "O"     /* eof_received */
//    "O"     /* connection_lost */
//    "O"     /* connection_closed */
//    "O"     /* connection_timeout */
//    "O"     /* connection_done */
//
//    "O"     /* exception_handler */
//    "O"     /* initial_connection_error */
//
//    "O"     /* initial_bytes_to_send */
//    "O"     /* initial_words_to_expect */
//    "O"     /* initial_regex_to_expect */
//
//    "p"     /* duplex */
//    "p"     /* line_mode */
//    "p"     /* wait_for_eol */
//    "p"     /* auto_reconnect */
//    "i"     /* max_line_length */
//
//    ":socket";

#define PxSocket_PARSE_ARGS                  \
    args,                                    \
    kwds,                                    \
    pxsocket_kwlist_formatstring,            \
    (char **)pxsocket_kwlist,                \
    &host,                                   \
    &hostlen,                                \
    &(s->port),                              \
    &(s->sock_family),                       \
    &(s->sock_type),                         \
    &(s->sock_proto),                        \
    &(s->no_tcp_nodelay),                    \
    &(s->no_exclusive_addr_use)
    /*
    &(s->handler)
    &(s->connection_made),                   \
    &(s->data_received),                     \
    &(s->data_sent),                         \
    &(s->send_failed),                       \
    &(s->lines_received),                    \
    &(s->eof_received),                      \
    &(s->connection_lost),                   \
    &(s->exception_handler),                 \
    &(s->initial_connection_error),          \
    &(s->initial_bytes_to_send),             \
    &(s->initial_words_to_expect),           \
    &(s->initial_regex_to_expect),           \
    &(s->line_mode),                         \
    &(s->wait_for_eol),                      \
    &(s->auto_reconnect),                    \
    &(s->max_line_length)
    */

#define PxSocket_XINCREF(s) do {             \
    if (Py_PXCTX())                          \
        break;                               \
    Py_XINCREF(s->protocol);                 \
    Py_XINCREF(s->connection_made);          \
    Py_XINCREF(s->data_received);            \
    Py_XINCREF(s->data_sent);                \
    Py_XINCREF(s->lines_received);           \
    Py_XINCREF(s->eof_received);             \
    Py_XINCREF(s->connection_lost);          \
    Py_XINCREF(s->exception_handler);        \
    Py_XINCREF(s->initial_connection_error); \
    Py_XINCREF(s->initial_bytes_to_send);    \
    Py_XINCREF(s->initial_words_to_expect);  \
    Py_XINCREF(s->initial_regex_to_expect);  \
} while (0)

#define PxSocket_XDECREF(s) do {             \
    if (Py_PXCTX())                          \
        break;                               \
    Py_XDECREF(s->protocol);                 \
    Py_XDECREF(s->connection_made);          \
    Py_XDECREF(s->data_received);            \
    Py_XDECREF(s->data_sent);                \
    Py_XDECREF(s->lines_received);           \
    Py_XDECREF(s->eof_received);             \
    Py_XDECREF(s->connection_lost);          \
    Py_XDECREF(s->exception_handler);        \
    Py_XDECREF(s->initial_connection_error); \
    Py_XDECREF(s->initial_bytes_to_send);    \
    Py_XDECREF(s->initial_words_to_expect);  \
    Py_XDECREF(s->initial_regex_to_expect);  \
} while (0)

//C_ASSERT(sizeof(PxSocket) == Px_PAGE_SIZE);

typedef struct _PxClientSocket {
    PxSocket _pxsocket;

    /* attributes */
    int auto_reconnect;
} PxClientSocket;

typedef struct _PxServerSocket {
    PxSocket _pxsocket;

    /* attributes */
    int auto_reconnect;
} PxServerSocket;

typedef struct _PxAddrInfo {
    PyObject_HEAD


} PxAddrInfo;

#define ASSERT_UNREACHABLE() (assert(0 == "unreachable code"))
#define XXX_IMPLEMENT_ME() (assert(0 == "not yet implemented"))

#ifndef Py_LIMITED_API
typedef struct _PyXList {
    PyObject_HEAD
    PxListHead *head;
    HANDLE heap_handle;
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cv;
} PyXListObject;
#endif

PyAPI_DATA(PyTypeObject) PyXList_Type;

#define PyXList_Check(op) PyObject_TypeCheck(op, &PyXList_Type)
#define PyXList_CheckExact(op) (Py_TYPE(op) == &PyXList_Type)

/* Create a new, empty xlist. */
PyAPI_FUNC(PyObject *)  PyXList_New(void);

PyAPI_FUNC(int) PyXList_Clear(PyObject *op);

/* Pops the first object off the xlist. */
PyAPI_FUNC(PyObject *)  PyXList_Pop(PyObject *xlist);

/* Push a PyObject *op onto xlist.  0 on success, -1 on error. */
PyAPI_FUNC(int) PyXList_Push(PyObject *xlist, PyObject *op);

/* Flush an entire xlist in a single interlocked operation.  Returns a tuple
 * with all elements. */
PyAPI_FUNC(PyObject *) PyXList_Flush(PyObject *xlist);

/* Returns the number of elements in the xlist.  On Windows, this will only
 * return a max ushort (2^16/65536), even if there are more than 2^16 entries
 * in the list. */
PyAPI_FUNC(Py_ssize_t) PyXList_Size(PyObject *);

#ifdef __cpplus
}
#endif

#endif /* PYPARALLEL_PRIVATE_H */

/* vim:set ts=8 sw=4 sts=4 tw=78 et nospell: */
