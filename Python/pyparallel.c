#include "Python.h"

#ifdef __cpplus
extern "C" {
#endif

#include <fcntl.h>

#include "pyparallel_private.h"
#include "fileio.h"
#include "frameobject.h"
#include "structmember.h"

/* Toggle to 1 to assist with tracing PxSocket_IOLoop(). */
#if 0
#define ODS(s) OutputDebugString(s)
#else
#define ODS(s)
#endif

#define CS_SOCK_SPINCOUNT 4

Py_CACHE_ALIGN
Py_TLS Context *ctx = NULL;
Py_TLS Context *tmp_ctx = NULL; /* A temp context to use for error handling
                                   if an exception occurs in a parallel ctx
                                   whilst trying to create a new context */
Py_TLS TLS tls;
Py_TLS PyThreadState *TSTATE;
Py_TLS HANDLE heap_override;
Py_TLS void *last_heap_override_malloc_addr;
Py_TLS void *last_context_heap_malloc_addr;

Py_TLS static int _PyParallel_BreakOnNextException;

Py_TLS static int _PxNewThread = 1;
Py_TLS static long _PyParallel_ThreadSeqId = 0;
Py_TLS static PyObject *_PyParallel_ThreadSeqIdObj = NULL;

volatile long _PyParallel_NextThreadSeqId = 0;

//Py_CACHE_ALIGN
long Py_MainThreadId  = -1;
long Py_MainProcessId = -1;
long Py_ParallelContextsEnabled = -1;
size_t _PxObjectSignature = -1;
size_t _PxSocketSignature = -1;
size_t _PxSocketBufSignature = -1;
int _PxBlockingCallsThreshold = 20;

int _Py_CtrlCPressed = 0;
int _Py_InstalledCtrlCHandler = 0;

static int _PyParallel_RegisteredIOAvailable = 0;

volatile long _PyParallel_SEH_EAV_InIoCallback = 0;
volatile int _PyParallel_Finalized = 0;

volatile long _PyParallel_EAV_In_PyErr_PrintEx;

int _PxSocketServer_PreallocatedSockets = 64;
int _PxSocket_MaxSyncSendAttempts = 3;
int _PxSocket_MaxSyncRecvAttempts = 3;
int _PxSocket_MaxRecvBufSize = 65536;
int _PxSocket_CriticalSectionSpinCount = CS_SOCK_SPINCOUNT;
int _PxSocket_ListenBacklog = SOMAXCONN;

int _PyTLSHeap_DefaultSize = Px_DEFAULT_TLS_HEAP_SIZE;

int _PxSocket_SendListSize = 30;

volatile int _PxSocket_ActiveHogs = 0;
volatile int _PxSocket_ActiveIOLoops = 0;
int _PyParallel_NumCPUs = 0;

static PyObject *PyExc_AsyncError;
static PyObject *PyExc_ProtectionError;
static PyObject *PyExc_UnprotectedError;
static PyObject *PyExc_AssignmentError;
static PyObject *PyExc_NoWaitersError;
static PyObject *PyExc_WaitError;
static PyObject *PyExc_WaitTimeoutError;
static PyObject *PyExc_AsyncIOBuffersExhaustedError;
static PyObject *PyExc_InvalidFileRangeError;
static PyObject *PyExc_FileTooLargeError;
static PyObject *PyExc_PersistenceError;
static PyObject *PyExc_LowMemory;
static PyObject *PyExc_SoftMemoryLoadLimitHit;
static PyObject *PyExc_HardMemoryLoadLimitHit;

Py_CACHE_ALIGN
/* Point at which socket servers will start aggressively disconnecting
   idle clients in order to free up more sockets. */
static short _PyParallel_SoftMemoryLoadLimit = 75;
/* Point at which no more accepts/connects can be posted. */
static short _PyParallel_HardMemoryLoadLimit = 90;
static short _PyParallel_CurrentMemoryLoad;

static BOOL _PyParallel_LowMemory = FALSE;

static MEMORYSTATUSEX _memory_status;

int
PyPx_EnableTLSHeap(void)
{
    if (!Py_PXCTX())
        return 0;

    _PyParallel_EnableTLSHeap();
    return 1;
}

int
PyPx_DisableTLSHeap(void)
{
    if (!Py_PXCTX())
        return 0;

    _PyParallel_DisableTLSHeap();
    return 1;
}

int
_PyParallel_IsFinalized(void)
{
    return _PyParallel_Finalized;
}


void
_PyParallel_SetDebugbreakOnNextException(void)
{
    _PyParallel_BreakOnNextException = 1;
}

void
_PyParallel_ClearDebugbreakOnNextException(void)
{
    _PyParallel_BreakOnNextException = 0;
}

int
_PyParallel_IsDebugbreakOnNextExceptionSet(void)
{
    return _PyParallel_BreakOnNextException;
}

void
_PyParallel_MaybeFreeObject(void *vp)
{
    PyObject *o = (PyObject *)vp;
    Context *oc;
    PxSocket *os;
    PxObject *ox;
    HANDLE oh;

    /* a = "active" */
    PxSocket *as;
    HANDLE ah;

    /* Validate incoming object. */

    if (!o)
        __debugbreak();

    if (!Py_PXCTX())
        __debugbreak();

    if (!Px_CLONED(o))
        __debugbreak();

    if (!_PyParallel_IsHeapOverrideActive())
        __debugbreak();

    ox = (PxObject *)o->px;
    if (!ox)
        __debugbreak();

    oc = ox->ctx;
    if (!oc)
        __debugbreak();

    os = (PxSocket *)oc->io_obj;
    if (!os)
        __debugbreak();

    oh = os->heap_override;
    if (!oh)
        __debugbreak();

    /* End of incoming object validation. */

    /* Set up the active values. */
    as = (PxSocket *)ctx->io_obj;
    if (!as)
        __debugbreak();

    ah = as->heap_override;
    if (!ah)
        __debugbreak();

    /* Verify the object came from the active heap override. */
    if (oh != ah)
        __debugbreak();

    HeapFree(ah, 0, o);
}

static __inline
void
CheckListEntry(PLIST_ENTRY entry)
{
    if ((((entry->Flink)->Blink) != entry) ||
        (((entry->Blink)->Flink) != entry))
        __debugbreak();
}

static __inline
BOOLEAN
IsListEmpty(PLIST_ENTRY head)
{
    CheckListEntry(head);
    return (BOOL)(head->Flink == head);
}

static __inline
void
InitializeListHead(PLIST_ENTRY head)
{
    head->Flink = head->Blink = head;
}

static __inline
BOOLEAN
RemoveEntryList(PLIST_ENTRY entry)
{
    PLIST_ENTRY prev;
    PLIST_ENTRY next;

    next = entry->Flink;
    prev = entry->Blink;

#ifdef Py_DEBUG
    if ((next->Blink != entry) || (prev->Flink != entry))
        __debugbreak();
#endif

    entry->Flink = NULL;
    entry->Blink = NULL;

    prev->Flink = next;
    next->Blink = prev;
    return (BOOLEAN)(prev == next);
}

static __inline
void
InsertHeadList(PLIST_ENTRY head, PLIST_ENTRY entry)
{
    PLIST_ENTRY next;
    CheckListEntry(head);
    next = head->Flink;
    entry->Flink = next;
    entry->Blink = head;
    if (next->Blink != head)
        __debugbreak();
    next->Blink = entry;
    head->Flink = entry;
    CheckListEntry(entry);
    CheckListEntry(next);
}

static __inline
PLIST_ENTRY
RemoveHeadList(PLIST_ENTRY head)
{
    PLIST_ENTRY entry, next;
    CheckListEntry(head);
    entry = head->Flink;
    next = entry->Flink;
    head->Flink = next;
    next->Blink = head;
    CheckListEntry(next);

    entry->Flink = NULL;
    entry->Blink = NULL;
    return entry;
}

static __inline
void
InsertTailList(PLIST_ENTRY head, PLIST_ENTRY entry)
{
    PLIST_ENTRY prev;
    CheckListEntry(head);
    prev = head->Blink;
    if (prev->Flink != head)
        __debugbreak();
    entry->Flink = head;
    entry->Blink = prev;
    prev->Flink = entry;
    head->Blink = entry;
    CheckListEntry(entry);
    CheckListEntry(head);
    return;
}

static __inline
void
AppendTailList(PLIST_ENTRY head, PLIST_ENTRY entry)
{
    PLIST_ENTRY end;
    CheckListEntry(head);
    end = head->Blink;
    head->Blink->Flink = entry;
    head->Blink = entry->Blink;
    entry->Blink->Flink = head;
    entry->Blink = end;
}

/* 0 on error */
static
char
_PyParallel_RefreshMemoryLoad(void)
{
    static HANDLE low_mem;
    MEMORYSTATUSEX *ms;
    Py_GUARD();

    ms = &_memory_status;

    if (!low_mem)
        low_mem = CreateMemoryResourceNotification(
            LowMemoryResourceNotification
        );

    if (!QueryMemoryResourceNotification(low_mem, &_PyParallel_LowMemory)) {
        PyErr_SetFromWindowsErr(0);
        return 0;
    }

    ms->dwLength = sizeof(_memory_status);
    if (!GlobalMemoryStatusEx(ms)) {
        PyErr_SetFromWindowsErr(0);
        return 0;
    }

    _PyParallel_CurrentMemoryLoad = (short)ms->dwMemoryLoad;

    return 1;
}

static
int
_PyParallel_HitSoftMemoryLimit(void)
{
    return (
        _PyParallel_CurrentMemoryLoad >= _PyParallel_SoftMemoryLoadLimit &&
        _PyParallel_CurrentMemoryLoad <  _PyParallel_HardMemoryLoadLimit
    );
}

static
int
_PyParallel_HitHardMemoryLimit(void)
{
    return _PyParallel_CurrentMemoryLoad >= _PyParallel_HardMemoryLoadLimit;
}

void *_PyHeap_Malloc(Context *c, size_t n, size_t align, int no_realloc);
void *_PyTLSHeap_Malloc(size_t n, size_t align);

int
_PyParallel_IsHeapOverrideActive(void)
{
    return (heap_override != NULL);
}

static
void
_PyParallel_SetHeapOverride(HANDLE heap_handle)
{
    assert(!_PyParallel_IsHeapOverrideActive());
    heap_override = heap_handle;
}

HANDLE
_PyParallel_GetHeapOverride(void)
{
    return heap_override;
}


static
void
_PyParallel_RemoveHeapOverride(void)
{
    assert(_PyParallel_IsHeapOverrideActive());
    heap_override = NULL;
}

/* tls-oriented stuff */
#define Px_TLS_HEAP_ACTIVE (tls.heap_depth > 0)

int
_PyParallel_IsTLSHeapActive(void)
{
    return (!Py_PXCTX() ? 0 : Px_TLS_HEAP_ACTIVE);
}

int
_PyParallel_GetTLSHeapDepth(void)
{
    return (!Py_PXCTX() ? 0 : tls.heap_depth);
}

void
_PyParallel_EnableTLSHeap(void)
{
    TLS     *t = &tls;
    Context *c = ctx;

    if (!Py_PXCTX())
        return;

    if (++t->heap_depth > 1)
        /* Heap already active. */
        return;

    if (t->heap_depth < 0)
        Py_FatalError("PyParallel_EnableTLSHeap(): heap depth overflow");

    assert(t->heap_depth == 1);
    assert(!t->ctx_heap);
    assert(c->h != t->h);

    t->ctx_heap = c->h;
    c->h = t->h;
}

void
_PyParallel_DisableTLSHeap(void)
{
    TLS     *t = &tls;
    Context *c = ctx;

    if (!Py_PXCTX())
        return;

    if (--t->heap_depth > 0)
        return;

    if (t->heap_depth < 0)
        Py_FatalError("PyParallel_DisableTLSHeap: negative heap depth");

    assert(t->heap_depth == 0);
    assert(t->ctx_heap);

    /* If c->h isn't pointing at t->h, the TLS heap was probably resized,
     * which means c->h *should* match the t->h->sle_prev corresponding
     * to the difference in the IDs. */
    if (c->h != t->h) {
        Heap *h = t->h;
        int distance = t->h->id - c->h->id;
        assert(distance >= 1);
        while (distance--)
            h = h->sle_prev;
        if (c->h != h)
            __debugbreak();
    }
    assert(c->h != t->ctx_heap);

    c->h = t->ctx_heap;
    t->ctx_heap = NULL;
}

#define _TMPBUF_SIZE 1024

#ifdef _WIN64
#define _tls_bitscan_fwd        _BitScanForward64
#define _tls_bitscan_rev        _BitScanReverse64
#define _tls_interlocked_or     _InterlockedOr64
#define _tls_interlocked_and    _InterlockedAnd64
#define _tls_popcnt             _Py_popcnt_u64
#else
#define _tls_bitscan_fwd        _BitScanForward
#define _tls_bitscan_rev        _BitScanReverse
#define _tls_interlocked_or     _InterlockedOr
#define _tls_interlocked_and    _InterlockedAnd
#define _tls_popcnt             _Py_popcnt_u32
#endif

static
PyThreadState *
get_main_thread_state(void)
{
    PyThreadState *tstate;

    tstate = (PyThreadState *)_Py_atomic_load_relaxed(&_PyThreadState_Current);
    if (!tstate)
        tstate = TSTATE;
    //assert(tstate);
    return tstate;
}

PyThreadState *
_PyParallel_GetCurrentThreadState(void)
{
    return get_main_thread_state();
}

static
PyThreadState *
get_main_thread_state_old(void)
{
    PyThreadState *tstate;

    tstate = (PyThreadState *)_Py_atomic_load_relaxed(&_PyThreadState_Current);
    if (!tstate) {
        _Py_lfence();
        _Py_clflush(&_PyThreadState_Current);
        tstate = (PyThreadState *)_PyThreadState_Current._value;
    }
    if (!tstate) {
        OutputDebugString(L"get_main_thread_state(2): !tstate\n");
        tstate = PyGILState_GetThisThreadState();
        if (!tstate) {
            OutputDebugString(L"get_main_thread_state(3): !tstate\n");

            if (Py_PXCTX()) {
                PyThreadState *ts1, *ts2;
                ts1 = tls.px->tstate;
                ts2 = ctx->px->tstate;
                assert(ts1 == ts2);
                tstate = ts1;
                _PyThreadState_Current._value = tstate;
            }
        }
    }
    if (!tstate) {
        OutputDebugString(L"get_main_thread_state(4): !tstate!\n");
        assert(TSTATE);
        tstate = TSTATE;
    }
    /*
    assert(tstate);
    */
    return tstate;
    /*
    return (PyThreadState *)_Py_atomic_load_relaxed(&_PyThreadState_Current);
    */
}


static
PxState *
PXSTATE(void)
{
    PxState *px;
    PyThreadState *pstate = get_main_thread_state();
    if (!pstate) {
        OutputDebugString(L"_PyThreadState_Current == NULL!\n");
        if (Py_PXCTX())
            px = ctx->px;
    } else
        px = (PxState *)pstate->px;
    if (!px) {
        OutputDebugString(L"!px");
    }
    if ((Px_PTR(px) & 0x0000000800000000) == 0x0000000800000000) {
        OutputDebugString(L"px -> 0x8..\n");
    }
    /*
    assert(px);
    assert((Px_PTR(px) & 0x0000000800000000) != 0x0000000800000000);
    */
    return px;
}

void
_PxContext_HeapSnapshot(Context *c, Heap *snapshot)
{
    memcpy(snapshot, c->h, sizeof(Heap));
}

Heap *
PxContext_HeapSnapshot(Context *c)
{
    assert(!c->s);
    c->s = &c->snapshot;
    _PxContext_HeapSnapshot(c, c->s);
    return c->s;
}

/* Ensure (assert/break) that the given heap is in the initial state. */
void
_PyHeap_EnsureReset(Heap *h)
{
    size_t aligned_sizeof_heap = Px_ALIGN(sizeof(Heap), Px_PTR_ALIGN_SIZE);
    size_t remaining = h->remaining;
    size_t allocated = h->allocated;
    size_t size = h->size;

    size_t expected_remaining = size - aligned_sizeof_heap;
    size_t expected_allocated = aligned_sizeof_heap;

    void *expected_next = Px_PTR_ADD(h->base, h->allocated);

    if (remaining != expected_remaining)
        __debugbreak();

    if (allocated != expected_allocated)
        __debugbreak();

    if (h->next != expected_next)
        __debugbreak();
}

void
_PyHeap_Reset(volatile Heap *h)
{
    size_t aligned_sizeof_heap = Px_ALIGN(sizeof(Heap), Px_PTR_ALIGN_SIZE);
    h->remaining = h->size - aligned_sizeof_heap;
    h->allocated = aligned_sizeof_heap;
    h->next = Px_PTR_ADD(h->base, aligned_sizeof_heap);
    h->next_alignment = Px_GET_ALIGNMENT(h->next);

    SecureZeroMemory(h->next, h->remaining);

    /* Eh, I can't be bothered figuring out the logic for calculating the
     * offsets such that this could be done via a ZeroMemory()/memset();
     * just reset counters manually for now. */
    h->mallocs = 1; /* 1 = the _PyHeap_Malloc() for sle_next. */
    /* Everything else gets zeroed. */
    h->deallocs = 0;
    h->mem_reallocs = 0;
    h->obj_reallocs = 0;
    h->resizes = 0;
    h->frees = 0;
    h->alignment_mismatches = 0;
    h->bytes_wasted = 0;
}


void
_PxContext_Rewind(Context *c, Heap *snapshot)
{
    Heap *s = snapshot;

    /*
     * The distance between heaps will be reflected by the difference in the
     * current context's heap ID and the snapshot heap ID.  (Distance in this
     * case refers to the number of links in the linked list.)
     */
    int distance = c->h->id - s->id;

    assert(s->ctx == c);

    if (distance >= 1) {
        Heap *h;
        assert(c->h->base != s->base);

        /* Reverse through the heaps N times, where N is the difference in IDs
         * between the current heap's ID and the snapshot heap's ID.  Then
         * check that the pointers line up, then repeat the reversal, but
         * reset each heap as we go along. */
        h = c->h;
        while (distance--)
            h = h->sle_prev;

        if (s->id != h->id) {
#ifdef Py_DEBUG
            __debugbreak();
#else
            Py_FatalError("_PxContext_Rewind: broken linked list?");
#endif
        }

        /* Pointers line up, do the loop again but reset the heaps as we go. */
        distance = c->h->id - s->id;
        /* Grab a copy of c->h before we start messing with it; useful during
         * debugging. */
        h = c->h;
        while (distance--) {
            if (distance > 1)
                _m_prefetchw(c->h->sle_prev);
            _PyHeap_Reset(c->h);
            _PyHeap_EnsureReset(c->h);
            c->h = c->h->sle_prev;
        }
    } else if (c->h->allocated == s->allocated) {
        /* Snapshot already matches position of current heap, nothing to
         * rewind. */
        assert(c->h->remaining == s->remaining);
        assert(c->h->next == s->next);
        return;
    }

    if (c->h->base != s->base)
        __debugbreak();

    if (c->h->id != s->id)
        __debugbreak();

    /* Heap snapshot lines up with context's current heap, so we can now
     * memcpy the snapshot back over the active heap. */
    memcpy(c->h, s, sizeof(Heap));
}

void
PxContext_RollbackHeap(Context *c, Heap **snapshot)
{
    _PxContext_Rewind(c, *snapshot);
    *snapshot = NULL;
    c->s = NULL;
}

PyObject *
create_pxsocket(
    PyObject *args,
    PyObject *kwds,
    ULONGLONG flags,
    PxSocket *parent,
    Context *use_this_context
);

void PxOverlapped_Reset(void *ol);

void
PxSocket_ResetBuffers(PxSocket *s)
{
    PyBytesObject *bytes;

    /* Clear sbuf. */
    s->sbuf = NULL;

    /* Reset rbuf. */
    bytes = R2B(s->rbuf);

    /* Clear the entire receive buffer. */
    SecureZeroMemory(&bytes->ob_sval[0], s->recvbuf_size);

    /* Set ob_size back to 0. */
    Py_SIZE(bytes) = 0;

    /* The WSABUF's len gets set to the recvbuf_size. */
    s->rbuf->w.len = s->recvbuf_size;
    s->rbuf->w.buf = (char *)&s->rbuf->ob_sval[0];
}

void
PxSocket_Reuse(PxSocket *s)
{
    ULONGLONG flags;
    PxSocket old;
    memcpy(&old, s, sizeof(PxSocket));

    /* Invariants... */
    if (!s->reused_socket)
        __debugbreak();

    if (s->sock_fd == INVALID_SOCKET)
        __debugbreak();

    if (!s->tp_io)
        __debugbreak();

    if (!s->startup_heap_snapshot)
        __debugbreak();

    if (!s->startup_socket_snapshot)
        __debugbreak();

    if (!s->startup_socket_flags)
        __debugbreak();

    /* End of invariants. */

    /* Copy the startup socket snapshot over the socket. */
    memcpy(s, s->startup_socket_snapshot, sizeof(PxSocket));

    /* Copy the state of the critical section back. */
    memcpy(&old.cs, &s->cs, sizeof(CRITICAL_SECTION));

    /* Reset the startup socket flags. */
    /* xxx: wait, I don't think we need the startup flag logic here... the
     * initial snapshot at the end of create_pxsocket() should persist the
     * flags in a pristine state.
     */
    flags = s->startup_socket_flags;
    if (flags != s->flags)
        __debugbreak();

    if (old.ctx != s->ctx)
        __debugbreak();

    if (old.ctx != s->startup_heap_snapshot->ctx)
        __debugbreak();

    if (old.ctx != s->rbuf->ctx)
        __debugbreak();

    if (s->rbuf->s != s)
        __debugbreak();

    /* If we wanted to be really clever, we could track the average number of
     * heaps (and thus, total memory allocated) in the parent/listen socket
     * for a given client socket lifetime.  Then, when it comes time to rewind
     * the context here (which reverses through all the heaps and resets
     * them), we could see if this particular instance allocated an abnormal
     * amount of heaps and if so, free the ones above the average amount.
     * This would be useful if the underlying protocol will consume large
     * amounts of memory (temporarily) in some (but not all) conditions.
     */
    _PxContext_Rewind(s->ctx, s->startup_heap_snapshot);

    PxSocket_ResetBuffers(s);

    //PxOverlapped_Reset(&s->rbuf->ol);

    s->reused = 1;
    s->recycled = 0;
}

/*
 * "Recycle" a socket; either reset the context and create a new socket(), or
 * literally re-use the existing s->sock_fd.
 */
void
PxSocket_Recycle(PxSocket **sp, BOOL force)
{
    PxSocket *s = *sp;
    PxSocket *new_socket;
    Context *c = s->ctx;
    PxSocket *parent = s->parent;
    TP_IO *tp_io = s->tp_io;
    int num_accepts_to_post;

    if (s->parent && s->parent->shutting_down) {
        *sp = NULL;
        return;
    }

    ODS(L"recycling...\n");

    if (force)
        goto do_recycle;

    /* Invariant test all the things! */
    if (s->was_accepting) {
        if (s->was_connected)
            __debugbreak();
        else if (s->was_disconnecting)
            __debugbreak();
        if (PxSocket_IS_SERVERCLIENT(s))
            InterlockedDecrement(&s->parent->accepts_posted);
    } else if (s->was_connecting) {
        if (s->was_accepting)
            __debugbreak();
        else if (s->was_disconnecting)
            __debugbreak();
        else if (s->was_connected)
            __debugbreak();
        else if (PxSocket_IS_SERVERCLIENT(s))
            __debugbreak();
    } else if (s->was_connected) {
        if (s->was_accepting)
            __debugbreak();
        else if (s->was_disconnecting)
            __debugbreak();
        else if (s->was_connecting)
            __debugbreak();
        if (s->parent)
            InterlockedDecrement(&s->parent->clients_connected);
    } else if (s->was_disconnecting) {
        if (s->was_accepting)
            __debugbreak();
        else if (s->was_connecting)
            __debugbreak();
        else if (s->was_connected)
            __debugbreak();
        if (s->parent)
            InterlockedDecrement(&s->parent->clients_disconnecting);
    }

    /* We don't do anything re: recycling clients at the moment. */
    if (PxSocket_IS_CLIENT(s)) {
        PxSocket_CallbackComplete(s);
        *sp = NULL;
        return;
    }

    if (WaitForSingleObject(s->parent->accepts_sem, 0) == WAIT_TIMEOUT) {
        PxSocketServer_UnlinkChild(s);
        *sp = NULL;
        return;
    }

    //ResetEvent(s->parent->fd_accept);

    goto do_recycle;

    /* Old approach below. */
    _Py_lfence();
    //_Py_clflush(&s->parent->accepts_posted);
    num_accepts_to_post = (
        s->parent->target_accepts_posted -
        s->parent->accepts_posted
    );
    if (num_accepts_to_post <= 0) {
        PxSocketServer_UnlinkChild(s);
        *sp = NULL;
        return;
    }


do_recycle:
    if (s->reused_socket) {
        int failed;
        int errval = NO_ERROR;
        int errlen = sizeof(int);
        failed = getsockopt(s->sock_fd,
                            SOL_SOCKET,
                            SO_ERROR,
                            (char *)&errval,
                            &errlen);
        if (failed) {
            DWORD wsa_error;
            assert(failed == SOCKET_ERROR);
            wsa_error = WSAGetLastError();
            if (wsa_error != WSAENOTSOCK)
                __debugbreak();
        }

        /* Make sure the socket didn't have an error. */
        if (errval != NO_ERROR)
            __debugbreak();

        /* Make sure wsa_error aligns with errval... */
        if (s->wsa_error != NO_ERROR)
            __debugbreak();

        /* Make sure the last operation was either a DisconnectEx() with
         * TF_REUSE_SOCKET flags, or TransmitFile() with TF_DISCONNECT and
         * TF_REUSE_SOCKET. */
        assert(s->last_io_op);
        if (s->last_io_op == PxSocket_IO_DISCONNECT) {
            if (s->disconnectex_flags != TF_REUSE_SOCKET)
                __debugbreak();
        } else if (s->last_io_op == PxSocket_IO_SENDFILE) {
            DWORD expected = TF_REUSE_SOCKET | TF_DISCONNECT;
            if (s->sendfile_flags != expected)
                __debugbreak();
        } else
            __debugbreak();

        PxSocket_Reuse(s);
        new_socket = s;
        ++c->times_reused;
        InterlockedIncrement(&s->parent->total_clients_reused);
    } else {

        /* Do an inverted test of the logic above if the socket isn't
         * indicating an error. */
        if (s->wsa_error == NO_ERROR) {
            if (s->last_io_op == PxSocket_IO_DISCONNECT) {
                if (s->disconnectex_flags == TF_REUSE_SOCKET)
                    __debugbreak();
            } else if (s->last_io_op == PxSocket_IO_SENDFILE) {
                /*
                DWORD expected = TF_REUSE_SOCKET | TF_DISCONNECT;
                if (s->sendfile_flags == expected)
                    __debugbreak();
                */
            }
        }

        new_socket = (PxSocket *)create_pxsocket(NULL, NULL, 0, NULL, c);
        if (new_socket) {
            if (new_socket->reused)
                __debugbreak();
            if (new_socket->reused_socket)
                __debugbreak();
            if (!new_socket->recycled)
                __debugbreak();
        }
        ++c->times_recycled;
        InterlockedIncrement(&s->parent->total_clients_recycled);
    }

    if (!new_socket)
        /* Eh, what should we do here? */
        __debugbreak();

    if (new_socket != s)
        __debugbreak();

    *sp = new_socket;

    return;
}

#define TLS_BUF_SPINCOUNT 8

/* 0 on failure, 1 on success */
static
int
PyObject2WSABUF(PyObject *o, WSABUF *w)
{
    int result = 1;
    if (PyBytes_Check(o)) {
        w->len = (ULONG)((PyVarObject *)o)->ob_size;
        w->buf = (char *)((PyBytesObject *)o)->ob_sval;
    } else if (PyByteArray_Check(o)) {
        w->len = (ULONG)((PyByteArrayObject *)o)->ob_alloc;
        w->buf = ((PyByteArrayObject *)o)->ob_bytes;
    } else if (PyUnicode_Check(o)) {
        Py_ssize_t nbytes;
        char *buf = PyUnicode_AsUTF8AndSize(o, &nbytes);
        if (buf) {
            w->buf = buf;
            /* xxx todo: range check */
            w->len = (DWORD)nbytes;
        } else
            result = 0;
    } else
        result = 0;

    return result;
}

/* 0 on failure, 1 on success */
/* Failure will be for one of two reasons:
 *  - PyObject2WSABUF() failed to convert `PyObject *o` into something
 *    sendable.  PyErr_Occurred() will not be set.
 *  - Out of mem.  PyErr_Occurred() will be set to PyErr_NoMemory().
 * Caller should rollback heap on failure.
 */
int
PxSocket_NEW_SBUF(
    Context *c,
    PxSocket *s,
    Heap *snapshot,
    DWORD *len,
    char *buf,
    PyObject *o,
    SBUF **sbuf,
    int copy_buf
)
{
    DWORD  nbytes;
    WSABUF w;
    SBUF *b;
    assert(!*sbuf);
    assert(snapshot);
    assert(c == ctx);
    assert(s->ctx == c);
    assert(c->io_obj == (PyObject *)s);
    assert(
        (!len && !buf &&  o) ||
        ( len &&  buf && !o && (*len > 0))
    );

    if (!o) {
        w.len = *len;
        w.buf =  buf;
    } else if (!PyObject2WSABUF(o, &w))
        return 0;

    nbytes = Px_PTR_ALIGN(sizeof(SBUF));
    if (copy_buf)
        nbytes += w.len;

    *sbuf = (SBUF *)_PyHeap_Malloc(c, nbytes, 0, 0);
    if (!*sbuf)
        return 0;

    b = *sbuf;

    b->last_thread_id = _Py_get_current_thread_id();
    b->snapshot = snapshot;
    b->s = s;
    s->sbuf = b;

    b->w.len = w.len;

    if (!copy_buf)
        b->w.buf = w.buf;
    else {
        b->w.buf = (char *)(_Py_PTR_ADD(b, Px_PTR_ALIGN(sizeof(SBUF))));
        memcpy(b->w.buf, w.buf, w.len);
    }

    return 1;
}

static
int
PxOverlapped_IsNull(void *ol)
{
    static OVERLAPPED _null;
    return memcmp(ol, &_null, sizeof(OVERLAPPED));
}

static
void
PxOverlapped_Reset(void *ol)
{
    SecureZeroMemory(ol, sizeof(OVERLAPPED));
}

int _PyObject_GenericSetAttr(PyObject *o, PyObject *n, PyObject *v);
int _PyObject_SetAttrString(PyObject *, char *, PyObject *w);

PyObject *_PyObject_GenericGetAttr(PyObject *o, PyObject *n);
PyObject *_PyObject_GetAttrString(PyObject *, char *);

void
_PyParallel_DisassociateCurrentThreadFromCallback(void)
{
    Context *c = ctx;
    if (Px_CTX_IS_DISASSOCIATED(c))
        return;

    /* Disable this logic for now... */
    return;

    DisassociateCurrentThreadFromCallback((PTP_CALLBACK_INSTANCE)c->instance);
    Px_CTXFLAGS(c) |= Px_CTXFLAGS_DISASSOCIATED;
}

void
_PyParallel_BlockingCall(void)
{
    Context *c = ctx;
    Stats   *s = STATS(c);
    Px_GUARD();

    if (Px_CTX_IS_DISASSOCIATED(c))
        return;

    if (s && ++s->blocking_calls > _PxBlockingCallsThreshold)
        _PyParallel_DisassociateCurrentThreadFromCallback();
}


void *
_PyHeap_MemAlignedMalloc(Context *c, size_t n)
{
    return _PyHeap_Malloc(c, n, Px_MEM_ALIGN_SIZE, 0);
}


PxListItem *
_PyHeap_NewListItem(Context *c)
{
    return (PxListItem *)_PyHeap_MemAlignedMalloc(c, sizeof(PxListItem));
}


PxListHead *
_PyHeap_NewList(Context *c)
{
    PxListHead *l;

    l = (PxListHead *)_PyHeap_MemAlignedMalloc(c, sizeof(PxListHead));
    if (l)
        InitializeSListHead(l);

    return l;
}


int
_PyParallel_IsParallelContext(void)
{
    long cur_thread_id = (long)_Py_get_current_thread_id();
    int active = (int)(Py_MainThreadId != cur_thread_id);
    int alternate = (int)(ctx && ctx->h ? 1 : 0);
    assert(Py_MainThreadId > 0);
    assert(Py_MainProcessId != -1);
    if (active != alternate)
        __debugbreak();
    return active;
}

void
_PyParallel_IncRef(void *vp)
{
    PyObject *op = (PyObject *)vp;
    if ((!Py_PXCTX() && (Py_ISPY(op) || Px_PERSISTED(op)))) {
        _Py_INC_REFTOTAL;
        (((PyObject*)(op))->ob_refcnt++);
    }
}

void
_PyParallel_DecRef(void *vp)
{
    PyObject *op = (PyObject *)vp;
    if (!Py_PXCTX()) {
        if (Px_PERSISTED(op) || Px_CLONED(op))
            Px_DECREF(op);
        else if (!Px_ISPX(op)) {
            /*
            Py_ssize_t refcnt = op->ob_refcnt;
            if (refcnt <= 0)
                __debugbreak();
            if (--op->ob_refcnt == 0)
                _Py_Dealloc(op);
            */
            //--_Py_RefTotal;

            if ((--((PyObject *)(op))->ob_refcnt) != 0) {
                if ((((PyObject *)(op))->ob_refcnt) < 0)
                    __debugbreak();
            } else
                _Py_Dealloc((PyObject *)(op));
        }
    }
}

Py_ssize_t
_PyParallel_RefCnt(void *vp)
{
    PyObject *op = (PyObject *)vp;
    return ((PyObject *)op)->ob_refcnt;
}

void *
_PyParallel_GetActiveContext(void)
{
    return ctx;
}

void
_PyObject_Dealloc(PyObject *o)
{
    PyTypeObject *tp;
    PyMappingMethods *mm;
    //PySequenceMethods *sm;
    destructor d;

    Py_GUARD_OBJ(o);
    Py_GUARD();

    assert(Py_ORIG_TYPE(o));

    if (Py_HAS_EVENT(o))
        PyEvent_DESTROY(o);

    tp = Py_TYPE(o);
    mm = tp->tp_as_mapping;
    //sm = tp->tp_as_sequence;
    d = Py_ORIG_TYPE_CAST(o)->tp_dealloc;
    Py_TYPE(o) = Py_ORIG_TYPE(o);
    Py_ORIG_TYPE(o) = NULL;
    (*d)(o);
    PyMem_FREE(tp);
    PyMem_FREE(mm);
    //PyMem_FREE(sm);
}


char
_protected(PyObject *obj)
{
    PyObject **dp;
    dp = _PyObject_GetDictPtr(obj);
    return (!dp ? Px_ISPROTECTED(obj) :
                  Px_ISPROTECTED(obj) && Px_ISPROTECTED(*dp));
}

PyObject *
_async_protected(PyObject *self, PyObject *obj)
{
    Py_INCREF(obj);
    Py_RETURN_BOOL(_protected(obj));
}


void
_unprotect(PyObject *obj)
{
    PyObject **dp;
    if (_protected(obj)) {
        Py_PXFLAGS(obj) &= ~Py_PXFLAGS_RWLOCK;
        obj->srw_lock = NULL;
    }
    dp = _PyObject_GetDictPtr(obj);
    if (dp && _protected(*dp)) {
        (*dp)->px_flags &= ~Py_PXFLAGS_RWLOCK;
        (*dp)->srw_lock = NULL;
    }
}

PyObject *
_async_unprotect(PyObject *self, PyObject *obj)
{
    Py_INCREF(obj);
    if (Py_ISPX(obj)) {
        PyErr_SetNone(PyExc_ProtectionError);
        return NULL;
    }
    _unprotect(obj);
    Py_RETURN_NONE;
}

PyObject *
_async_read_lock(PyObject *self, PyObject *obj)
{
    Px_PROTECTION_GUARD(obj);
    Py_INCREF(obj);
    return _read_lock(obj);
}

PyObject *
_async_read_unlock(PyObject *self, PyObject *obj)
{
    Px_PROTECTION_GUARD(obj);
    Py_INCREF(obj);
    return _read_unlock(obj);
}

PyObject *
_async_try_read_lock(PyObject *self, PyObject *obj)
{
    Px_PROTECTION_GUARD(obj);
    Py_INCREF(obj);
    Py_RETURN_BOOL(_try_read_lock(obj));
}

PyObject *
_async_write_lock(PyObject *self, PyObject *obj)
{
    Px_PROTECTION_GUARD(obj);
    Py_INCREF(obj);
    return _write_lock(obj);
}

PyObject *
_async_write_unlock(PyObject *self, PyObject *obj)
{
    Px_PROTECTION_GUARD(obj);
    Py_INCREF(obj);
    return _write_unlock(obj);
}

PyObject *
_async_try_write_lock(PyObject *self, PyObject *obj)
{
    Px_PROTECTION_GUARD(obj);
    Py_INCREF(obj);
    Py_RETURN_BOOL(_try_write_lock(obj));
}


Object *
_PyHeap_NewObject(Context *c)
{
    return (Object *)_PyHeap_Malloc(c, sizeof(Object), 0, 0);
}

#define _Px_READ_LOCK(o)    if (Py_HAS_RWLOCK(o)) _read_lock(o)
#define _Px_READ_UNLOCK(o)  if (Py_HAS_RWLOCK(o)) _read_unlock(o)
#define _Px_WRITE_LOCK(o)   if (Py_HAS_RWLOCK(o)) _write_lock(o)
#define _Px_WRITE_UNLOCK(o) if (Py_HAS_RWLOCK(o)) _write_unlock(o)

#define Py_PXCB (_PyParallel_ExecutingCallbackFromMainThread())
#define Px_XISPX(o) ((o) ? Px_ISPX(o) : 0)


int
_Px_TryPersist(PyObject *o)
{
    BOOL pending;
    Context  *c;
    PxObject *x;
    if (!o || (!(Px_ISPX(o))) || Px_PERSISTED(o))
        return 1;

    x = Py_ASPX(o);
    if (!InitOnceBeginInitialize(&(x->persist), 0, &pending, NULL)) {
        PyErr_SetFromWindowsErr(0);
        return 0;
    }
    if (!pending)
        return 1;

    assert(!(Px_PERSISTED(o)));

    c = x->ctx;
    if (!Px_CTX_IS_PERSISTED(c))
        Px_CTXFLAGS(c) |= Px_CTXFLAGS_IS_PERSISTED;

    c->persisted_count++;

    Py_PXFLAGS(o) |= Py_PXFLAGS_PERSISTED;
    Py_REFCNT(o) = 1;

    Px_CTXFLAGS(c) |= Px_CTXFLAGS_IS_PERSISTED;

    if (!InitOnceComplete(&(x->persist), 0, NULL)) {
        PyErr_SetFromWindowsErr(0);
        return 0;
    }

    return 1;
}


int
_Px_objobjargproc_ass(PyObject *o, PyObject *k, PyObject *v)
{
    if (!Py_PXCTX() && !Py_PXCB) {
        assert(!Px_ISPX(o));
        assert(!Px_ISPX(k));
        assert(!Px_XISPX(v));
    }
    if (!Px_ISPY(o) || (!Py_PXCTX() && !Py_PXCB))
        return 1;

    return !(!_Px_TryPersist(k) || !_Px_TryPersist(v));
}


int
_PyObject_GenericSetAttr(PyObject *o, PyObject *n, PyObject *v_orig)
{
    Context *c = ctx;
    PyObject *v_copy = NULL;
    PyObject *v = NULL;
    PyTypeObject *tp;
    PxSocket *s = NULL;
    int result;
    assert(Py_ORIG_TYPE(o));

    _Px_WRITE_LOCK(o);
    __try {
        tp = Py_ORIG_TYPE_CAST(o);
        if (c && c->io_obj && c->io_type == Px_IOTYPE_SOCKET) {
            s = (PxSocket *)c->io_obj;
            if (!s->heap_override) {
                s->heap_override = HeapCreate(0, 0, 0);
                if (!s->heap_override) {
                    PyErr_SetFromWindowsErr(0);
                    _Px_WRITE_UNLOCK(o);
                    return -1;
                }
            }
            _PyParallel_SetHeapOverride(s->heap_override);
            if (Py_ISPY(v_orig)) {
                /* If it's a main thread object, don't bother cloning it. */
                ;
            } else {
                if (!Py_ISPX(v_orig))
                    __debugbreak();
                v_copy = PyObject_Clone(v_orig,
                                        "unsupported clone object type");
            }
        }
        v = (v_copy ? v_copy : v_orig);
        if (tp->tp_setattro)
            result = (*tp->tp_setattro)(o, n, v);
        else
            result = PyObject_GenericSetAttr(o, n, v);

        if (_PyParallel_IsHeapOverrideActive())
            _PyParallel_RemoveHeapOverride();

    } __finally {
        _Px_WRITE_UNLOCK(o);
    }
    if (result == -1 || (!s && !_Px_objobjargproc_ass(o, n, v)))
        return -1;

    return result;
}

PyObject *
_PyObject_GenericGetAttr(PyObject *o, PyObject *n)
{
    Context *c = ctx;
    PxSocket *s = NULL;
    PyTypeObject *tp;
    PyObject *result;
    assert(Py_ORIG_TYPE(o));

    _Px_READ_LOCK(o);
    if (c && c->io_obj && c->io_type == Px_IOTYPE_SOCKET) {
        s = (PxSocket *)c->io_obj;
        s->last_getattr_name = n;
    }
    tp = Py_ORIG_TYPE_CAST(o);
    if (tp->tp_getattro)
        result = (*tp->tp_getattro)(o, n);
    else
        result = PyObject_GenericGetAttr(o, n);
    if (s)
        s->last_getattr_value = result;
    _Px_READ_UNLOCK(o);

    return result;
}

int
_PyObject_SetAttrString(PyObject *o, char *n, PyObject *v)
{
    PyTypeObject *tp;
    int result;
    assert(Py_ORIG_TYPE(o));

    _Px_WRITE_LOCK(o);
    tp = Py_ORIG_TYPE_CAST(o);
    if (tp->tp_setattr)
        result = (*tp->tp_setattr)(o, n, v);
    else {
        PyObject *s;
        s = PyUnicode_InternFromString(n);
        if (!s)
            return -1;
        result = PyObject_GenericSetAttr(o, s, v);
        Py_DECREF(s);
    }
    _Px_WRITE_UNLOCK(o);
    if (result != -1 && !_Px_objobjargproc_ass(o, NULL, v))
        result = -1;

    return result;
}

PyObject *
_PyObject_GetAttrString(PyObject *o, char *n)
{
    PyTypeObject *tp;
    PyObject *result;
    assert(Py_ORIG_TYPE(o));

    _Px_READ_LOCK(o);
    tp = Py_ORIG_TYPE_CAST(o);
    if (tp->tp_getattr)
        result = (*tp->tp_getattr)(o, n);
    else {
        PyObject *s;
        s = PyUnicode_InternFromString(n);
        if (!s)
            return NULL;
        result = PyObject_GenericGetAttr(o, s);
        Py_DECREF(s);
    }
    _Px_READ_UNLOCK(o);

    return result;
}

/* MappingMethods */
Py_ssize_t
_Px_mp_length(PyObject *o)
{
    Py_ssize_t result;
    assert(Py_ORIG_TYPE(o));
    _Px_READ_LOCK(o);
    result = Py_ORIG_TYPE_CAST(o)->tp_as_mapping->mp_length(o);
    _Px_READ_UNLOCK(o);
    return result;
}

PyObject *
_Px_mp_subcript(PyObject *o, PyObject *k)
{
    PyObject *result;
    assert(Py_ORIG_TYPE(o));
    _Px_READ_LOCK(o);
    result = Py_ORIG_TYPE_CAST(o)->tp_as_mapping->mp_subscript(o, k);
    _Px_READ_UNLOCK(o);
    return result;
}

int
_Px_mp_ass_subscript(PyObject *o, PyObject *k, PyObject *v)
{
    int result;
    assert(Py_ORIG_TYPE(o));
    _Px_WRITE_LOCK(o);
    result = Py_ORIG_TYPE_CAST(o)->tp_as_mapping->mp_ass_subscript(o, k, v);
    _Px_WRITE_UNLOCK(o);
    if (result == -1 || !_Px_objobjargproc_ass(o, k, v))
        return -1;
    return result;
}

/* SequenceMethods */
Py_ssize_t
_Px_sq_length(PyObject *o)
{
    Py_ssize_t result;
    assert(Py_ORIG_TYPE(o));
    _Px_READ_LOCK(o);
    result = Py_ORIG_TYPE_CAST(o)->tp_as_sequence->sq_length(o);
    _Px_READ_UNLOCK(o);
    return result;
}

char
_PyObject_PrepOrigType(PyObject *o, PyObject *kwds)
{
    PyMappingMethods *old_mm, *new_mm;
    /*PySequenceMethods *old_sm, *new_sm;*/

    if (!Py_ORIG_TYPE(o)) {
        PyTypeObject *type = Py_TYPE(o), *tp;
        size_t size;
        void *offset = type;
        void *m;
        int is_heap = PyType_HasFeature(type, Py_TPFLAGS_HEAPTYPE);
        int is_gc = PyType_IS_GC(type);
        int is_tracked = 0;

        if (is_heap)
            size = sizeof(PyHeapTypeObject);
        else
            size = sizeof(PyTypeObject);

        if (is_gc && is_heap) {
            size += sizeof(PyGC_Head);
            offset = _Py_AS_GC(type);
            is_tracked = _PyObject_GC_IS_TRACKED(type);
        }

        m = PyMem_MALLOC(size);
        if (!m) {
            PyErr_NoMemory();
            return 0;
        }

        if (is_tracked)
            _PyObject_GC_UNTRACK(type);

        memcpy(m, offset, size);

        if (is_gc && is_heap)
            tp = (PyTypeObject *)_Py_FROM_GC(m);
        else
            tp = (PyTypeObject *)m;

        if (is_tracked)
            _PyObject_GC_TRACK(tp);

        Py_ORIG_TYPE(o) = type;
        Py_TYPE(o) = tp;
        tp->tp_dealloc  = _PyObject_Dealloc;
        tp->tp_setattro = _PyObject_GenericSetAttr;
        tp->tp_getattro = _PyObject_GenericGetAttr;
        tp->tp_setattr  = _PyObject_SetAttrString;
        tp->tp_getattr  = _PyObject_GetAttrString;

        old_mm = type->tp_as_mapping;
        if (old_mm && old_mm->mp_subscript) {
            size = sizeof(PyMappingMethods);
            new_mm = (PyMappingMethods *)PyMem_MALLOC(size);
            if (!new_mm)
                goto free_m;

            memcpy(new_mm, old_mm, size);

            new_mm->mp_subscript = _Px_mp_subcript;

            new_mm->mp_length = (old_mm->mp_length ? _Px_mp_length : 0);
            new_mm->mp_ass_subscript = (
                old_mm->mp_ass_subscript ? _Px_mp_ass_subscript : 0
            );

            tp->tp_as_mapping = new_mm;
        }

        /*
        old_sm = type->tp_as_sequence;
        if (old_sm) {
            assert(old_sm->sq_item);
            size = sizeof(PySequenceMethods);
            new_sm = (PySequenceMethods *)PyMem_MALLOC(size);
            if (!new_sm)
                goto free_new_mm;

            memcpy(new_sm, old_sm, size);

            new_sm->sq_item = _Px_sq_item;

            new_sm->sq_length = (old_sm->sq_length ? _Px_sq_length : 0);
            new_sm->sq_concat = (old_sm->sq_concat ? _Px_sq_concat : 0);
            new_sm->sq_repeat = (old_sm->sq_repeat ? _Px_sq_repeat : 0);
            new_sm->sq_ass_item = (old_sm->sq_ass_item ? _Px_sq_ass_item : 0);
            new_sm->sq_contains = (old_sm->sq_contains ? _Px_sq_contains : 0);
            new_sm->sq_inplace_concat = (
                old_sm->sq_inplace_concat ? _Px_sq_inplace_concat : 0
            );
            new_sm->sq_inplace_repeat = (
                old_sm->sq_inplace_repeat ? _Px_sq_inplace_repeat : 0
            );

            tp->tp_as_sequence = new_sm;
        }
        */

        goto check_invariants;

        /*
    free_new_mm:
        PyMem_FREE(new_mm);
        */

    free_m:
        PyMem_FREE(m);

        PyErr_NoMemory();
        goto error;

    }

check_invariants:
    assert(Py_ORIG_TYPE(o));
    assert(Py_ORIG_TYPE_CAST(o)->tp_dealloc);
    assert(Py_ORIG_TYPE_CAST(o)->tp_dealloc != _PyObject_Dealloc);
    assert(Py_TYPE(o)->tp_dealloc  == _PyObject_Dealloc);
    assert(Py_TYPE(o)->tp_setattro == _PyObject_GenericSetAttr);
    assert(Py_TYPE(o)->tp_getattro == _PyObject_GenericGetAttr);
    assert(Py_TYPE(o)->tp_setattr  == _PyObject_SetAttrString);
    assert(Py_TYPE(o)->tp_getattr  == _PyObject_GetAttrString);
    old_mm = (Py_ORIG_TYPE_CAST(o)->tp_as_mapping);
    if (old_mm && old_mm->mp_subscript) {
        new_mm = Py_TYPE(o)->tp_as_mapping;
        assert(new_mm->mp_subscript == _Px_mp_subcript);
        assert(old_mm->mp_length ? (new_mm->mp_length == _Px_mp_length) : 1);
        assert(
            !old_mm->mp_ass_subscript ? 1 : (
                new_mm->mp_ass_subscript == _Px_mp_ass_subscript
            )
        );
    }
    /*
    old_sm = (Py_ORIG_TYPE_CAST(o)->tp_as_sequence);
    if (old_sm) {
        new_sm = Py_TYPE(o)->tp_as_sequence;
        assert(new_sm->sq_item);
        assert(new_sm->sq_item == _Px_sq_item);
        assert(old_sm->sq_length ? (new_sm->sq_length == _Px_sq_length) : 1);
        assert(old_sm->sq_concat ? (new_sm->sq_concat == _Px_sq_concat) : 1);
        assert(old_sm->sq_repeat ? (new_sm->sq_repeat == _Px_sq_repeat) : 1);
        assert(old_sm->sq_ass_item ? (new_sm->sq_ass_item == _Px_sq_ass_item) : 1);
        assert(old_sm->sq_contains ? (new_sm->sq_contains == _Px_sq_contains) : 1);
        assert(
            !old_sm->sq_inplace_concat ? 1 : (
                new_sm->sq_inplace_concat == _Px_sq_inplace_concat
            )
        );
        assert(
            !old_sm->sq_inplace_repeat ? 1 : (
                new_sm->sq_inplace_repeat == _Px_sq_inplace_repeat
            )
        );
    }
    */
    return 1;
error:
    assert(PyErr_Occurred());
    return 0;
}

char
_PyEvent_TryCreate(PyObject *o)
{
    char success = 1;
    assert(Py_HAS_RWLOCK(o));
    _write_lock(o);
    if (!Py_HAS_EVENT(o)) {
        success = 0;
        if (Py_ISPX(o))
            PyErr_SetNone(PyExc_WaitError);
        else if (!_PyObject_PrepOrigType(o, 0))
            goto done;
        else if (!PyEvent_CREATE(o))
            PyErr_SetFromWindowsErr(0);
        else {
            Py_PXFLAGS(o) |= Py_PXFLAGS_EVENT;
            success = 1;
        }
    }
done:
    if (!success)
        assert(PyErr_Occurred());
    _write_unlock(o);
    return success;
}

BOOL
_Py_HandleCtrlC(DWORD ctrltype)
{
    if (ctrltype == CTRL_C_EVENT) {
        _Py_sfence();
        _Py_CtrlCPressed = 1;
        _Py_lfence();
        _Py_clflush(&_Py_CtrlCPressed);
        return TRUE;
    }
    return FALSE;
}
PHANDLER_ROUTINE _Py_CtrlCHandlerRoutine = (PHANDLER_ROUTINE)_Py_HandleCtrlC;

int
_Py_CheckCtrlC(void)
{
    Py_GUARD();

    if (_Py_CtrlCPressed) {
        _Py_CtrlCPressed = 0;
        PyErr_SetNone(PyExc_KeyboardInterrupt);
        return 1;
    }

    return 0;
}

PyObject *
_async_wait(PyObject *self, PyObject *o)
{
    DWORD result;
    Px_PROTECTION_GUARD(o);
    Py_INCREF(o);
    if (!_PyEvent_TryCreate(o))
        return NULL;

    if (Py_PXCTX()) {
        /* This should really be handled via a threadpool wait. */
        result = WaitForSingleObject((HANDLE)o->event, INFINITE);
    } else {
        /* Stolen from _async_run_once(). */
        if (!_Py_InstalledCtrlCHandler) {
            if (!SetConsoleCtrlHandler(_Py_CtrlCHandlerRoutine, TRUE)) {
                PyErr_SetFromWindowsErr(0);
                return NULL;
            }
            _Py_InstalledCtrlCHandler = 1;
        }

        do {
            result = WaitForSingleObject((HANDLE)o->event, 100);
            if (PyErr_CheckSignals() || _Py_CheckCtrlC())
                return NULL;
            if (result == WAIT_TIMEOUT)
                continue;
            break;
        } while (1);
    }

    if (result == WAIT_OBJECT_0)
        Py_RETURN_NONE;

    else if (result == WAIT_ABANDONED)
        PyErr_SetString(PyExc_SystemError, "wait abandoned");

    else if (result == WAIT_TIMEOUT)
        PyErr_SetString(PyExc_SystemError, "infinite wait timed out?");

    else if (result == WAIT_FAILED)
        PyErr_SetFromWindowsErr(0);

    else
        PyErr_SetString(PyExc_SystemError, "unexpected result from wait");

    return NULL;
}

PyObject *
_async_prewait(PyObject *self, PyObject *o)
{
    Px_PROTECTION_GUARD(o);
    Py_INCREF(o);
    if (!_PyEvent_TryCreate(o))
        return NULL;

    Py_INCREF(o);
    return o;
}

PyObject *
_async_signal(PyObject *self, PyObject *o)
{
    PyObject *result = NULL;
    Px_PROTECTION_GUARD(o);
    Py_INCREF(o);
    _write_lock(o);

    if (!Py_HAS_EVENT(o))
        PyErr_SetNone(PyExc_NoWaitersError);
    else if (!PyEvent_SIGNAL(o))
        PyErr_SetFromWindowsErr(0);
    else
        result = Py_None;

    _write_unlock(o);
    Py_XINCREF(result);
    return result;
}

PyObject *
_async_signal_and_wait(PyObject *self, PyObject *args)
{
    PyObject *s, *w;
    PyObject *result = NULL;
    DWORD wait_result;

    if (!PyArg_UnpackTuple(args, "signal_and_wait", 2, 2, &s, &w))
        goto done;

    Px_PROTECTION_GUARD(s);
    Px_PROTECTION_GUARD(w);

    if (s == w) {
        PyErr_SetString(PyExc_WaitError,
                        "signal and wait objects must differ");
        goto done;
    }

    if (!_PyEvent_TryCreate(w))
        goto done;

    if (!Py_HAS_EVENT(s)) {
        PyErr_SetNone(PyExc_NoWaitersError);
        goto done;
    }

    Py_INCREF(s);
    Py_INCREF(w);

    wait_result = SignalObjectAndWait(Py_EVENT(s), Py_EVENT(w), INFINITE, 0);

    if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_IO_COMPLETION)
        result = Py_None;

    else if (wait_result == WAIT_ABANDONED)
        PyErr_SetString(PyExc_SystemError, "wait abandoned");

    else if (wait_result == WAIT_TIMEOUT)
        PyErr_SetString(PyExc_SystemError, "infinite wait timed out?");

    else if (wait_result == WAIT_FAILED)
        PyErr_SetFromWindowsErr(0);

    else
        PyErr_SetString(PyExc_SystemError, "unexpected result from wait");

    Py_DECREF(s);
    Py_DECREF(w);

done:
    if (!result)
        assert(PyErr_Occurred());
    else
        Py_INCREF(result);

    return result;
}


PyObject *
_protect(PyObject *obj)
{
    PyObject **dp;
    if (!obj)
        return NULL;

    if (!_protected(obj)) {
        if (!_PyObject_PrepOrigType(obj, 0))
            return NULL;
        InitializeSRWLock((PSRWLOCK)&(obj->srw_lock));
        Py_PXFLAGS(obj) |= Py_PXFLAGS_RWLOCK;
    }

    dp = _PyObject_GetDictPtr(obj);
    if (dp && *dp && !_protected(*dp)) {
        if (!_PyObject_PrepOrigType(*dp, 0)) {
            /* Manually undo the protection we applied above. */
            Py_PXFLAGS(obj) &= ~Py_PXFLAGS_RWLOCK;
            obj->srw_lock = NULL;
            return NULL;
        }
        InitializeSRWLock((PSRWLOCK)&((*dp)->srw_lock));
        Py_PXFLAGS((*dp)) |= Py_PXFLAGS_RWLOCK;
    }
    return obj;
}

PyObject *
_async_protect(PyObject *self, PyObject *obj)
{
    Py_INCREF(obj);
    /*
    if (Py_ISPX(obj)) {
        PyErr_SetNone(PyExc_ProtectionError);
        return NULL;
    }
    */
    Py_INCREF(obj);
    return _protect(obj);
}

PyObject *
_async__rawfile(PyObject *self, PyObject *obj)
{
    PyObject *raw;
    fileio   *f;

    Py_INCREF(obj);
    if (PyFileIO_Check(obj))
        return obj;

    raw = PyObject_GetAttrString(obj, "raw");
    if (!raw) {
        PyErr_SetString(PyExc_ValueError, "not an io file object");
        return NULL;
    }

    if (!PyFileIO_Check(raw)) {
        PyErr_SetString(PyExc_ValueError, "invalid type for raw attribute");
        return NULL;
    }

    f = (fileio *)raw;
    Py_INCREF(f);
    f->owner = obj;
    return (PyObject *)f;
}

int
_PxPages_LookupHeapPage(PxPages *pages, Px_UINTPTR *value, void *p)
{
    PxPages *x;
    HASH_FIND_INT(pages, value, x);
    if (x) {
        Heap *h1, *h2;
        assert(x->count >= 1 && x->count <= 2);
        h1 = x->heaps[0];
        h2 = (x->count == 2 ? x->heaps[1] : NULL);
        if (Px_PTR_IN_HEAP(p, h1) || (h2 && Px_PTR_IN_HEAP(p, h2)))
            return 1;
    }
    return 0;
}

int
PxPages_Find(PxPages *pages, void *p)
{
    int found;
    Px_UINTPTR lower, upper;

    lower = Px_PAGESIZE_ALIGN_DOWN(p, Px_PAGE_SIZE);
    upper = Px_PAGESIZE_ALIGN_UP(p, Px_PAGE_SIZE);

    found = _PxPages_LookupHeapPage(pages, &lower, p);
    if (!found && lower != upper)
        found = _PxPages_LookupHeapPage(pages, &upper, p);

    return found;
}

void
_PxPages_AddHeapPage(PxPages **pages, Px_UINTPTR *value, Heap *h)
{
    PxPages *x;
    HASH_FIND_INT(*pages, value, x);
    if (x) {
        /* Original:
        assert(x->count == 1);
        x->heaps[1] = h;
        x->count++; */
        x->heaps[x->count++] = h;
    } else {
        x = (PxPages *)malloc(sizeof(PxPages));
        x->heaps[0] = h;
        x->count = 1;
        x->base = *value;
        HASH_ADD_INT(*pages, base, x);
    }
}

void
_PxPages_RemoveHeapPage(PxPages **pages, Px_UINTPTR *value, Heap *h)
{
    PxPages *x;
    HASH_FIND_INT(*pages, value, x);
    assert(x);
    if (x->count == 1) {
        assert(x->heaps[0] == h);
        HASH_DEL(*pages, x);
        free(x);
    } else {
        assert(x->count >= 1 && x->count <= 2);
        if (x->heaps[0] == h)
            x->heaps[0] = x->heaps[1];
        else
            assert(x->heaps[1] == h);
        x->heaps[1] = NULL;
        x->count = 1;
    }
}

void
PxPages_Dump(PxPages *pages)
{
    PxPages *x, *t;
    int i = 0;
    HASH_ITER(hh, pages, x, t) {
        i++;
        printf("[%d] base: 0x%llx, count: %d\n", i, x->base, x->count);
    }
}


void
_PxState_InitPxPages(PxState *px)
{
    Py_GUARD();

    InitializeSRWLock(&px->pages_srwlock);
}

void
_PxState_RegisterHeap(PxState *px, Heap *h, Context *c)
{
    int i;

    AcquireSRWLockExclusive(&px->pages_srwlock);

    assert((h->size % h->page_size) == 0);

    for (i = 0; i < h->pages; i++) {
        void *p;
        Px_UINTPTR lower, upper;

        p = Px_PTR_ADD(h->base, (i * h->page_size));

        lower = Px_PAGESIZE_ALIGN_DOWN(p, h->page_size);
        upper = Px_PAGESIZE_ALIGN_UP(p, h->page_size);

        _PxPages_AddHeapPage(&(px->pages), &lower, h);
        if (lower != upper)
            _PxPages_AddHeapPage(&(px->pages), &upper, h);
    }
    ReleaseSRWLockExclusive(&px->pages_srwlock);
}

void
_PxContext_UnregisterHeaps(Context *c)
{
    Heap    *h;
    Stats   *s;
    PxState *px;
    int      i, heap_count = 0;

    Py_GUARD();

    px =  c->px;
    s  = &c->stats;

    AcquireSRWLockExclusive(&px->pages_srwlock);

    h = &c->heap;
    while (h) {
        heap_count++;

        assert((h->size % h->page_size) == 0);

        for (i = 0; i < h->pages; i++) {
            void *p;
            Px_UINTPTR lower, upper;

            p = Px_PTR_ADD(h->base, (i * h->page_size));

            lower = Px_PAGESIZE_ALIGN_DOWN(p, h->page_size);
            upper = Px_PAGESIZE_ALIGN_UP(p, h->page_size);

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

#define _SIG_INVALID        (0UL)
#define _SIG_NULL           (1UL << 1)
#define _SIG_UNKNOWN        (1UL << 2)
#define _SIG_PY             (1UL << 3)
#define _SIG_PX             (1UL << 4)

Py_TLS int _Px_ObjectSignature_CallDepth = 0;
Py_TLS int _Px_SafeObjectSignatureTest_CallDepth = 0;

unsigned long
_Px_MemorySignature(void *m)
{
    PxState *px;
    unsigned long signature;

    if (!m)
        return _MEMSIG_NULL;

    px = PXSTATE();
    if (!px)
        return (_PyMem_InRange(m) ? _MEMSIG_PY : _MEMSIG_NOT_READY);

    signature = _MEMSIG_UNKNOWN;

#ifdef Py_DEBUG
    AcquireSRWLockShared(&px->pages_srwlock);
    if (PxPages_Find(px->pages, m))
        signature = _MEMSIG_PX;
    ReleaseSRWLockShared(&px->pages_srwlock);
#endif

    if (signature == _MEMSIG_UNKNOWN && _PyMem_InRange(m))
        signature = _MEMSIG_PY;

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

    if (!s) {
        signature = _OBJSIG_UNKNOWN;
        goto done;
    }

    is_py = (s == (Py_uintptr_t)_Py_NOT_PARALLEL);

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

    if (_PyParallel_Finalized)
        return (_PYTEST(flags) ? 1 : 0);

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
            return (Py_PXCTX() ? 1 : ((s & _OBJSIG_PX) == _OBJSIG_PX) ? 1 : 0);
        }

        return ((s & _OBJSIG_PX) == _OBJSIG_PX);

    } else if (flags & _PYOBJ_TEST) {

        if (!m)
            return 0;

        return ((s & _OBJSIG_PY) == _OBJSIG_PY);

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
    unsigned long s = 0;
    unsigned long o;

    assert(_MEMTEST(flags));

    if (_PyParallel_Finalized)
        return (_PYTEST(flags) ? 1 : 0);

    if (m) {
        if (_PyParallel_IsHeapOverrideActive()) {
            if (m == last_heap_override_malloc_addr)
                s = _MEMSIG_PX;
        }
        if (!s) {
            if (m == last_context_heap_malloc_addr)
                s = _MEMSIG_PX;
        }
        if (!s) {
            o = _Px_SafeObjectSignatureTest(m);
            s = _Px_MemorySignature(m);
            if (s & _MEMSIG_NOT_READY || (o > s))
                s = o;
        }
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
#ifdef Py_DEBUG
            if (!(s & _MEMSIG_PX)) {
                PxState *px = PXSTATE();
                AcquireSRWLockShared(&px->pages_srwlock);
                printf("\ncouldn't find ptr: 0x%llx\n", m);
                PxPages_Dump(px->pages);
                ReleaseSRWLockExclusive(&px->pages_srwlock);
            } else {
                //printf("found ptr 0x%llx\n", m);
            }
            assert(s & _MEMSIG_PX);
#endif
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

int
_Px_TEST(void *p)
{
    unsigned long o, m, s;
    if (!p)
        return _SIG_NULL;

    o = _Px_SafeObjectSignatureTest(p);
    m = _Px_MemorySignature(p);
    s = Px_MAX(o, m);
    return (s & _SIG_PX);
}

void
_PyParallel_ContextGuardFailure(const char *function,
                                const char *filename,
                                int lineno,
                                int was_px_ctx)
{
    int err;
    char buf[128], *fmt;
    SecureZeroMemory(buf, sizeof(buf));

    if (was_px_ctx)
        fmt = "%s called outside of parallel context (%s:%d)";
    else
        fmt = "%s called from within parallel context (%s:%d)";

    err = snprintf(buf, sizeof(buf), fmt, function, filename, lineno);
    if (err == -1)
        Py_FatalError("_PyParallel_ContextGuardFailure: snprintf failed");
    else {
        char *guard_override = getenv("PYPARALLEL_PY_GUARD");
        if (!guard_override)
            Py_FatalError(buf);

        if (!strcmp(guard_override, "warn")) {
            PyErr_Warn(PyExc_RuntimeWarning, buf);
        } else if (!strcmp(guard_override, "break")) {
            __debugbreak();
        }
    }
}
/*
#endif
*/
#define Px_SIZEOF_HEAP           Px_CACHE_ALIGN(sizeof(Heap))
#define Px_USEABLE_HEAP_SIZE(n) (Py_MAX(n, Px_PAGE_ALIGN_SIZE) - Px_SIZEOF_HEAP)
#define Px_NEW_HEAP_SIZE(n) \
    (Py_MAX(Px_PAGE_ALIGN(Px_USEABLE_HEAP_SIZE(n)+Px_SIZEOF_HEAP), \
            (n+Px_SIZEOF_HEAP)))

static __inline
size_t
_PyParallel_NewHeapSize(size_t needed)
{
    size_t new_size = 0;
    size_t tmp2 = 0;
    if (needed + Px_SIZEOF_HEAP <= Px_PAGE_ALIGN_SIZE)
        new_size = Px_PAGE_ALIGN(needed);
    else
        new_size = needed + Px_SIZEOF_HEAP;
    tmp2 = Px_NEW_HEAP_SIZE(needed);
    if (tmp2 != new_size)
        __debugbreak();
    return new_size;
}

void *
Heap_Init(Context *c, size_t n, int page_size)
{
    Heap  *h;
    Stats *s = &(c->stats);
    size_t size;
    int flags;

    assert(!Px_TLS_HEAP_ACTIVE);

    if (!page_size)
        page_size = Px_PAGE_SIZE;

    assert(
        page_size == Px_PAGE_SIZE ||
        page_size == Px_LARGE_PAGE_SIZE
    );


    if (n < Px_DEFAULT_HEAP_SIZE)
        size = Px_DEFAULT_HEAP_SIZE;
    else
        size = n;

    size = Px_PAGESIZE_ALIGN_UP(size, page_size);

    assert((size % page_size) == 0);

    if (!c->h) {
        /* First init. */
        h = &(c->heap);
        h->id = 1;
    } else {
        h = c->h->sle_next;
        h->sle_prev = c->h;
        h->id = h->sle_prev->id + 1;
    }

    assert(h);

    h->page_size = page_size;
    h->pages = size / page_size;

    h->size = size;
    flags = HEAP_ZERO_MEMORY;
    h->base = h->next = HeapAlloc(c->heap_handle, flags, h->size);
    if (!h->base)
        return PyErr_SetFromWindowsErr(0);
    h->next_alignment = Px_GET_ALIGNMENT(h->base);
    h->remaining = size;
    s->remaining = size;
    s->size += size;
    s->heaps++;
    c->h = h;
    h->ctx = c;
    h->sle_next = (Heap *)_PyHeap_Malloc(c, sizeof(Heap), 0, 0);
    assert(h->sle_next);
#ifdef Py_DEBUG
    _PxState_RegisterHeap(c->px, h, c);
#endif
    return h;
}

/* 0 = failure, 1 = success */
int
_PyTLSHeap_Init(size_t n, int page_size)
{
    TLS *t = &tls;
    Heap *h;
    Stats *s = &(t->stats);
    size_t size;
    int flags;

    if (!page_size)
        page_size = Px_PAGE_SIZE;

    assert(
        page_size == Px_PAGE_SIZE ||
        page_size == Px_LARGE_PAGE_SIZE
    );

    if (n < _PyTLSHeap_DefaultSize)
        size = _PyTLSHeap_DefaultSize;
    else
        size = n;

    size = Px_PAGESIZE_ALIGN_UP(size, page_size);

    assert((size % page_size) == 0);

    if (!t->h) {
        /* First init. */
        h = &(t->heap);
        h->id = 1;
    } else {
        h = t->h->sle_next;
        h->sle_prev = t->h;
        h->id = h->sle_prev->id + 1;
    }

    assert(h);

    h->page_size = page_size;
    h->pages = size / page_size;

    h->size = size;
    flags = HEAP_ZERO_MEMORY;
    h->base = h->next = HeapAlloc(t->handle, flags, h->size);
    if (!h->base)
        return (int)PyErr_SetFromWindowsErr(0);
    h->next_alignment = Px_GET_ALIGNMENT(h->base);
    h->remaining = size;
    s->remaining = size;
    s->size += size;
    s->heaps++;
    t->h = h;
    h->tls = t;
    h->sle_next = (Heap *)_PyTLSHeap_Malloc(sizeof(Heap), 0);
    assert(h->sle_next);
#ifdef Py_DEBUG
    _PxState_RegisterHeap(t->px, h, 0);
#endif
    return 1;
}

void *
_PyHeap_Init(Context *c, Py_ssize_t n)
{
    return Heap_Init(c, n, 0);
}

void *
_PyTLSHeap_Malloc(size_t n, size_t align)
{
    void  *next;
    Heap  *h;
    TLS   *t = &tls;
    Stats *s = &t->stats;
    size_t alignment_diff;
    size_t alignment = align;
    size_t requested_size = n;
    size_t aligned_size;

    assert(t->heap_depth > 0 || _PxNewThread);

    if (!alignment)
        alignment = Px_PTR_ALIGN_SIZE;
begin:
    h = t->h;

    if (alignment > h->next_alignment)
        alignment_diff = Px_PTR_ALIGN(alignment - h->next_alignment);
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

        next = h->next;
        h->next = Px_PTR_ADD(h->next, aligned_size);
        h->next_alignment = Px_GET_ALIGNMENT(h->next);

        assert(Px_PTR_ADD(h->base, h->allocated) == h->next);
        assert(_Py_IS_ALIGNED(h->base, alignment));
        assert(Px_GET_ALIGNMENT(next) >= alignment);
        return next;
    }

    if (!_PyTLSHeap_Init(Px_NEW_HEAP_SIZE(aligned_size), 0))
        return PyErr_NoMemory();

    goto begin;
}

void *
_PyHeapOverride_Malloc(size_t n, size_t align)
{
    void *p;
    HANDLE h;
    int flags = HEAP_ZERO_MEMORY;
    size_t aligned_size = Px_ALIGN(n, Px_MAX(align, Px_PTR_ALIGN_SIZE));
    assert(_PyParallel_IsHeapOverrideActive());

    h = _PyParallel_GetHeapOverride();

    p = HeapAlloc(h, HEAP_ZERO_MEMORY, aligned_size);
    if (!p)
        PyErr_SetFromWindowsErr(0);

    last_heap_override_malloc_addr = p;

    return p;
}

/* If no_realloc is 1: return PyErr_NoMemory() if the allocation can't be
 * satisfied by already allocated memory (i.e. don't call HeapAllocate()).
 */
void *
_PyHeap_Malloc(Context *c, size_t n, size_t align, int no_realloc)
{
    void  *next;
    Heap  *h;
    Stats *s;
    size_t alignment_diff;
    size_t alignment = align;
    size_t requested_size = n;
    size_t aligned_size;

    if (!c) {
        Context *tls_ctx = ctx;
        __debugbreak();
    }

    if (Px_TLS_HEAP_ACTIVE)
        return _PyTLSHeap_Malloc(n, align);

    if (_PyParallel_IsHeapOverrideActive())
        return _PyHeapOverride_Malloc(n, align);

    s = &c->stats;
    if (!alignment)
        alignment = Px_PTR_ALIGN_SIZE;

    if (!c->h) {
        /* I've seen this breakpoint hit in two circumstances so far (YMMV):
         *  1. A 3rd-party lib/DLL/C code is calling one of the Python malloc
         *     routines without holding the GIL.
         *  2. I didn't trace down the exact cause, but if you look at the
         *     stack trace, the frame before us was ffi_call_AMD64 from
         *     _cytpes.pyd.  Easy to reproduce: start a debug version of the
         *     interpreter, then `import IPython` -- it'll hit this
         *     immediately.  Presumably that's being caused by mixing and
         *     matching debug/release builds, I guess.
         * Either way, this isn't a recoverable error by the time it gets
         * here.
         */
        __debugbreak();
        ASSERT_UNREACHABLE();
    }

begin:
    h = c->h;

    if (alignment > h->next_alignment)
        alignment_diff = Px_PTR_ALIGN(alignment - h->next_alignment);
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

        next = h->next;
        h->next = Px_PTR_ADD(h->next, aligned_size);
        h->next_alignment = Px_GET_ALIGNMENT(h->next);

        assert(Px_PTR_ADD(h->base, h->allocated) == h->next);
        assert(_Py_IS_ALIGNED(h->base, alignment));
        assert(Px_GET_ALIGNMENT(next) >= alignment);
        last_context_heap_malloc_addr = next;
        return next;
    }

    /* If h->sle_next->size is non-zero, it means the next heap in the list
     * has already been allocated.  This happens when a context heap snapshot
     * is taken, a heap resize occurs (i.e. the code path below this logic is
     * hit), and then the context heap snapshot is rolled back, which involves
     * enumerating over the heaps and resetting them (instead of returning
     * them immediately back to the operating system via HeapFree()).
     */
    if (h->sle_next->size) {
        _PyHeap_EnsureReset(h->sle_next);
        c->h = h->sle_next;
        goto begin;
    }

    if (no_realloc)
        return PyErr_NoMemory();

    /* Force a resize. */
    if (!_PyHeap_Init(c, _PyParallel_NewHeapSize(aligned_size)))
        return PyErr_NoMemory();

    goto begin;
}


void
_PyHeap_FastFree(Heap *h, Stats *s, void *p)
{
    h->frees++;
    s->frees++;
}

void *
_PyTLSHeap_Realloc(void *p, size_t n)
{
    void *r = _PyTLSHeap_Malloc(n, 0);
    if (!r)
        return NULL;

    if (p)
        memcpy(r, p, n);

    return r;
}

void *
_PyHeap_Realloc(Context *c, void *p, size_t n)
{
    void  *r;
    Heap  *h;
    Stats *s;

    if (Px_TLS_HEAP_ACTIVE)
        return _PyTLSHeap_Realloc(p, n);

    h = c->h;
    s = &c->stats;
    r = _PyHeap_Malloc(c, n, 0, 0);
    if (!r)
        return NULL;
    if (!p)
        return r;
    h->mem_reallocs++;
    s->mem_reallocs++;
    memcpy(r, p, n);
    return r;
}

void
_PyHeap_Free(Context *c, void *p)
{
    Heap  *h;
    Stats *s;

    if (Px_TLS_HEAP_ACTIVE)
        return;

    Px_GUARD_MEM(p);

    h = c->h;
    s = &c->stats;

    h->frees++;
    s->frees++;
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

PyObject *
init_object(Context *c, PyObject *p, PyTypeObject *tp, Py_ssize_t nitems)
{
    /* Main use cases:
     *  1.  Redirect from PyObject_NEW/PyObject_NEW_VAR.  p will be NULL.
     *  2.  PyObject_MALLOC/PyObject_INIT combo (PyUnicode* does this).
     *      p will pass Px_GUARD_MEM(p) and everything will be null.  We
     *      need to allocate x & o manually.
     *  3.  Redirect from PyObject_GC_Resize.  p shouldn't be NULL, although
     *      it might not be a PX allocation -- we could be resizing a PY
     *      allocation.  That's fine, as our realloc doesn't actually free
     *      anything, so we're, in effect, just copying the existing object.
     */
    TLS      *t = &tls;
    PyObject *n;
    PxObject *x;
    Object   *o;
    Stats    *s;
    size_t    object_size;
    size_t    total_size;
    size_t    bytes_to_copy;
    int       init_type;
    int       is_varobj = -1;
    int       is_heap_override_active = _PyParallel_IsHeapOverrideActive();

#define _INIT_NEW       1
#define _INIT_INIT      2
#define _INIT_RESIZE    3

    s = (Px_TLS_HEAP_ACTIVE ? &t->stats : &c->stats);

    if (!p) {
        /* Case 1: PyObject_NEW/NEW_VAR (via (Object|VarObject)_New). */
        init_type = _INIT_NEW;
        assert(tp);

        object_size = _Px_VSZ(tp, nitems);
        total_size  = _Px_SZ(object_size);
        n = (PyObject *)_PyHeap_Malloc(c, total_size, 0, 0);
        if (!n)
            return PyErr_NoMemory();
        x = _Px_X_PTR(n, object_size);
        o = _Px_O_PTR(n, object_size);

        Py_TYPE(n) = tp;
        if (is_varobj = (tp->tp_itemsize > 0)) {
            s->varobjs++;
            Py_SIZE(n) = nitems;
        } else
            s->objects++;

    } else {
        if (tp) {
            /* Case 2: PyObject_INIT/INIT_VAR called against manually
             * allocated memory (i.e. not allocated via PyObject_NEW). */
            init_type = _INIT_INIT;
            assert(tp);
            Px_GUARD_MEM(p);

            /* Need to manually allocate x + o storage. */
            x = (PxObject *)_PyHeap_Malloc(c, _Px_SZ(0), 0, 0);
            if (!x)
                return PyErr_NoMemory();

            o = _Px_O_PTR(x, 0);
            n = p;

            Py_TYPE(n) = tp;

            if (is_varobj = (tp->tp_itemsize > 0))
                Py_SIZE(n) = nitems;

            if (!Px_ISMIMIC(n)) {
                if (is_varobj)
                    s->varobjs++;
                else
                    s->objects++;
            }

        } else {
            /* Case 3: PyObject_GC_Resize called.  Object to resize may or may
             * not be from a parallel context.  Doesn't matter either way as
             * we don't really realloc anything behind the scenes -- we just
             * malloc another, larger chunk from our heap and copy over the
             * previous data. */
            init_type = _INIT_RESIZE;
            assert(!tp);
            assert(Py_TYPE(p) != NULL);
            is_varobj = 1;

            object_size = _Px_VSZ(tp, nitems);
            total_size  = _Px_SZ(object_size);
            n = (PyObject *)_PyHeap_Malloc(c, total_size, 0, 0);
            if (!n)
                return PyErr_NoMemory();
            x = _Px_X_PTR(n, object_size);
            o = _Px_O_PTR(n, object_size);

            /* Just do a blanket copy of everything rather than trying to
             * isolate the underlying VarObject ob_items.  It doesn't matter
             * if we pick up old pointers and whatnot (i.e. old px/is_px refs)
             * as all that stuff is initialized in the next section. */
            bytes_to_copy = _PyObject_VAR_SIZE(tp, Py_SIZE(p));

            assert(bytes_to_copy < object_size);
            assert(Py_SIZE(p) < nitems);

            memcpy(n, p, bytes_to_copy);

            Py_TYPE(n) = tp;
            Py_SIZE(n) = nitems;

            if (Py_PXOBJ(p)) {
                /* XXX do we really need to do this?  (Original line of
                 * thinking was that we might need to treat the object
                 * differently down the track (i.e. during cleanup) if
                 * it was resized.) */
                Py_ASPX(p)->resized_to = n;
                x->resized_from = p;
            }

            c->h->resizes++;
            s->resizes++;
        }
    }
    Py_REFCNT(n) = 1;

    assert(tp);
    assert(Py_TYPE(n) == tp);
    assert(is_varobj == 0 || is_varobj == 1);

    Py_EVENT(n) = NULL;
    Py_PXFLAGS(n) |= Py_PXFLAGS_ISPX;
    Py_ORIG_TYPE(n) = NULL;

    if (is_varobj)
        assert(Py_SIZE(n) == nitems);

    n->px = x;
    n->is_px = _Py_IS_PARALLEL;
    n->srw_lock = NULL;

    x->ctx = c;
    x->signature = _PxObjectSignature;

    if (Px_ISMIMIC(n))
        goto end;

    if (Px_TLS_HEAP_ACTIVE)
        goto end;

    if (is_heap_override_active) {
        Py_PXFLAGS(n) |= Py_PXFLAGS_CLONED;
        goto end;
    }

    o->op = n;
    append_object((is_varobj ? &c->varobjs : &c->objects), o);

    if (!c->ob_first) {
        c->ob_first = n;
        c->ob_last  = n;
        n->_ob_next = NULL;
        n->_ob_prev = NULL;
    } else {
        PyObject *last;
        assert(!c->ob_first->_ob_prev);
        assert(!c->ob_last->_ob_next);
        last = c->ob_last;
        last->_ob_next = n;
        n->_ob_prev = last;
        n->_ob_next = NULL;
        c->ob_last = n;
    }

end:
    return n;
}


PyObject *
Object_Init(PyObject *op, PyTypeObject *tp, Context *c)
{
    assert(tp->tp_itemsize == 0);
    return init_object(c, op, tp, 0);
}


PyObject *
Object_New(PyTypeObject *tp, Context *c)
{
    return init_object(c, NULL, tp, 0);
}


PyVarObject *
VarObject_Init(PyVarObject *v, PyTypeObject *tp, Py_ssize_t nitems, Context *c)
{
    assert(tp->tp_itemsize > 0);
    return (PyVarObject *)init_object(c, (PyObject *)v, tp, nitems);
}


PyVarObject *
VarObject_New(PyTypeObject *tp, Py_ssize_t nitems, Context *c)
{
    return (PyVarObject *)init_object(c, NULL, tp, nitems);
}

PyVarObject *
VarObject_Resize(PyObject *v, Py_ssize_t nitems, Context *c)
{
    return (PyVarObject *)init_object(c, v, NULL, nitems);
}

#ifdef Py_DEBUG
void *
_PxObject_Realloc(void *p, size_t nbytes)
{
    unsigned long o, m, s;
    Context *c = ctx;
    o = _Px_SafeObjectSignatureTest(p);
    m = _Px_MemorySignature(p);
    s = Px_MAX(o, m);
    if (Py_PXCTX()) {
        void *r;
        if (s & _SIG_PY)
            printf("\n_PxObject_Realloc(Py_PXCTX() && p = _SIG_PY)\n");
        r = _PyHeap_Realloc(c, p, nbytes);
        return r;
    } else {
        if (s & _SIG_PX)
            printf("\n_PxObject_Realloc(!Py_PXCTX() && p = _SIG_PX)\n");
        return PyObject_Realloc(p, nbytes);
    }
    assert(0);
}

void
_PxObject_Free(void *p)
{
    unsigned long o, m, s;
    Context *c = ctx;
    if (!p)
        return;
    o = _Px_SafeObjectSignatureTest(p);
    m = _Px_MemorySignature(p);
    s = Px_MAX(o, m);
    if (Py_PXCTX()) {
        if (!(s & _SIG_PX))
            printf("\n_PxObject_Free(Py_PXCTX() && p != _SIG_PX)\n");
        else
            _PyHeap_Free(c, p);
    } else {
        if (s & _SIG_PX)
            printf("\n_PxObject_Free(!Py_PXCTX() && p = _SIG_PX)\n");
        else
            PyObject_Free(p);
    }
}

#else
void *
_PxObject_Realloc(void *p, size_t nbytes)
{
    Px_GUARD();
    return _PyHeap_Realloc(ctx, p, nbytes);
}

void
_PxObject_Free(void *p)
{
    Px_GUARD();
    if (!p)
        return;
    _PyHeap_Free(ctx, p);
}
#endif


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



void
Px_INCCTX(Context *c)
{
    InterlockedIncrement(&(c->refcnt));
}

void
_PxState_ReleaseContext(PxState *px, Context *c)
{
    Context *last;
    assert(c->refcnt == 0);
    if (c->persisted_count > 0) {

        assert(Px_CTX_IS_PERSISTED(c));

        InterlockedIncrement(&(px->contexts_persisted));
        InterlockedDecrement(&(px->active));
        InterlockedDecrement(&(px->contexts_active));

        Px_CTXFLAGS(c) &= ~Px_CTXFLAGS_IS_PERSISTED;
        Px_CTXFLAGS(c) |=  Px_CTXFLAGS_WAS_PERSISTED;

        return;
    }

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


long
Px_DECCTX(Context *c)
{
    PxState *px = c->px;
    InterlockedDecrement(&(c->refcnt));
    assert(c->refcnt >= 0);

    if (c->refcnt > 0)
        return c->refcnt;

    assert(c->refcnt == 0);
    _PxState_ReleaseContext(px, c);
    return 0;
}

int
_PxState_AllocIOBufs(PxState *px, Context *c, int count, int size)
{
    size_t   nbufs;
    size_t   bufsize;
    size_t   heapsize;
    size_t   all_io;
    size_t   all_bufs;
    size_t   iosize;
    void    *io_first;
    void    *buf_first;
    int      i;
    int      result = 0;
    PxIO    *io;
    char    *buf;

    assert(px);

    nbufs = count;
    bufsize = size;
    iosize = Px_MEM_ALIGN(sizeof(PxIO));

    all_io = nbufs * iosize;
    all_bufs = nbufs * bufsize;

    heapsize = all_io + all_bufs;

    c->heap_handle = HeapCreate(HEAP_NO_SERIALIZE, heapsize, 0);
    if (!c->heap_handle) {
        PyErr_SetFromWindowsErr(0);
        goto done;
    }

    if (!_PyHeap_Init(c, heapsize))
        goto free_heap;

    io_first = _PyHeap_Malloc(c, all_io, Px_MEM_ALIGN_SIZE, 1);
    if (!io_first)
        goto free_heap;

    do {
        buf_first = _PyHeap_Malloc(c, all_bufs, Px_MEM_ALIGN_SIZE, 1);
        if (buf_first)
            break;

        all_bufs -= bufsize;
        nbufs--;

    } while (all_bufs > 0);

    for (i = 0; i < nbufs; i++) {
        io =  (PxIO *)Px_PTR_ADD(io_first,  (i * iosize));
        buf = (char *)Px_PTR_ADD(buf_first, (i * bufsize));

        assert(Px_PTR(io) ==  Px_ALIGN(io,  Px_MEM_ALIGN_SIZE));
        assert(Px_PTR(buf) == Px_ALIGN(buf, Px_MEM_ALIGN_SIZE));

        io->size = (int)bufsize;
        io->buf  = buf;

        PxList_Push(px->io_free, E2I(io));
    }

    assert(PxList_QueryDepth(px->io_free) == nbufs);

    result = 1;
    goto done;

free_heap:
    HeapDestroy(c->heap_handle);

done:
    if (!result)
        assert(PyErr_Occurred());

    return result;
}

void *
_PyParallel_CreatedNewThreadState(PyThreadState *tstate)
{
    PxState *px;

    TSTATE = tstate;

    _PyParallel_RefreshMemoryLoad();

    px = (PxState *)malloc(sizeof(PxState));
    if (!px)
        return PyErr_NoMemory();

    SecureZeroMemory(px, sizeof(PxState));

    px->errors = PxList_New();
    if (!px->errors)
        goto free_px;

    px->completed_callbacks = PxList_New();
    if (!px->completed_callbacks)
        goto free_errors;

    px->completed_errbacks = PxList_New();
    if (!px->completed_errbacks)
        goto free_completed_callbacks;

    px->new_threadpool_work = PxList_New();
    if (!px->new_threadpool_work)
        goto free_completed_errbacks;

    px->incoming = PxList_New();
    if (!px->incoming)
        goto free_new_threadpool_work;

    px->finished = PxList_New();
    if (!px->finished)
        goto free_incoming;

    px->finished_sockets = PxList_New();
    if (!px->finished_sockets)
        goto free_finished;

    px->io_free = PxList_New();
    if (!px->io_free)
        goto free_finished_sockets;

    px->work_ready = PxList_New();
    if (!px->work_ready)
        goto free_io_free;

    px->io_free_wakeup = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!px->io_free_wakeup)
        goto free_work_ready;

    px->wakeup = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!px->wakeup)
        goto free_io_wakeup;

    goto skip_threadpool_stuff_for_now;

    px->ptp = CreateThreadpool(NULL);
    if (!px->ptp)
        goto free_wakeup;

    px->ptp_cbe = &px->tp_cbe;
    InitializeThreadpoolEnvironment(px->ptp_cbe);

    px->ptp_cg = CreateThreadpoolCleanupGroup();
    if (!px->ptp_cg)
        goto free_threadpool;

skip_threadpool_stuff_for_now:
    _PxState_InitPxPages(px);

    InitializeCriticalSectionAndSpinCount(&(px->cs), 12);

    tstate->px = px;
    px->tstate = tstate;

    tstate->is_parallel_thread = 0;
    px->ctx_ttl = 1;

    InitializeCriticalSectionAndSpinCount(&px->contexts_cs, 24);
    InitializeListHead(&px->contexts);

    goto done;

//free_cleanup_group:
    CloseThreadpoolCleanupGroup(px->ptp_cg);

free_threadpool:
    CloseThreadpool(px->ptp);

free_wakeup:
    CloseHandle(px->wakeup);

free_io_wakeup:
    CloseHandle(px->io_free_wakeup);

free_work_ready:
    PxList_FreeListHead(px->work_ready);

free_io_free:
    PxList_FreeListHead(px->io_free);

free_finished_sockets:
    PxList_FreeListHead(px->finished_sockets);

free_finished:
    PxList_FreeListHead(px->finished);

free_incoming:
    PxList_FreeListHead(px->incoming);

free_new_threadpool_work:
    PxList_FreeListHead(px->new_threadpool_work);

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

void
_PyParallel_ClearingThreadState(PyThreadState *tstate)
{

}

PyObject* _async_run(PyObject *, PyObject *);

void
_PyParallel_DeletingThreadState(PyThreadState *tstate)
{
    PxState *px = (PxState *)tstate->px;

    assert(px);

    if (px->contexts_active > 0) {
        printf("_PyParallel_DeletingThreadState(): px->contexts_active: %d\n",
               px->contexts_active);
    }
}

void
_PyParallel_DeletingInterpreterState(PyInterpreterState *interp)
{

}

void
_PyParallel_InitializedThreadState(PyThreadState *pstate)
{
    //if (Py_MainThreadId != _Py_get_current_thread_id())
    //    PyEval_RestoreThread(pstate);
}


int
_PyParallel_ExecutingCallbackFromMainThread(void)
{
    PyThreadState *tstate;
    PxState       *px;
    Py_GUARD();

    tstate = PyThreadState_GET();

    assert(!tstate->is_parallel_thread);
    px = (PxState *)tstate->px;
    return (px->processing_callback == 1);
}

PyObject *
PxState_SetError(Context *c)
{
    PxState *px = c->px;
    PyThreadState *pstate = c->pstate;
    assert(pstate->curexc_type != NULL);
    PxList_TimestampItem(c->error);
    c->error->from = c;
    c->error->p1 = pstate->curexc_type;
    c->error->p2 = pstate->curexc_value;
    c->error->p3 = pstate->curexc_traceback;
    InterlockedExchange(&(c->done), 1);
    /*
    InterlockedIncrement64(done);
    InterlockedDecrement(inflight);
    */
    PxList_Push(px->errors, c->error);
    SetEvent(px->wakeup);
    return NULL;
}

void
PxContext_HandleError(Context *c)
{
    PyObject *args, *r;
    PxState *px = c->px;
    PyThreadState *pstate = c->pstate;

    assert(pstate->curexc_type != NULL);

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
            /*
            InterlockedIncrement64(done);
            InterlockedDecrement(inflight);
            */
            PxList_Push(px->completed_errbacks, c->errback_completed);
            SetEvent(px->wakeup);
            return;
        }
    }

error:
    PxState_SetError(c);
}

int
_PyParallel_InitTLS(void)
{
    TLS  *t = &tls;
    assert(_PxNewThread != 0);
    assert(!t->h);

    t->handle = HeapCreate(HEAP_NO_SERIALIZE, _PyTLSHeap_DefaultSize, 0);
    if (!t->handle)
        Py_FatalError("_PyParallel_InitTLSHeap:HeapCreate");

    TSTATE = ctx->tstate;
    t->px = (PxState *)TSTATE->px;
    assert(t->px);

    if (!_PyTLSHeap_Init(0, 0))
        return 0;

    t->thread_id = _Py_get_current_thread_id();
    t->thread_seq_id = InterlockedIncrement(&_PyParallel_NextThreadSeqId);
    _PyParallel_ThreadSeqId = t->thread_seq_id;

    if (!_PyParallel_RegisteredIOAvailable)
        return 1;

    //InitializeCriticalSectionAndSpinCount(&t->riobuf_bitmap_cs, 24);

    return 1;
}

PyDoc_STRVAR(_async_thread_seq_id_doc,
             "if in a parallel thread, return the sequence id of this "
             "thread, otherwise, return None");

PyObject *
_async_thread_seq_id(PyObject *self, PyObject *o)
{
    if (!Py_PXCTX())
        Py_RETURN_NONE;

    if (!_PyParallel_ThreadSeqIdObj) {
        _PyParallel_EnableTLSHeap();
        _PyParallel_ThreadSeqIdObj = PyLong_FromLong(_PyParallel_ThreadSeqId);
        _PyParallel_DisableTLSHeap();
    }

    return _PyParallel_ThreadSeqIdObj;
}

void
_PyParallel_EnteredCallback(Context *c, PTP_CALLBACK_INSTANCE instance)
{
    Stats *s;
    ctx = c;

    if (_PxNewThread) {
        if (!_PyParallel_InitTLS())
            Py_FatalError("TLS heap initialization failed");
        _PxNewThread = 0;
    }

    assert(
        c->error                &&
        c->pstate               &&
        c->decrefs              &&
        c->outgoing             &&
        c->errback_completed    &&
        c->callback_completed
    );

    s = &(c->stats);
    s->entered = _Py_rdtsc();
    assert(c->tstate);
    assert(c->heap_handle);

    if (instance)
        c->instance = instance;

    c->pstate->thread_id = _Py_get_current_thread_id();
}

void
_PyParallel_EnteredIOCallback(
    Context *c,
    PTP_CALLBACK_INSTANCE instance,
    void *overlapped,
    ULONG io_result,
    ULONG_PTR nbytes,
    TP_IO *tp_io
)
{
    _PyParallel_EnteredCallback(c, instance);

    c->io_result = io_result;
    c->io_nbytes = nbytes;
}

void
_PyParallel_ExitingCallback(Context *c)
{
    c->stats.exited = _Py_rdtsc();
}

void
_PyParallel_ExitingIOCallback(Context *c)
{
    _PyParallel_ExitingCallback(c);
}

void
NTAPI
_PyParallel_WorkCallback(PTP_CALLBACK_INSTANCE instance, void *context)
{
    Context  *c = (Context *)context;
    Stats    *s;
    PxState  *px;
    PyObject *r;
    PyObject *func = NULL,
             *args = NULL,
             *kwds = NULL,
             *callback = NULL,
             *callback_args = NULL,
             *callback_kwds = NULL,
             *errback = NULL;

    PyThreadState *pstate;

    volatile long       *pending;
    volatile long       *inflight;
    volatile long long  *done;

    _PyParallel_EnteredCallback(c, instance);

    s = &(c->stats);
    px = (PxState *)c->tstate->px;

    if (c->tp_wait) {
        pending = &(px->waits_pending);
        inflight = &(px->waits_inflight);
        done = &(px->waits_done);
    } else if (c->tp_io) {
        pending = &(px->io_pending);
        inflight = &(px->io_inflight);
        done = &(px->io_done);
    } else if (c->tp_timer) {
        pending = &(px->timers_pending);
        inflight = &(px->timers_inflight);
        done = &(px->timers_done);
    } else {
        pending = &(px->pending);
        inflight = &(px->inflight);
        done = &(px->done);
    }

    InterlockedDecrement(pending);
    InterlockedIncrement(inflight);

    pstate = c->pstate;

    func = c->func;
    args = c->args;
    kwds = c->kwds;

    callback = c->callback;
    errback = c->errback;

    if (c->tp_wait) {
        assert(
            c->wait_result == WAIT_OBJECT_0 ||
            c->wait_result == WAIT_TIMEOUT  ||
            c->wait_result == WAIT_ABANDONED_0
        );
        if (c->wait_result == WAIT_OBJECT_0)
            goto start;
        else if (c->wait_result == WAIT_TIMEOUT) {
            PyErr_SetNone(PyExc_WaitTimeoutError);
            goto errback;
        } else {
            PyErr_SetFromWindowsErr(0);
            goto errback;
        }
    } /* else if (c->tp_io && c->io_type == Px_IOTYPE_FILE) {
        PxListItem *item;
        PyObject *obj;
        PxIO *io;
        ULONG_PTR nbytes;
        assert(
            (c->io_type & (PyAsync_IO_WRITE)) &&
            c->overlapped != NULL &&
            c->io != NULL
        );
        io = c->io;
        obj = io->obj;
        nbytes = c->io_nbytes;
        if (PxIO_IS_ONDEMAND(io))
            PxList_Push(px->io_ondemand, E2I(io));
        else {
            assert(PxIO_IS_PREALLOC(io));
            memset(&(io->overlapped), 0, sizeof(OVERLAPPED));
            io->obj = NULL;
            PxList_Push(px->io_free, E2I(io));
            SetEvent(px->io_free_wakeup);
        }

        item = _PyHeap_NewListItem(c);
        if (!item) {
            PyErr_NoMemory();
            goto error;
        }
        item->p1 = obj;
        PxList_Push(c->decrefs, item);

        if (c->io_result == NO_ERROR) {
            if (!c->callback)
                goto after_callback;
            args = PyTuple_Pack(2, obj, nbytes);
            goto start_callback;
        } else {
            PyErr_SetFromWindowsErr(c->io_result);
            goto errback;
        }
        assert(0);
    } else if (c->tp_io && c->io_type == Px_IOTYPE_SOCKET) {
        PxSocket *s = (PxSocket *)c->io_obj;
        switch (s->io_op) {
            case PxSocket_IO_CONNECT:
                READ_LOCK(s);
                func = s->connection_made;
                READ_UNLOCK(s);
                args = Py_BuildValue("(O)", s);
                break;
            case PxSocket_IO_READ:
                func = s->data_received;
                if (!func)
                    break;
        }
        READ_UNLOCK(s);

    } */ else if (c->tp_timer) {
        assert(0);
    }

start:
    s->start = _Py_rdtsc();
    c->result = PyObject_Call(c->func, c->args, c->kwds);
    s->end = _Py_rdtsc();

    if (c->result) {
        assert(!pstate->curexc_type);
        if (c->callback) {
            if (!args)
                args = Py_BuildValue("(O)", c->result);
            r = PyObject_CallObject(c->callback, args);
            if (null_with_exc_or_non_none_return_type(r, pstate))
                goto errback;
        }
        c->callback_completed->from = c;
        PxList_TimestampItem(c->callback_completed);
        InterlockedExchange(&(c->done), 1);
        InterlockedIncrement64(done);
        InterlockedDecrement(inflight);
        PxList_Push(px->completed_callbacks, c->callback_completed);
        SetEvent(px->wakeup);
        goto end;
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
            InterlockedIncrement64(done);
            InterlockedDecrement(inflight);
            PxList_Push(px->completed_errbacks, c->errback_completed);
            SetEvent(px->wakeup);
            goto end;
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
    InterlockedIncrement64(done);
    InterlockedDecrement(inflight);
    PxList_Push(px->errors, c->error);
    SetEvent(px->wakeup);
end:
    _PyParallel_ExitingCallback(c);
}

void
NTAPI
_PyParallel_WaitCallback(
    PTP_CALLBACK_INSTANCE instance,
    void *context,
    PTP_WAIT wait,
    TP_WAIT_RESULT wait_result)
{
    Context  *c = (Context *)context;

    assert(wait == c->tp_wait);
    c->wait_result = wait_result;

    _PyParallel_WorkCallback(instance, c);
}

void
NTAPI
_PyParallel_IOCallback(
    PTP_CALLBACK_INSTANCE instance,
    void *context,
    void *overlapped,
    ULONG io_result,
    ULONG_PTR nbytes,
    TP_IO *tp_io
)
{
    Context *c = (Context *)context;
    assert(tp_io == c->tp_io);
    c->io_result = io_result;
    c->io_nbytes = nbytes;
    assert(overlapped == &(c->overlapped));
    /* c->io = OL2PxIO(c->overlapped);*/
    _PyParallel_WorkCallback(instance, c);
}

PyDoc_STRVAR(_async_cpu_count_doc,
"cpu_count() -> integer\n\n\
Return an integer representing the number of online logical CPUs,\n\
or -1 if this value cannot be established.");

#if defined(__DragonFly__) || \
    defined(__OpenBSD__)   || \
    defined(__FreeBSD__)   || \
    defined(__NetBSD__)    || \
    defined(__APPLE__)
int
_bsd_cpu_count(void)
{
    int err = -1;
    int ncpu = -1;
    int mib[4];
    size_t len = sizeof(int);
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    err = sysctl(mib, 2, &ncpu, &len, NULL, 0);
    if (!err)
        return ncpu;
    else
        return -1;
}
#endif

int
_cpu_count(void)
{
#ifdef MS_WINDOWS
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#elif __hpux
    return mpctl(MPC_GETNUMSPUS, NULL, NULL);
#ifndef _SC_NPROCESSORS_ONLN
#ifdef _SC_NPROC_ONLN /* IRIX */
#define _SC_NPROCESSORS_ONLN _SC_NPROC_ONLN
#endif
#endif /* ! defined(_SC_NPROCESSORS_ONLN) */
#elif defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
    return sysconf(_SC_NPROCESSORS_ONLN);
#elif __APPLE__
    int err = -1;
    int ncpu = -1;
    size_t len = sizeof(int);
    err = sysctlnametomib("hw.logicalcpu", &ncpu, &len, NULL, 0);
    if (!err)
        return ncpu;
    else
        return _bsd_cpu_count();
#elif defined(__DragonFly__) || \
      defined(__OpenBSD__)   || \
      defined(__FreeBSD__)   || \
      defined(__NetBSD__)
    return _bsd_cpu_count();
#else
    return -1;
#endif
}

PyObject *
_async_cpu_count(PyObject *self)
{
    return PyLong_FromLong(_cpu_count());
}

PyDoc_STRVAR(_async_active_io_loops_doc, "xxx todo\n");

PyObject *
_async_active_io_loops(PyObject *self)
{
    return PyLong_FromLong(_PxSocket_ActiveIOLoops);
}

PyDoc_STRVAR(_async_active_hogs_doc, "xxx todo\n");

PyObject *
_async_active_hogs(PyObject *self)
{
    return PyLong_FromLong(_PxSocket_ActiveHogs);
}

PyDoc_STRVAR(_async_seh_eav_in_io_callback_doc, "xxx todo\n");

PyObject *
_async_seh_eav_in_io_callback(PyObject *self)
{
    return PyLong_FromLong(_PyParallel_SEH_EAV_InIoCallback);
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

    _PxObjectSignature = (Px_PTR(_Py_rdtsc()) ^ Px_PTR(&_PxObjectSignature));
    _PxSocketSignature = (Px_PTR(_Py_rdtsc()) ^ Px_PTR(&_PxSocketSignature));
    _PxSocketBufSignature = (
        Px_PTR(_Py_rdtsc()) ^
        Px_PTR(&_PxSocketBufSignature)
    );

    Py_ParallelContextsEnabled = 0;
    _Py_lfence();
    _Py_clflush(&Py_MainThreadId);

    _PyParallel_NumCPUs = _cpu_count();
    if (!_PyParallel_NumCPUs)
        Py_FatalError("_PyParallel_Init: GetActiveProcessorCount() failed");

}

PyObject *
_async_run_once(PyObject *self, PyObject *args);

void
_PyParallel_Finalize(void)
{
    PxState *px = PXSTATE();

    assert(px);

    _PyParallel_Finalized = 1;

    if (px->contexts_active == 0)
        return;

    if (px->contexts_active < 0)
        __debugbreak();

    if (px->contexts_active) {
        int i = 0;
        long active_contexts = 0;
        long persisted_contexts = 0;

        PySys_FormatStdout("_PyParallel: waiting for %d contexts...\n",
                           px->contexts_active);

        do {
            i++;
            active_contexts = px->contexts_active;
            persisted_contexts = px->contexts_persisted;
            /*
            PySys_FormatStdout("_async.run(%d) [%d/%d] "
                               "(hogs: %d, ioloops: %d)\n",
                               i, active_contexts, persisted_contexts,
                               _PxSocket_ActiveHogs, _PxSocket_ActiveIOLoops);
            */
            if (active_contexts < 0)
                __debugbreak();
            assert(active_contexts >= 0);
            if (active_contexts == 0)
                break;
            if (!_async_run_once(NULL, NULL))
                break;
        } while (1);
    }

    if (px->contexts_active > 0) {
        printf("_PyParallel_Finalize(): px->contexts_active: %d\n",
               px->contexts_active);
    }
}

void
_PyParallel_ClearMainThreadId(void)
{
    _Py_sfence();
    Py_MainThreadId = 0;
    _Py_lfence();
    _Py_clflush(&Py_MainThreadId);
    //TSTATE = NULL;
}

void
_PyParallel_CreatedGIL(void)
{
    //_PyParallel_ClearMainThreadId();
}

void
_PyParallel_AboutToDropGIL(void)
{
    //_PyParallel_ClearMainThreadId();
}

void
_PyParallel_DestroyedGIL(void)
{
    //_PyParallel_ClearMainThreadId();
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
        /*Py_FatalError(buf);*/
    }

    if (Py_MainProcessId == -1)
        Py_FatalError("_PyParallel_JustAcquiredGIL: Py_MainProcessId == -1");

    _Py_sfence();
    Py_MainThreadId = _Py_get_current_thread_id();
    _Py_lfence();
    _Py_clflush(&Py_MainThreadId);
    //TSTATE = \
    //    (PyThreadState *)_Py_atomic_load_relaxed(&_PyThreadState_Current);
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

#define _METHOD(m, n, a) {#n, (PyCFunction)##m##_##n##, a, m##_##n##_doc }
#define _PARALLEL(n, a) _METHOD(_parallel, n, a)
#define _PARALLEL_N(n) _PARALLEL(n, METH_NOARGS)
#define _PARALLEL_O(n) _PARALLEL(n, METH_O)
#define _PARALLEL_V(n) _PARALLEL(n, METH_VARARGS)
static PyMethodDef _parallel_methods[] = {
    _PARALLEL_V(map),

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

/* xlist */
void
xlist_dealloc(PyXListObject *xlist)
{
    HeapDestroy(xlist->heap_handle);
    free(xlist);
}

PyObject *
PyXList_New(void)
{
    PyXListObject *xlist;

    if (Py_PXCTX()) {
        PyErr_SetString(PyExc_RuntimeError,
                        "xlist objects cannot be "
                        "created from parallel threads");
        return NULL;
    }

    xlist = (PyXListObject *)malloc(sizeof(PyXListObject));
    if (!xlist)
        return PyErr_NoMemory();

    memset(xlist, 0, sizeof(PyXListObject));

    xlist->heap_handle = HeapCreate(0, 0, 0);
    if (!xlist->heap_handle) {
        PyErr_SetFromWindowsErr(0);
        free(xlist);
        return NULL;
    }

    xlist->head = PxList_NewFromHeap(xlist->heap_handle);
    if (!xlist->head) {
        free(xlist);
        return NULL;
    }

    /* Manually initialize the type. */
    xlist->ob_base.ob_type = &PyXList_Type;
    xlist->ob_base.ob_refcnt = 1;

    InitializeCriticalSectionAndSpinCount(&(xlist->cs), 4);

    return (PyObject *)xlist;
}

PyObject *
xlist_new(PyTypeObject *tp, PyObject *args, PyObject *kwds)
{
    assert(tp == &PyXList_Type);
    return PyXList_New();
}

PyObject *
xlist_alloc(PyTypeObject *tp, Py_ssize_t nitems)
{
    assert(nitems == 0);
    assert(tp == &PyXList_Type);

    return PyXList_New();
}

PyObject *
xlist_pop(PyObject *self, PyObject *args)
{
    PyXListObject *xlist = (PyXListObject *)self;
    PxListItem *item;
    PyObject *obj = NULL;
    assert(args == NULL);
    Py_GUARD();
    /*Py_INCREF(xlist);*/
    item = PxList_Pop(xlist->head);
    //obj = (item ? I2O(item) : NULL);
    if (obj)
        Py_REFCNT(obj) = 1;
    return obj;
}

PyObject *
PyXList_Pop(PyObject *xlist)
{
    return xlist_pop(xlist, NULL);
}

PyObject *
PyObject_Clone(PyObject *src, const char *errmsg)
{
    int valid_type;
    PyObject *result = NULL;
    PyTypeObject *tp;

    tp = Py_TYPE(src);

    valid_type = (
        PyBytes_CheckExact(src)         ||
        PyByteArray_CheckExact(src)     ||
        PyUnicode_CheckExact(src)       ||
        PyLong_CheckExact(src)          ||
        PyFloat_CheckExact(src)         ||
        PyMethod_Check(src)
    );

    if (!valid_type) {
        PyErr_Format(PyExc_ValueError, errmsg, tp->tp_name);
        return NULL;
    }

    assert(_PyParallel_IsHeapOverrideActive());

    if (PyLong_CheckExact(src)) {
        result = _PyLong_Copy((PyLongObject *)src);

    } else if (PyFloat_CheckExact(src)) {
        result = _PxObject_Init(NULL, &PyFloat_Type);
        if (!result)
            return NULL;
        PyFloat_AS_DOUBLE(result) = PyFloat_AS_DOUBLE(src);

    } else if (PyUnicode_CheckExact(src)) {
        result = _PyUnicode_Copy(src);

    } else if (PyBytes_CheckExact(src)) {
        char *c = NULL;
        Py_ssize_t len;
        if (PyBytes_AsStringAndSize(src, &c, &len) != -1) {
            result = (PyObject *)PyBytes_FromStringAndSize(c, len);
            //(char *)b->ob_sval[0],
            //((PyVarObject *)b)->ob_size
        }
    } else if (PyByteArray_CheckExact(src)) {
        result = (PyObject *)PyByteArray_FromObject(src);
    } else if (PyMethod_Check(src)) {
        result = (PyObject *)PyMethod_Clone(src);
    } else {
        XXX_IMPLEMENT_ME();
    }

    if (result) {
        if (!Px_CLONED(result))
            __debugbreak();
    }

    return result;
}

PyObject *
xlist_push(PyObject *obj, PyObject *src)
{
    __debugbreak();
    return NULL;
}

/*
PyObject *
xlist_push(PyObject *obj, PyObject *src)
{
    PyXListObject *xlist = (PyXListObject *)obj;
    assert(src);

    Py_INCREF(xlist);
    Py_INCREF(src);

    if (!Py_PXCTX())
        PxList_PushObject(xlist->head, src);
    else {
        PyObject *dst;
        _PyParallel_SetHeapOverride(xlist->heap_handle);
        dst = PyObject_Clone(src, "objects of type %s cannot "
                                  "be pushed to xlists");
        _PyParallel_RemoveHeapOverride();
        if (!dst)
            return NULL;

        PxList_PushObject(xlist->head, dst);
    }

    //if (Px_CV_WAITERS(xlist))
    //    ConditionVariableWakeOne(&(xlist->cv));

    Py_RETURN_NONE;
}
*/

PyObject *
xlist_flush(PyObject *self, PyObject *arg)
{
    Py_RETURN_NONE;
}

Py_ssize_t
PyXList_Length(PyObject *self)
{
    PyXListObject *xlist = (PyXListObject *)self;
    Py_INCREF(xlist);
    return PxList_QueryDepth(xlist->head);
}

PyDoc_STRVAR(xlist_pop_doc,   "XXX TODO\n");
PyDoc_STRVAR(xlist_push_doc,  "XXX TODO\n");
PyDoc_STRVAR(xlist_size_doc,  "XXX TODO\n");
PyDoc_STRVAR(xlist_flush_doc, "XXX TODO\n");
#define _XLIST(n, a) _METHOD(xlist, n, a)
#define _XLIST_N(n) _XLIST(n, METH_NOARGS)
#define _XLIST_O(n) _XLIST(n, METH_O)
#define _XLIST_V(n) _XLIST(n, METH_VARARGS)
#define _XLIST_K(n) _XLIST(n, METH_VARARGS | METH_KEYWORDS)
static PyMethodDef xlist_methods[] = {
    _XLIST_N(pop),
    _XLIST_O(push),
    _XLIST_N(flush),
    { NULL, NULL }
};

static PySequenceMethods xlist_as_sequence = {
    (lenfunc)PyXList_Length,                    /* sq_length */
    0,                                          /* sq_concat */
    0,                                          /* sq_repeat */
    0,                                          /* sq_item */
    0,                                          /* sq_slice */
    0,                                          /* sq_ass_item */
    0,                                          /* sq_ass_slice */
    0,                                          /* sq_contains */
    0,                                          /* sq_inplace_concat */
    0,                                          /* sq_inplace_repeat */
};


static PyTypeObject PyXList_Type = {
    PyObject_HEAD_INIT(0)
    "xlist",
    sizeof(PyXListObject),
    0,
    (destructor)xlist_dealloc,                  /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_reserved */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    &xlist_as_sequence,                         /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                         /* tp_flags */
    "Interlocked List Object",                  /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    xlist_methods,                              /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    0,/*(initproc)xlist_init,*/                 /* tp_init */
    xlist_alloc,                                /* tp_alloc */
    xlist_new,                                  /* tp_new */
    0,                                          /* tp_free */
};


/* mod _async */

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

PyObject *
_async_persisted_contexts(PyObject *self, PyObject *args)
{
    return PyLong_FromLong(PXSTATE()->contexts_persisted);
}


void
incref_args(Context *c)
{
    Py_INCREF(c->func);
    Py_XINCREF(c->args);
    Py_XINCREF(c->kwds);
    Py_XINCREF(c->callback);
    Py_XINCREF(c->errback);
}


void
incref_waitobj_args(Context *c)
{
    Py_INCREF(c->waitobj);
    Py_INCREF(c->waitobj_timeout);
    incref_args(c);
}



void
decref_args(Context *c)
{
    Py_XDECREF(c->func);
    Py_XDECREF(c->args);
    Py_XDECREF(c->kwds);
    Py_XDECREF(c->callback);
    Py_XDECREF(c->errback);
}


void
decref_waitobj_args(Context *c)
{
    Py_DECREF(c->waitobj);
    Py_DECREF(c->waitobj_timeout);
}

Context *
_PxState_FreeContext(PxState *px, Context *c)
{
    Heap *h;
    Stats *s;
    Object *o;
    PxListItem *item;
    Context *prev, *next;

    assert(c->px == px);

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

    /* xxx todo: check refcnts of func/args/kwds etc? */
    decref_args(c);

    if (c->tp_wait)
        decref_waitobj_args(c);

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

    item = PxList_Flush(c->decrefs);
    while (item) {
        PxListItem *next = PxList_Next(item);
        Py_XDECREF((PyObject *)item->p1);
        Py_XDECREF((PyObject *)item->p2);
        Py_XDECREF((PyObject *)item->p3);
        Py_XDECREF((PyObject *)item->p4);
        _PyHeap_Free(c, item);
        item = next;
    }

    for (o = c->events.first; o; o = o->next) {
        assert(Py_HAS_EVENT(o));
        assert(Py_EVENT(o));
        PyEvent_DESTROY(o);
    }

    px->contexts_destroyed++;

    if (!Px_CTX_WAS_PERSISTED(c)) {
        InterlockedDecrement(&(px->active));
        InterlockedDecrement(&(px->contexts_active));
    }

    /*
    if (c->io_obj) {
        if (Py_TYPE(c->io_obj) == &PxSocket_Type) {
            PxSocket *s = (PxSocket *)c->io_obj;
            if (c->tp_io)
                CancelThreadpoolIo(c->tp_io);
        }
        Py_DECREF(c->io_obj);
    }
    */

#ifdef Py_DEBUG
    _PxContext_UnregisterHeaps(c);
#endif

    HeapDestroy(c->heap_handle);
    free(c);
    return next;
}

int
_PxState_PurgeContexts(PxState *px)
{
    Context *c;
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

        c = _PxState_FreeContext(px, c);
        destroyed++;
    }

    return destroyed;
}

#ifndef _WIN64
#ifndef InterlockedAdd
#define InterlockedAdd InterlockedExchangeAdd
#endif
#endif

void
_PyParallel_SchedulePyNoneDecref(long refs)
{
    PxState *px = PXSTATE();
    assert(refs > 0);
    InterlockedAdd(&(px->incoming_pynone_decrefs), refs);
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
    PyFrameObject *old_frame;
    Py_GUARD();

    if (PyErr_CheckSignals() || _Py_CheckCtrlC())
        return NULL;

    if (!_Py_InstalledCtrlCHandler) {
        if (!SetConsoleCtrlHandler(_Py_CtrlCHandlerRoutine, TRUE)) {
            PyErr_SetFromWindowsErr(0);
            return NULL;
        }
        _Py_InstalledCtrlCHandler = 1;
    }

    _PyParallel_RefreshMemoryLoad();

    tstate = get_main_thread_state();

    px = (PxState *)tstate->px;

    if (px->submitted == 0 &&
        px->waits_submitted == 0 &&
        px->persistent == 0 &&
        px->contexts_persisted == 0 &&
        px->contexts_active == 0)
    {
        PyErr_SetNone(PyExc_AsyncRunCalledWithoutEventsError);
        return NULL;
    }

    /* .oO(Heh.  What on earth was I exploring when I thought this would be
     * necesssary....) */
    if (px->incoming_pynone_decrefs) {
        long r = InterlockedExchange(&(px->incoming_pynone_decrefs), 0);
        assert(r >= 0);
        if (r > 0) {
            PyObject *o = Py_None;
            assert((Py_REFCNT(o) - r) > 0);
            o->ob_refcnt -= r;
        }
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

        assert(!c->io_obj);

        item = (Px_DECCTX(c) ?
            PxList_Transfer(px->finished, item) :
            PxList_SeverFromNext(item)
        );
    }

start:
    if (PyErr_CheckSignals())
        return NULL;

    /* Print uncaught parallel errors to stderr. */
    item = PxList_Flush(px->errors);
    if (item) {
        do {
            c = (Context *)item->from;
            assert(PyExceptionClass_Check((PyObject *)item->p1));
            PyErr_Restore((PyObject *)item->p1,
                          (PyObject *)item->p2,
                          (PyObject *)item->p3);

            item = PxList_Transfer(px->finished, item);
            InterlockedIncrement64(&(px->done));

            if (c->io_obj) {
                PxSocket *s = (PxSocket *)c->io_obj;
                /* Ah, __try/__except, my old friend.  We really need to alter
                   how exceptions are allocated from memory.  This block is
                   necessary 'cause we're racing the context reset/recycle
                   logic in a lot of cases. */
                __try {
                    PyErr_PrintEx(0);
                } __except(
                    GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
                        EXCEPTION_EXECUTE_HANDLER :
                        EXCEPTION_CONTINUE_SEARCH
                ) {
                    ++_PyParallel_EAV_In_PyErr_PrintEx;
                }
                if (s->sock_fd != INVALID_SOCKET) {
                    closesocket(s->sock_fd);
                    s->sock_fd = INVALID_SOCKET;
                }
                c->io_obj = NULL;
            }
        } while (item);
    }

    /* New threadpool work. */
    while (item = PxList_Pop(px->new_threadpool_work)) {
        PTP_SIMPLE_CALLBACK cb;
        c = (Context *)item->from;
        cb = (PTP_SIMPLE_CALLBACK)item->p1;

        if (!TrySubmitThreadpoolCallback(cb, c, c->ptp_cbe)) {
            PyErr_SetFromWindowsErr(0);
            Py_XDECREF(item->p2);
            Py_XDECREF(item->p3);
            Py_XDECREF(item->p4);
            return NULL;
        }
    }

    assert(px->processing_callback == 0);
    /* Process incoming work items. */
    old_frame = ((PyFrameObject *)(tstate->frame));
    ((PyFrameObject *)(tstate->frame)) = NULL;
    while (item = PxList_Pop(px->incoming)) {
        HANDLE wait;
        PyObject *func, *args, *kwds, *result;

        px->processing_callback = 1;

        func = (PyObject *)item->p1;
        args = (PyObject *)item->p2;
        kwds = (PyObject *)item->p3;
        wait = (HANDLE)item->p4;
        c = (Context *)item->from;

        if (wait) {
            InterlockedDecrement(&(px->sync_wait_pending));
            InterlockedIncrement(&(px->sync_wait_inflight));
        } else {
            InterlockedDecrement(&(px->sync_nowait_pending));
            InterlockedIncrement(&(px->sync_nowait_inflight));
        }

        if (kwds)
            assert(PyDict_CheckExact(kwds));

        result = PyObject_Call(func, args, kwds);

        ++processed_incoming;

        if (wait) {
            PxListItem *d;
            d = c->decref;
            assert(
                d &&
                d->p1 == NULL &&
                d->p2 == NULL &&
                d->p3 == NULL &&
                d->p4 == NULL
            );

            if (!result) {
                PyObject *exc_type, *exc_value, *exc_tb;

                assert(tstate->curexc_type);

                PyErr_Fetch(&exc_type, &exc_value, &exc_tb);

                assert(exc_type);
                assert(exc_value);
                assert(exc_tb);

                item->p1 = d->p1 = exc_type;
                item->p2 = d->p2 = exc_value;
                item->p3 = d->p3 = exc_tb;

                PyErr_Clear();

            } else {
                item->p1 = NULL;
                item->p2 = d->p1 = result;
                item->p3 = NULL;
            }
            PxList_Push(c->decrefs, d);
            c->decref = NULL;
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
            Py_XDECREF(result);

            c = (Context *)item->from;
            /* More hacks to persist socket/IO objects. */
            if (c->io_obj)
                continue;

            Px_DECCTX(c);
            _PyHeap_Free(c, item);

            if (!result) {
                px->processing_callback = 0;
                return NULL;
            }
        }
    }
    px->processing_callback = 0;
    ((PyFrameObject *)(tstate->frame)) = old_frame;


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

    if (px->contexts_active == 0 || purged)
        Py_RETURN_NONE;

    /* Return if we've done something useful... */
    if (processed_errors    ||
        processed_finished  ||
        processed_incoming  ||
        processed_errbacks  ||
        processed_callbacks)
            Py_RETURN_NONE;

    /* ...and wait for a bit if we haven't. */
    err = WaitForSingleObject(px->wakeup, 10);
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

    if (c->args == Py_None) {
        Py_DECREF(c->args);
        c->args = Py_BuildValue("()");
    }

    if (c->kwds == Py_None) {
        Py_DECREF(c->kwds);
        c->kwds = NULL;
    }

    if (c->args && !PyTuple_Check(c->args)) {
        PyObject *tmp = c->args;
        c->args = Py_BuildValue("(O)", c->args);
        Py_DECREF(tmp);
    }

    return 1;
}

int
extract_waitobj_args(PyObject *args, Context *c)
{
    if (!PyArg_UnpackTuple(
            args, "", 2, 7,
            &(c->waitobj),
            &(c->waitobj_timeout),
            &(c->func),
            &(c->args),
            &(c->kwds),
            &(c->callback),
            &(c->errback)))
        return 0;

    if (c->waitobj_timeout != Py_None) {
        PyErr_SetString(PyExc_ValueError, "non-None value for timeout");
        return 0;
    }

    if (c->callback == Py_None) {
        Py_DECREF(c->callback);
        c->callback = NULL;
    }

    if (c->errback == Py_None) {
        Py_DECREF(c->errback);
        c->errback = NULL;
    }

    if (c->args == Py_None) {
        Py_DECREF(c->args);
        c->args = Py_BuildValue("()");
    }

    if (c->kwds == Py_None) {
        Py_DECREF(c->kwds);
        c->kwds = NULL;
    }

    if (c->args && !PyTuple_Check(c->args)) {
        PyObject *tmp = c->args;
        c->args = Py_BuildValue("(O)", c->args);
        Py_DECREF(tmp);
    }

    return 1;
}


int
submit_work(Context *c)
{
    int retval;
    PTP_SIMPLE_CALLBACK cb = _PyParallel_WorkCallback;
    if (Py_PXCTX()) {
        assert(c->instance);
        cb(c->instance, c);
        return 1;
    } else {
        retval = TrySubmitThreadpoolCallback(cb, c, c->ptp_cbe);
        if (!retval)
            PyErr_SetFromWindowsErr(0);
        return retval;
    }
}

int
create_threadpool_for_context(Context *c,
                              DWORD min_threads,
                              DWORD max_threads);

Context *
new_context(size_t heapsize)
{
    PxState *px;
    Stats *s;
    PyThreadState *pstate;
    Context *c;

    if (_PyParallel_HitHardMemoryLimit())
        return (Context *)PyErr_NoMemory();

    c = (Context *)malloc(sizeof(Context));

    if (!c)
        return (Context *)PyErr_NoMemory();

    SecureZeroMemory(c, sizeof(Context));

    c->heap_handle = HeapCreate(HEAP_NO_SERIALIZE, Px_DEFAULT_HEAP_SIZE, 0);
    if (!c->heap_handle) {
        PyErr_SetFromWindowsErr(0);
        goto free_context;
    }

    c->tstate = get_main_thread_state();

    assert(c->tstate);
    px = c->px = (PxState *)c->tstate->px;

    if (!_PyHeap_Init(c, heapsize))
        goto free_heap;

    c->refcnt = 1;
    c->ttl = px->ctx_ttl;

    pstate = (PyThreadState *)_PyHeap_Malloc(c, sizeof(PyThreadState), 0, 0);
    c->pstate = pstate;

    c->error = _PyHeap_NewListItem(c);
    c->errback_completed = _PyHeap_NewListItem(c);
    c->callback_completed = _PyHeap_NewListItem(c);

    c->outgoing = _PyHeap_NewList(c);
    c->decrefs  = _PyHeap_NewList(c);

    if (!(c->error                &&
          c->pstate               &&
          c->decrefs              &&
          c->outgoing             &&
          c->errback_completed    &&
          c->callback_completed))
            goto free_heap;

    pstate->px = c;
    pstate->is_parallel_thread = 1;
    pstate->interp = c->tstate->interp;

    //c->px->contexts_created++;
    InterlockedIncrement64(&(c->px->contexts_created));
    InterlockedIncrement(&(c->px->contexts_active));

    s = &(c->stats);
    s->startup_size = s->allocated;

    if (ctx) {
        Context *x = ctx;
        if (x && x->tp_ctx) {
            if (x->tp_ctx != x)
                __debugbreak();

            if (x->ptp_cbe != &x->tp_cbe)
                __debugbreak();

            c->tp_ctx = x;
            c->ptp_cbe = x->ptp_cbe;
        }
    }

    return c;

free_heap:
    HeapDestroy(c->heap_handle);

free_context:
    free(c);

    return NULL;
}


void PxSocketServer_Shutdown(PxSocket *s);


void
CALLBACK
PxSocket_ThreadpoolCleanupGroupCancelCallback(
    _Inout_opt_ PVOID object_context,
    _Inout_opt_ PVOID cleanup_context
)
{
    Context *c = (Context *)object_context;
    PxSocket *parent = (PxSocket *)cleanup_context;
    PxSocket *child = NULL;


    if (!c)
        __debugbreak();

    if (Px_PTR(parent) == Px_PTR(c->io_obj)) {
        closesocket(parent->sock_fd);
        parent->sock_fd = INVALID_SOCKET;
        return;
    }

    child = (PxSocket *)c->io_obj;
    __debugbreak();

    __try {
        //closesocket(child->sock_fd);
        //child->sock_fd = INVALID_SOCKET;
        PxSocket_CallbackComplete(child);
    } __except(
        GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
            EXCEPTION_EXECUTE_HANDLER :
            EXCEPTION_CONTINUE_SEARCH
    ) {
        __debugbreak();
        ++parent->child_seh_eav_count;
    }
}

/* 0 = failure, 1 = success */
int
create_threadpool_for_context(Context *c,
                              DWORD min_threads,
                              DWORD max_threads)
{
    PxState *px = c->px;
    DWORD err = NO_ERROR;

    assert(!c->ptp);
    assert(!c->ptp_cg);
    assert(!c->ptp_cgcb);
    assert(!c->ptp_cbe);
    assert(!c->tp_ctx);

    c->ptp = CreateThreadpool(NULL);
    if (!c->ptp) {
        err = GetLastError();
        __debugbreak();
    }

    c->ptp_cbe = &c->tp_cbe;
    InitializeThreadpoolEnvironment(c->ptp_cbe);

    c->ptp_cg = CreateThreadpoolCleanupGroup();
    if (!c->ptp_cg) {
        err = GetLastError();
        __debugbreak();
    }

    SetThreadpoolThreadMaximum(c->ptp, max_threads);
    if (!SetThreadpoolThreadMinimum(c->ptp, min_threads)) {
        err = GetLastError();
        __debugbreak();
    }

    SetThreadpoolCallbackPool(c->ptp_cbe, c->ptp);

    c->ptp_cgcb = PxSocket_ThreadpoolCleanupGroupCancelCallback;
    SetThreadpoolCallbackCleanupGroup(c->ptp_cbe, c->ptp_cg, c->ptp_cgcb);

    c->tp_ctx = c;

    /* Only top-level contexts (i.e. those with threadpool contexts) get added
     * to the PxState list of contexts. */
    EnterCriticalSection(&px->contexts_cs);
    InsertTailList(&px->contexts, &c->px_link);
    LeaveCriticalSection(&px->contexts_cs);

    return 1;
}

Context *
new_context_for_socket(size_t heapsize)
{
    Context *c = NULL;
    Context *x = ctx;
    int copy_tp_info = 0;
    c = new_context(heapsize);

    if (!c)
        return NULL;

    if (c->tp_ctx) {
        if (x->tp_ctx != x)
            __debugbreak();
        if (c->ptp_cbe != x->ptp_cbe)
            __debugbreak();
    } else {
        DWORD min_threads = _PyParallel_NumCPUs;
        DWORD max_threads = _PyParallel_NumCPUs * 2;
        if (!create_threadpool_for_context(c, min_threads, max_threads))
            __debugbreak();
    }

    return c;
}

PyObject *
_async_submit_work(PyObject *self, PyObject *args)
{
    PyObject *result = NULL;
    Context  *c;
    PxState  *px;
    PxListItem *item;

    c = new_context(0);
    if (!c)
        return NULL;

    px = c->px;

    if (!extract_args(args, c))
        goto free_context;

    item = _PyHeap_NewListItem(c);
    if (!item)
        goto free_context;

    item->from = c;

    InterlockedIncrement64(&(px->submitted));
    InterlockedIncrement(&(px->pending));
    InterlockedIncrement(&(px->active));
    c->stats.submitted = _Py_rdtsc();

    if (!submit_work(c))
        goto error;

    incref_args(c);

    result = (Py_INCREF(Py_None), Py_None);
    goto done;

error:
    InterlockedDecrement(&(c->px->contexts_active));
    InterlockedDecrement(&(px->pending));
    InterlockedDecrement(&(px->active));
    InterlockedIncrement64(&(px->done));
    decref_args(c);

free_context:
    HeapDestroy(c->heap_handle);
    free(c);

done:
    if (!result)
        assert(PyErr_Occurred());
    return result;
}

/*
PyObject *
_async_socket(PyObject *self, PyObject *args)
{
    PyObject *result = NULL;
    PySocketSockObject *sock;

    if (!PyArg_ParseTuple(args, "O!:socket", PySocketModule.Sock_Type, &sock))
        return NULL;

    return sock;
}
*/

PyObject *
_async_enabled(PyObject *self, PyObject *args)
{
    /*
    if (!PyArg_ParseTuple(args, "O!:socket", PySocketModule.Sock_Type, &sock))
        return NULL;
        */
    return NULL;
}

/*
PyObject *
_async_socket(PyObject *self, PyObject *args)
{
    PyObject *result = NULL;
    return result;
}
*/


PyObject *
_async_run(PyObject *self, PyObject *args)
{
    PyThreadState *tstate = get_main_thread_state();
    PxState *px = PXSTATE();
    int i = 0;
    long active_contexts = 0;
    long persisted_contexts = 0;
    do {
        i++;
        active_contexts = px->contexts_active;
        persisted_contexts = px->contexts_persisted;
        if (Py_VerboseFlag)
            PySys_FormatStdout("_async.run(%d) [%d/%d] "
                               "(hogs: %d, ioloops: %d)\n",
                               i, active_contexts, persisted_contexts,
                               _PxSocket_ActiveHogs, _PxSocket_ActiveIOLoops);
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
    Context  *c;
    PxState  *px;
    PTP_WAIT_CALLBACK cb;

    c = new_context(0);
    if (!c)
        return NULL;

    px = c->px;

    if (!extract_waitobj_args(args, c))
        goto free_context;

    cb = _PyParallel_WaitCallback;
    c->tp_wait = CreateThreadpoolWait(cb, c, c->ptp_cbe);
    if (!c->tp_wait) {
        PyErr_SetFromWindowsErr(0);
        goto free_context;
    }

    if (!_PyEvent_TryCreate(c->waitobj))
        goto free_context;

    SetThreadpoolWait(c->tp_wait, Py_EVENT(c->waitobj), NULL);

    InterlockedIncrement64(&(px->waits_submitted));
    InterlockedIncrement(&(px->waits_pending));
    InterlockedIncrement(&(px->active));
    c->stats.submitted = _Py_rdtsc();

    incref_waitobj_args(c);

    result = (Py_INCREF(Py_None), Py_None);
    goto done;

free_context:
    InterlockedDecrement(&(c->px->contexts_active));
    HeapDestroy(c->heap_handle);
    free(c);

done:
    if (!result)
        assert(PyErr_Occurred());
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
    return NULL;
}

/*
PxIO *
get_pxio(int size)
{
    PxIO *io = NULL;
    PxState *px = (Py_PXCTX() ? ctx->px : PXSTATE());
    short io_attempt = 0;
    if (size > PyAsync_IO_BUFSIZE) {
alloc_io:
        io = (PxIO *)PxList_Malloc(sizeof(PxIO));
        if (!io)
            return PyErr_NoMemory();
        io->flags = PxIO_ONDEMAND;
        io->buf = (char *)malloc(size);
        if (!io->buf) {
            PxList_Free(io);
            return PyErr_NoMemory();
        }
        io->size = size;
    } else {
try_io:
        io_attempt++;
        io = (PxIO *)PxList_Pop(px->io_free);
        if (!io) {
            if (io_attempt > 1)
                goto alloc_io;
            else {
                int r;
                InterlockedIncrement64(&(px->io_stalls));
                r = WaitForSingleObject(px->io_free_wakeup, 100);
                if (r == WAIT_OBJECT_0 || r == WAIT_TIMEOUT)
                    goto try_io;
                else {
                    PyErr_SetExcFromWindowsErr(
                        PyExc_AsyncIOBuffersExhaustedError, 0);
                    goto free_context;
                }
            }
        }
        assert(io);
        assert(PxIO_IS_PREALLOC(io));
    }
    assert(io);
    return io;
} */

#define PxSocketIO_Check(o) (0)

PyObject *
_async_submit_write_io(PyObject *self, PyObject *args)
{
    PyObject *o, *cb, *eb, *result = NULL;
    Py_buffer pybuf;
    fileio   *f;
    Context  *c;
    PxState  *px;
    PxIO     *io;
    char     *buf;
    char      success;
    int is_file = 0;
    int is_socket = 0;
    int io_attempt = 0;
    PTP_WIN32_IO_CALLBACK callback;

    if (!PyArg_ParseTuple(args, "Oy*OO", &o, &pybuf, &cb, &eb))
        return NULL;

    is_file = PyFileIO_Check(o);
    is_socket = PxSocketIO_Check(o);
    assert(!(is_file && is_socket));

    if (is_socket) {
        PyErr_SetString(PyExc_ValueError, "sockets not supported yet");
        return NULL;
    } else {
        assert(is_file);
        f = (fileio *)o;
        if (!f->native) {
            PyErr_SetString(PyExc_ValueError,
                            "file was not opened with async.open()");
            return NULL;
        }
    }

    Px_PROTECTION_GUARD(o);

    if (!_PyEvent_TryCreate(o))
        return NULL;

    c = new_context(0);
    if (!c)
        return NULL;

    px = c->px;

    c->callback = (cb == Py_None ? NULL : cb);
    c->errback  = (eb == Py_None ? NULL : eb);
    c->func = NULL;
    c->args = NULL;
    c->kwds = NULL;

    Py_XINCREF(c->callback);
    Py_XINCREF(c->errback);

    callback = _PyParallel_IOCallback;
    c->tp_io = CreateThreadpoolIo(f->h, callback, c, c->ptp_cbe);
    if (!c->tp_io) {
        PyErr_SetFromWindowsErr(0);
        goto free_context;
    }
    c->io_type = Px_IOTYPE_FILE;

    if (pybuf.len > PyAsync_IO_BUFSIZE) {
alloc_io:
        io = (PxIO *)PxList_Malloc(sizeof(PxIO));
        if (!io) {
            PyErr_NoMemory();
            goto free_context;
        }
        io->flags = PxIO_ONDEMAND;
        buf = (char *)malloc(pybuf.len);
        if (!buf) {
            PxList_Free(io);
            PyErr_NoMemory();
            goto free_context;
        }
        io->buf = buf;
    } else {
try_io:
        io_attempt++;
        io = (PxIO *)PxList_Pop(px->io_free);
        if (!io) {
            if (io_attempt > 1)
                goto alloc_io;
            else {
                int r;
                InterlockedIncrement64(&(px->io_stalls));
                /* XXX TODO create more buffers or wait for existing buffers. */
                /* xxx todo: convert to submit_wait */
                r = WaitForSingleObject(px->io_free_wakeup, 100);
                if (r == WAIT_OBJECT_0 || r == WAIT_TIMEOUT)
                    goto try_io;
                else {
                    PyErr_SetExcFromWindowsErr(
                        PyExc_AsyncIOBuffersExhaustedError, 0);
                    goto free_context;
                }
            }
        }
        assert(io);
        assert(PxIO_IS_PREALLOC(io));
    }
    assert(io);

    /* Ugh.  This is ass-backwards.  Need to refactor PxIO to support
     * Py_buffer natively. */
    io->len = (ULONG)pybuf.len;
    memcpy(io->buf, pybuf.buf, pybuf.len);
    PyBuffer_Release(&pybuf);

    io->obj = o;
    c->io_type = PyAsync_IO_WRITE;

    Py_INCREF(io->obj);
    _write_lock(o);
    io->overlapped.Offset = f->write_offset.LowPart;
    io->overlapped.OffsetHigh = f->write_offset.HighPart;
    f->write_offset.QuadPart += io->size;
    _write_unlock(o);

    StartThreadpoolIo(c->tp_io);

    InterlockedIncrement64(&(px->io_submitted));
    InterlockedIncrement(&(px->io_pending));
    InterlockedIncrement(&(px->active));
    c->stats.submitted = _Py_rdtsc();
    c->px->contexts_created++;
    InterlockedIncrement(&(c->px->contexts_active));

    success = WriteFile(f->h, io->buf, io->size, NULL, &(io->overlapped));
    if (!success) {
        int last_error = GetLastError();
        if (last_error != ERROR_IO_PENDING) {
            CancelThreadpoolIo(c->tp_io);
            InterlockedDecrement(&(px->io_pending));
            InterlockedDecrement(&(px->active));
            InterlockedIncrement64(&(px->done));
            if (c->errback)
                PyErr_Warn(PyExc_RuntimeWarning,
                           "file io errbacks not yet supported");
            PyErr_SetFromWindowsErr(0);
            goto free_io;
        } else {
            result = Py_None;
            goto done;
        }
    } else {
        CancelThreadpoolIo(c->tp_io);
        InterlockedDecrement(&(px->io_pending));
        InterlockedDecrement(&(px->active));
        InterlockedIncrement64(&(px->done));
        InterlockedIncrement64(&(px->async_writes_completed_synchronously));
        PySys_WriteStdout("_async.write() completed synchronously\n");
        result = Py_True;
        if (c->callback)
            PyErr_Warn(PyExc_RuntimeWarning,
                       "file io callbacks not yet supported");
        goto free_io;
    }

    assert(0); /* unreachable */

free_io:
    if (PxIO_IS_ONDEMAND(io)) {
        free(io->buf);
        PxList_Free(io);
    }

free_context:
    InterlockedDecrement(&(px->contexts_active));
    px->contexts_destroyed++;
    free(c);

done:
    if (!result)
        assert(PyErr_Occurred());
    else {
        assert(result == Py_None || result == Py_True);
        Py_INCREF(result);
    }
    return result;
}

PyObject *
_async_submit_read_io(PyObject *self, PyObject *args)
{
    //return _async_submit_io(self, args, 0);
    return NULL;
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
_call_from_main_thread(PyObject *self, PyObject *targs, int wait)
{
    int err;
    Context *c;
    PyObject *result = NULL;
    PxListItem *item;
    PxState *px;
    PyObject *func, *arg, *args, *kwds, *tmp;

    Px_GUARD();

    func = arg = args = kwds = tmp = NULL;

    c = ctx;
    assert(!c->pstate->curexc_type);

    item = _PyHeap_NewListItem(c);
    if (!item)
        return PyErr_NoMemory();

    if (wait) {
        assert(c->decref == NULL);
        c->decref = _PyHeap_NewListItem(c);
        if (!c->decref) {
            PyErr_NoMemory();
            goto error;
        }
        c->decref->from = c;
    }

    if (!PyArg_UnpackTuple(targs, "call_from_main_thread",
                           1, 3, &func, &arg, &kwds))
        goto error;

    assert(func);
    if (func == Py_None || !PyCallable_Check(func)) {
        PyErr_SetString(PyExc_TypeError, "parameter 1 must be callable");
        goto error;
    }

    if (kwds && kwds == Py_None)
        kwds = NULL;

    if (kwds) {
        if (!PyDict_CheckExact(kwds)) {
            PyErr_SetString(PyExc_TypeError, "param 3 must be None or dict");
            goto error;
        }
    }

    if (arg) {
        if (PyTuple_Check(arg))
            args = arg;
        else {
            args = Py_BuildValue("(O)", arg);
            if (!args)
                goto error;
        }
    } else {
        args = PyTuple_New(0);
        if (!args)
            goto error;
    }

    item->p1 = func;
    item->p2 = args;
    item->p3 = kwds;

    if (wait) {
        item->p4 = (void *)CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!item->p4) {
            PyErr_SetFromWindowsErr(0);
            goto error;
        }
    } else
        assert(item->p4 == NULL);

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
    assert(c->decref == NULL);
    InterlockedDecrement(&(px->sync_wait_inflight));
    InterlockedIncrement64(&(px->sync_wait_done));

    CloseHandle(item->p4);
    item->p4 = NULL;
error:
    _PyHeap_Free(c, item);

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

PyObject *
_async_filecloser(PyObject *self, PyObject *args)
{
    fileio *f;
    PyObject *o, *result = NULL;

    if (!PyTuple_Check(args)) {
        PyErr_BadInternalCall();
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "O!", &PyFileIO_Type, &f))
        return NULL;

    if (!f->h || f->h == INVALID_HANDLE_VALUE) {
        PyErr_BadInternalCall();
        return NULL;
    }

    o = (PyObject *)f;

    _write_lock(o);
    if (f->size > 0 && f->writable) {
        LPCWSTR n;
        Py_UNICODE *u;
        LARGE_INTEGER i;

        u = f->name;
        n = (LPCWSTR)u;

        Px_PROTECTION_GUARD(o);
        /* Close the file and re-open without FILE_FLAG_NO_BUFFERING in order
         * to set the EOF marker to the correct position (as opposed to the
         * sector-aligned position we set it to as part of _async_fileopener).
         */
        CloseHandle(f->h);
        f->h = CreateFile(n, GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL, OPEN_EXISTING, 0, NULL);

        if (!f->h || f->h == INVALID_HANDLE_VALUE) {
            PyErr_SetFromWindowsErrWithUnicodeFilename(0, u);
            goto done;
        }

        i.QuadPart = f->size;
        if (!SetFilePointerEx(f->h, i, NULL, FILE_BEGIN)) {
            PyErr_SetFromWindowsErrWithUnicodeFilename(0, u);
            goto done;
        }

        if (!SetEndOfFile(f->h)) {
            PyErr_SetFromWindowsErrWithUnicodeFilename(0, u);
            goto done;
        }
    }

    CloseHandle(f->h);
    f->fd = -1;
    result = Py_True;

done:
    _write_unlock(o);
    if (!result)
        assert(PyErr_Occurred());

    Py_XINCREF(result);
    return result;
}


PyObject *
_async__close(PyObject *self, PyObject *obj)
{
    fileio   *f;

    Py_INCREF(obj);
    if (!PyFileIO_Check(obj)) {
        PyErr_SetString(PyExc_ValueError, "not an io file object");
        return NULL;
    }

    f = (fileio *)obj;
    Py_DECREF(f->owner);

    Py_RETURN_NONE;
}

PyObject *
_async_fileopener(PyObject *self, PyObject *args)
{
    LPCWSTR name;
    Py_UNICODE *uname;
    int namelen;
    int flags;
    int caching_behavior;

    int access = 0;
    int share = 0;
    char notif_flags;

    int create_flags = 0;
    int file_flags = FILE_FLAG_OVERLAPPED;

    int exists = 1;

    Py_ssize_t size = 0;

    PyObject *templ;
    PyObject *result = NULL;
    PyObject *fileobj;
    fileio   *f;

    HANDLE h;
    WIN32_FIND_DATA d;

    if (!PyArg_ParseTuple(args, "inOu#iO:fileopener", &caching_behavior,
                          &size, &templ, &uname, &namelen, &flags, &fileobj))
        return NULL;

    name = (LPCWSTR)uname;

    h = FindFirstFile(name, &d);
    if (h && h != INVALID_HANDLE_VALUE) {
        if (!FindClose(h)) {
            PyErr_SetFromWindowsErrWithUnicodeFilename(0, uname);
            goto done;
        }
    } else
        exists = 0;

    if (exists && (flags & O_EXCL)) {
        assert(!(flags & O_APPEND));
        PyErr_SetExcFromWindowsErrWithUnicodeFilename(
            PyExc_OSError,
            EEXIST,
            uname
        );
        goto done;
    } else if (!exists && (flags & O_RDONLY)) {
        assert(!(flags & O_APPEND));
        PyErr_SetExcFromWindowsErrWithUnicodeFilename(
            PyExc_OSError,
            ENOENT,
            uname
        );
        goto done;
    }

    if (flags & O_RDONLY)
        file_flags |= FILE_ATTRIBUTE_READONLY;

    if (flags & (O_RDWR | O_RDONLY)) {
        access |= GENERIC_READ;
        share  |= FILE_SHARE_READ;
    }

    if (flags & (O_RDWR | O_WRONLY)) {
        access |= GENERIC_WRITE;
        share  |= FILE_SHARE_WRITE;
    }

    if (flags & O_APPEND) {
        access |= FILE_APPEND_DATA;
        share  |= FILE_SHARE_WRITE;
    }

    /* There's not a 1:1 mapping between create flags and POSIX flags, so
     * the following code is a bit fiddly. */
    if (flags & O_RDONLY)
        create_flags = OPEN_EXISTING;

    else if (flags & O_WRONLY)
        create_flags = (exists ? TRUNCATE_EXISTING : CREATE_ALWAYS);

    else if ((flags & O_RDWR) || (flags & O_APPEND))
        create_flags = (exists ? OPEN_EXISTING : CREATE_ALWAYS);

    else if (flags & O_EXCL)
        create_flags = CREATE_NEW;

    else if (flags & O_APPEND)
        create_flags = (exists ? OPEN_EXISTING : CREATE_ALWAYS);

    else {
        PyErr_SetString(PyExc_ValueError, "unexpected value for flags");
        goto done;
    }

    switch (caching_behavior) {
        case PyAsync_CACHING_DEFAULT:
            file_flags |= FILE_FLAG_NO_BUFFERING;
            break;

        case PyAsync_CACHING_BUFFERED:
            break;

        case PyAsync_CACHING_RANDOMACCESS:
            file_flags |= FILE_FLAG_RANDOM_ACCESS;
            break;

        case PyAsync_CACHING_SEQUENTIALSCAN:
            file_flags |= FILE_FLAG_SEQUENTIAL_SCAN;
            break;

        case PyAsync_CACHING_WRITETHROUGH:
            file_flags |= FILE_FLAG_WRITE_THROUGH;
            break;

        case PyAsync_CACHING_TEMPORARY:
            file_flags |= FILE_ATTRIBUTE_TEMPORARY;
            break;

        default:
            PyErr_Format(PyExc_ValueError,
                         "invalid caching behavior: %d",
                         caching_behavior);
            goto done;
    }

    h = CreateFile(name, access, share, 0, create_flags, file_flags, 0);
    if (!h || h == INVALID_HANDLE_VALUE) {
        PyErr_SetFromWindowsErrWithUnicodeFilename(0, uname);
        goto done;
    } else if (size > 0) {
        LARGE_INTEGER i;
        i.QuadPart = Px_PAGE_ALIGN(size);
        if (!SetFilePointerEx(h, i, NULL, FILE_BEGIN)) {
            CloseHandle(h);
            PyErr_SetFromWindowsErrWithUnicodeFilename(0, uname);
            goto done;
        }
        if (!SetEndOfFile(h)) {
            CloseHandle(h);
            PyErr_SetFromWindowsErrWithUnicodeFilename(0, uname);
            goto done;
        }
    }

    notif_flags = (
        FILE_SKIP_COMPLETION_PORT_ON_SUCCESS |
        FILE_SKIP_SET_EVENT_ON_HANDLE
    );
    if (!SetFileCompletionNotificationModes(h, notif_flags)) {
        CloseHandle(h);
        PyErr_SetFromWindowsErrWithUnicodeFilename(0, uname);
    }

    if (!_protect(fileobj))
        goto done;

    f = (fileio *)fileobj;
    f->h = h;
    f->native = 1;
    f->istty = 0;
    f->name = uname;
    f->caching = caching_behavior;

    result = PyLong_FromVoidPtr(h);
done:
    if (!result)
        assert(PyErr_Occurred());

    return result;
}

PyObject *
_async__post_open(PyObject *self, PyObject *args)
{
    fileio     *f;
    PyObject   *o;
    Py_ssize_t  size;
    int         caching;
    int         is_write;

    if (!PyArg_ParseTuple(args, "Onii", &o, &caching, &size, &is_write))
        return NULL;

    f = (fileio *)o;
    f->size = size;
    f->caching = caching;

    if (is_write)
        assert(f->writable);

    if (size > 0 && !is_write) {
        PyErr_SetString(PyExc_ValueError,
                        "non-zero size invalid for read-only files");
            return NULL;
    }

    Py_RETURN_NONE;
}

PyObject *
_async__address(PyObject *self, PyObject *o)
{
    Py_INCREF(o);
    return PyLong_FromVoidPtr(o);
}

PyObject *
_async__dbg_address(PyObject *self, PyObject *addr)
{
    PyObject *o;
    Py_INCREF(addr);
    o = (PyObject *)PyLong_AsVoidPtr(addr);
    PySys_FormatStdout("address: 0x%x, refcnt: %d\n", o, Py_REFCNT(o));
    Py_RETURN_NONE;
}

/* Helper inline function that the macros use (allowing breakpoints to be set
 * at object creation time, which is useful when debugging). */

PyObject *
_wrap(PyTypeObject *tp, PyObject *args, PyObject *kwds)
{
    PyObject *self;
    Py_GUARD();
    if (!(self = _protect(tp->tp_new(tp, args, kwds))))
        return NULL;

    if (kwds && !args)
        args = PyTuple_New(0);

    if (args && tp->tp_init(self, args, kwds)) {
        Py_DECREF(self);
        return NULL;
    }

    return self;
}

#define _ASYNC_WRAP(name, type)                                                \
PyDoc_STRVAR(_async_##name##_doc,                                              \
"Helper function for creating an async-protected instance of type '##name'\n");\
PyObject *                                                                     \
_async_##name(PyObject *self, PyObject *args, PyObject *kwds)                  \
{                                                                              \
    return _wrap(&type, args, kwds);                                           \
}

_ASYNC_WRAP(list, PyList_Type)
_ASYNC_WRAP(dict, PyDict_Type)

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
\n\
Socket IO functions:\n\
connect(sock, (host, port)[, buf[, callback[, errback]]])\n\
");

PyDoc_STRVAR(_async_run_doc,
"run() -> None\n\
\n\
Runs the _async event loop.");


PyDoc_STRVAR(_async_unregister_doc,
"unregister(object) -> None\n\
\n\
Unregisters an asynchronous object.");


PyDoc_STRVAR(_async_read_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_open_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_pipe_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_write_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_fileopener_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_filecloser_doc, "XXX TODO\n");
PyDoc_STRVAR(_async__address_doc, "XXX TODO\n");
PyDoc_STRVAR(_async__dbg_address_doc, "XXX TODO\n");
PyDoc_STRVAR(_async__close_doc, "XXX TODO\n");
PyDoc_STRVAR(_async__rawfile_doc,"XXX TODO\n");
PyDoc_STRVAR(_async__post_open_doc,"XXX TODO\n");
PyDoc_STRVAR(_async_submit_write_io_doc, "XXX TODO\n");

PyDoc_STRVAR(_async_map_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_wait_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_rdtsc_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_client_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_server_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_signal_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_prewait_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_protect_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_run_once_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_unprotect_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_protected_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_is_active_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_read_lock_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_read_unlock_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_try_read_lock_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_write_lock_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_write_unlock_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_try_write_lock_doc, "XXX TODO\n");
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
PyDoc_STRVAR(_async_persisted_contexts_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_signal_and_wait_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_is_parallel_thread_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_call_from_main_thread_doc, "XXX TODO\n");
PyDoc_STRVAR(_async_call_from_main_thread_and_wait_doc, "XXX TODO\n");

/* And now for the exported symbols... */
PyThreadState *
_PyParallel_GetThreadState(void)
{
    Context *c = ctx;
    Px_GUARD();
    if (!c)
        c = tmp_ctx;
    if (!c)
        __debugbreak();
    if (!c->pstate)
        __debugbreak();
    if (c->pstate == c->tstate)
        __debugbreak();
    return c->pstate;
}

void
_Px_NewReference(PyObject *op)
{
    Px_GUARD();
    Px_GUARD_MEM(op);

#ifdef Py_DEBUG
    if (!_Px_TEST(op))
        printf("\n_Px_NewReference(op) -> failed _Px_TEST!\n");
    if (!Py_ASPX(op))
        printf("\n_Px_NewReference: no px object!\n");
#endif

    assert(op->is_px != _Py_NOT_PARALLEL);

    if (op->is_px != _Py_IS_PARALLEL)
        op->is_px = _Py_IS_PARALLEL;

    assert(Py_TYPE(op));

    op->ob_refcnt = 1;
    ctx->stats.newrefs++;
}

void
_Px_ForgetReference(PyObject *op)
{
    Px_GUARD_OBJ(op);
    Px_GUARD();
    ctx->stats.forgetrefs++;
}

void
_Px_Dealloc(PyObject *op)
{
    Px_GUARD_OBJ(op);
    Px_GUARD();
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
    Px_GUARD();
    return _PyHeap_Malloc(ctx, n, 0, 0);
}

void *
_PxMem_Realloc(void *p, size_t n)
{
    return _PxObject_Realloc(p, n);
}

void
_PxMem_Free(void *p)
{
    _PxObject_Free(p);
}

void
_Px_NegativePersistedCount(const char *fname, int lineno, Context *c, int count)
{
    char buf[300];

    PyOS_snprintf(buf, sizeof(buf),
                  "%s:%i context at %p has negative ref count "
                  "%" PY_FORMAT_SIZE_T "d",
                  fname, lineno, c, count);
    Py_FatalError(buf);
}


void
Px_DecRef(PyObject *o)
{
    assert(!Py_PXCTX());
    assert(Px_PERSISTED(o) || Px_CLONED(o));

    _Py_DEC_REFTOTAL;
    if ((--((PyObject *)(o))->ob_refcnt) != 0) {
        _Py_CHECK_REFCNT(o);
    } else {
        if (Px_PERSISTED(o)) {
            Context *c = Py_ASPX(o)->ctx;
            int count = InterlockedDecrement(&(c->persisted_count));
            if (count < 0)
                _Px_NegativePersistedCount(__FILE__, __LINE__, c, count);
            else if (count == 0) {
                InterlockedDecrement(&(c->px->contexts_persisted));
                _PxState_FreeContext(c->px, c);
            }
            return;
        } else {
            assert(Px_CLONED(o));
            /* xxx todo: decref parent's children count */

            return;
        }

        assert(0);
    }
}

const char *
PxSocket_GetRecvCallback(PxSocket *s)
{
    int lines_mode;
    READ_LOCK(s);
    lines_mode = PyObject_IsTrue(PxSocket_GET_ATTR("lines_mode"));
    READ_UNLOCK(s);
    return (lines_mode ? "lines_received" : "data_received");
}

/* 0 = failure, 1 = success */
int
PxSocket_UpdateConnectTime(PxSocket *s)
{
    int seconds;
    int bytes = sizeof(seconds);
    int result = 0;
    char *b = (char *)&seconds;
    int  *n = &bytes;
    SOCKET fd = s->sock_fd;

    if (getsockopt(fd, SOL_SOCKET, SO_CONNECT_TIME, b, n) != NO_ERROR)
        goto end;

    s->connect_time = seconds;

    result = 1;

end:
    return result;
}

int PxSocket_InitInitialBytes(PxSocket *s);
int PxSocket_InitNextBytes(PxSocket *s);
int PxSocket_LoadNextBytes(PxSocket *s);

/* Hybrid sync/async IO loop. */
void
PxSocket_IOLoop(PxSocket *s)
{
    /* Note to self: there are a bunch of lines that are commented out
       calls to heap rollback, e.g.:

        if (!PxSocket_LoadInitialBytes(s)) {
            //PxContext_RollbackHeap(c, &snapshot);
            PxSocket_EXCEPTION();
        }

       Context: we were initially rolling back heaps on failure.  However,
       this currently memsets all that memory back to 0 as part of the
       rewind, which will include whatever tstate->cur_exc etc point at,
       which PxSocket_EXCEPTION() will push to the main thread's error
       list.

       So, comment out the heap rollback for now to stop that.  (Leaving
       it in is useful though, as it marks exceptional exit points.) */
    PyObject *func, *args, *result;
    PyBytesObject *bytes;
    TLS *t = &tls;
    Context *c = ctx;
    char *buf = NULL;
    DWORD err, wsa_error;
    SOCKET fd;
    WSABUF *w = NULL, *old_wsabuf = NULL;
    SBUF *sbuf = NULL;
    RBUF *rbuf = NULL;
    ULONG recv_avail = 0;
    ULONG rbuf_size = 0;
    DWORD recv_flags = 0;
    DWORD recv_nbytes = 0;
    Heap *snapshot = NULL;
    int i, n, success, flags = 0;
    TRANSMIT_FILE_BUFFERS *tf = NULL;

    /* AcceptEx stuff */
    DWORD sockaddr_len, local_addr_len, remote_addr_len;
    LPSOCKADDR local_addr;
    LPSOCKADDR remote_addr;

    sockaddr_len = sizeof(SOCKADDR);
    s->acceptex_addr_len = sockaddr_len + 16;

    fd = s->sock_fd;

    assert(s->ctx == c);
    assert(c->io_obj == (PyObject *)s);

    InterlockedIncrement(&_PxSocket_ActiveIOLoops);

    s->last_thread_id = s->this_thread_id;
    s->this_thread_id = _Py_get_current_thread_id();
    s->last_cpuid = s->this_cpuid;
    /* __rdtscp() crashes with a "privileged instruction" exception on Azure
     * (and presumably any other CPU that doesn't support RDTSCP).  So, like,
     * let's not do it for now. */
    //__rdtscp(&(s->this_cpuid));
    s->last_io_op = s->this_io_op;
    s->this_io_op = 0;

    s->thread_seq_id_bitmap |= (
        1ULL << (_PyParallel_ThreadSeqId % Px_INTPTR_BITS)
    );

    s->ioloops++;

    ODS(L"PxSocket_IOLoop()\n");

    if (s->next_io_op) {
        int op = s->next_io_op;
        s->next_io_op = 0;
        s->in_overlapped_callback = 0;
        switch (op) {
            case PxSocket_IO_CONNECT:
                s->this_io_op = PxSocket_IO_CONNECT;
                goto do_connect;
            case PxSocket_IO_ACCEPT:
                goto do_accept;
            default:
                assert(0);
        }
    } else {
        s->in_overlapped_callback = 1;
        assert(s->last_io_op);
        switch (s->last_io_op) {
            case PxSocket_IO_ACCEPT:
                //PxSocket_UpdateConnectTime(s);
                goto overlapped_acceptex_callback;

            case PxSocket_IO_CONNECT:
                //PxSocket_UpdateConnectTime(s);
                goto overlapped_connectex_callback;

            case PxSocket_IO_SEND:
                goto overlapped_send_callback;

            case PxSocket_IO_SENDFILE:
                goto overlapped_sendfile_callback;

            case PxSocket_IO_RECV:
                goto overlapped_recv_callback;

            case PxSocket_IO_DISCONNECT:
                goto overlapped_disconnectex_callback;

            default:
                ASSERT_UNREACHABLE();

        }
    }

    ASSERT_UNREACHABLE();

do_connect:
    ODS(L"do_connect:\n");

    if (!PxSocket_InitInitialBytes(s))
        PxSocket_FATAL();

    if (!PxSocket_InitNextBytes(s))
        PxSocket_FATAL();

    if (0 && s->initial_bytes.len) {
        if (s->rbuf->w.buf != s->initial_bytes.buf)
            __debugbreak();
        if (s->rbuf->w.len != s->initial_bytes.len)
            __debugbreak();
    }

    s->this_io_op = PxSocket_IO_CONNECT;
    PxOverlapped_Reset(&s->overlapped_connectex);
    StartThreadpoolIo(s->tp_io);
    s->was_connecting = 1;
    success = ConnectEx(s->sock_fd,
                        &s->remote_addr.sa,
                        s->remote_addr_len,
                        s->initial_bytes.buf,
                        s->initial_bytes.len,
                        /*
                        s->rbuf->w.buf,
                        s->rbuf->w.len,
                        */
                        &s->connectex_sent_bytes,
                        &s->overlapped_connectex);
    if (success) {
        /* We're doing an overlapped connect so this should never occur. */
        __debugbreak();
    } else {
        wsa_error = WSAGetLastError();
        switch (wsa_error) {
            case WSAEINVAL:
                if (s->overlapped_connectex.Internal != STATUS_PENDING)
                    __debugbreak();
                s->was_status_pending = 1;
                /* Intentional follow-on to WSA_IO_PENDING. */
            case WSA_IO_PENDING:
                /* Overlapped accept initiated successfully, a completion
                 * packet will be posted when a new client connects. */
                goto end;

            case WSAENOTSOCK:
                /*
                __debugbreak();
                PxSocket_UpdateConnectTime(s);
                */
            case WSAENETRESET:
            case WSAECONNREFUSED:
            case WSAENETUNREACH:
            case WSAEHOSTUNREACH:
                PxSocket_RECYCLE(s);
        }

        PxSocket_WSAERROR("ConnectEx");
    }

    ASSERT_UNREACHABLE();

overlapped_connectex_callback:
    ODS(L"overlapped_connectex_callback:\n");
    if (!s->was_connecting)
        __debugbreak();

    s->was_connecting = 0;
    if (c->io_result != NO_ERROR) {
        if (s->overlapped_connectex.Internal == NO_ERROR)
            __debugbreak();

        if (s->wsa_error == NO_ERROR) {
            switch (c->io_result) {
                case ERROR_CONNECTION_REFUSED:
                    s->wsa_error = WSAECONNREFUSED;
                    break;
                /* Haven't seen any of these in the wild, but putting in
                   some stub code anyway to make things easier down the
                   track. */
                case ERROR_NETWORK_UNREACHABLE:
                    __debugbreak();
                    //s->wsa_error = WSANETUNREACH;
                    break;
                case ERROR_HOST_UNREACHABLE:
                    __debugbreak();
                    //s->wsa_error = WSAHOSTUNREACH;
                case ERROR_CONNECTION_INVALID:
                    __debugbreak();
                    //s->wsa_error = WSANOTSOCK?
                    break;
                case ERROR_HOST_DOWN:
                    __debugbreak();
                    //s->wsa_error = WSANETDOWN?
                    break;
            }
        }

        if (s->wsa_error == NO_ERROR)
            /* If you hit this breakpoint, review the value of c->io_result
               and see if you need to add to the switch statement above. */
            __debugbreak();

        /* Disable this for client sockets... */
        /*
        switch (s->wsa_error) {
            case WSAENOTSOCK:
            case WSAENETRESET:
            case WSAECONNREFUSED:
            case WSAENETUNREACH:
            case WSAEHOSTUNREACH:
                PxSocket_RECYCLE(s);
        }
        */

        if (s->connectex_snapshot)
            PxContext_RollbackHeap(c, &s->connectex_snapshot);
        PxSocket_OVERLAPPED_ERROR("ConnectEx");
    }

    err = setsockopt(s->sock_fd,
                     SOL_SOCKET,
                     SO_UPDATE_CONNECT_CONTEXT,
                     NULL,
                     0);

    if (err == SOCKET_ERROR)
        PxSocket_WSAERROR("setsockopt(SO_UPDATE_CONNECT_CONTEXT)");

    s->num_bytes_just_sent = (DWORD)s->overlapped_connectex.InternalHigh;
    if (s->num_bytes_just_sent != s->connectex_sent_bytes) {
        /* s->connectex_sent_bytes seems to be 0... */
        if (s->connectex_sent_bytes != 0)
            __debugbreak();
    }

    if (s->connectex_snapshot)
        PxContext_RollbackHeap(c, &s->connectex_snapshot);

    if (s->num_bytes_just_sent == 0) {
        if (!s->initial_bytes.len)
            goto eof_received;
    } else
        s->total_bytes_sent += s->num_bytes_just_sent;

    ODS(L"connected:\n");

    s->was_connected = 1;

connected:
    goto start;


do_accept:
    ODS(L"do_accept:\n");
    //rbuf = NULL;
    if (!PxSocket_InitInitialBytes(s))
        PxSocket_FATAL();

    if (!PxSocket_InitNextBytes(s))
        PxSocket_FATAL();

    /*
     * It's handy knowing whether or not our protocol sends the first chunk of
     * data or whether we expect the client to send something first.  This
     * will be reflected in s->initial_bytes.len.  If that is greater than
     * zero, then we send first.  In which case, we set our AcceptEx() recv
     * buffer to 0, which means AcceptEx() will return as soon as a connection
     * has been established (without waiting for the other side to actually
     * send something).
     *
     * If the client sends something first, then we use a recv buffer.
     * AcceptEx() will return once the client has connected *and* sent
     * something.  If it connects and doesn't send anything, we won't waste
     * any time processing the connection.  To deal with clients that connect
     * and don't send anything, we periodically check socket connection time
     * in the PxSocketServer_Start() I/O loop.
     *
     * (Or at least we will when I implement that.  It's currently an xxx
     * todo.)
     */
    if (s->initial_bytes.len || s->initial_bytes_callable)
        s->acceptex_bufsize = 0;
    else
        s->acceptex_bufsize = (s->recvbuf_size - (s->acceptex_addr_len * 2));

    assert(s->acceptex_addr_len > 0);

    s->this_io_op = PxSocket_IO_ACCEPT;
    PxOverlapped_Reset(&s->overlapped_acceptex);
    StartThreadpoolIo(s->parent->tp_io);
    s->was_accepting = 1;
    success = AcceptEx(s->parent->sock_fd,
                       s->sock_fd,
                       s->rbuf->w.buf,
                       s->acceptex_bufsize,
                       s->acceptex_addr_len,
                       s->acceptex_addr_len,
                       &(s->acceptex_recv_bytes),
                       &(s->overlapped_acceptex));
    if (success) {
        /* AcceptEx() completed synchronously.  No completion packet will be
         * queued.  The number of received bytes will live in
         * s->acceptex_recv_bytes.
         */

        /* xxx: do accepts ever complete synchronously when we specify an
         * overlapped struct?  I don't think so. */
        __debugbreak();
    } else {
        wsa_error = WSAGetLastError();
        switch (wsa_error) {
            case WSAEINVAL:
                if (s->overlapped_acceptex.Internal != STATUS_PENDING)
                    __debugbreak();
                s->was_status_pending = 1;
                /* Intentional follow-on to WSA_IO_PENDING. */
            case WSA_IO_PENDING:
                /* Overlapped accept initiated successfully, a completion
                 * packet will be posted when a new client connects. */
                InterlockedIncrement(&s->parent->accepts_posted);
                goto end;

            case WSAENOTSOCK:
                /*
                __debugbreak();
                PxSocket_UpdateConnectTime(s);
                */
            case WSAENETRESET:
            case WSAECONNABORTED:
            case WSAECONNRESET:
                PxSocket_RECYCLE(s);
        }

        PxSocket_WSAERROR("AcceptEx");
    }

    ASSERT_UNREACHABLE();

overlapped_acceptex_callback:
    ODS(L"do_acceptex_callback:\n");
    if (!s->was_accepting)
        __debugbreak();
    InterlockedDecrement(&s->parent->accepts_posted);
    s->was_accepting = 0;
    if (c->io_result != NO_ERROR) {
#ifdef Py_DEBUG
        if (s->overlapped_acceptex.Internal == NO_ERROR)
            __debugbreak();

        if (s->wsa_error == NO_ERROR) {
            if (c->io_result != 64)
                __debugbreak();
        }
#endif

        if (c->io_result == 64)
            PxSocket_RECYCLE(s);

        if (c->io_result == WSA_OPERATION_ABORTED)
            PxSocket_RECYCLE(s);

        switch (s->wsa_error) {
            case WSAENETRESET:
            case WSAECONNABORTED:
            case WSAECONNRESET:
                PxSocket_RECYCLE(s);
        }
#ifdef Py_DEBUG
        PxSocket_OVERLAPPED_ERROR("AcceptEx");
#else
        PxSocket_RECYCLE(s);
#endif
    }

    s->num_bytes_just_received = (DWORD)s->overlapped_acceptex.InternalHigh;
    if (s->num_bytes_just_received == 0) {
        /* Treat receipt of zero bytes as an EOF if our protocol expects the
         * client to send something first.  (At the time of writing, I've run
         * into this pretty consistently when doing short `wrk` calls, i.e.
         * `wrk --latency -c 1 -t 1 -d 1 http://...) */
        if (!s->initial_bytes.len && !s->initial_bytes_callable)
            goto eof_received;
    }

    ODS(L"accepted:\n");

    /* Update the socket context, */
    err = setsockopt(s->sock_fd,
                     SOL_SOCKET,
                     SO_UPDATE_ACCEPT_CONTEXT,
                     (char *)&(s->parent->sock_fd),
                     sizeof(SOCKET));
    if (err == SOCKET_ERROR)
        PxSocket_WSAERROR("setsockopt(SO_UPDATE_ACCEPT_CONTEXT)");

    GetAcceptExSockaddrs(
        s->rbuf->w.buf,
        s->acceptex_bufsize,
        s->acceptex_addr_len,
        s->acceptex_addr_len,
        &local_addr,
        &local_addr_len,
        &remote_addr,
        &remote_addr_len
    );

    memcpy(&(s->local_addr), local_addr, local_addr_len);
    memcpy(&(s->remote_addr), remote_addr, remote_addr_len);

    InterlockedIncrement(&s->parent->clients_connected);
    s->was_connected = 1;
    SetEvent(s->parent->client_connected);

    goto start;

overlapped_disconnectex_callback:
    ODS(L"overlapped_disconnectex_callback:\n");
    if (!s->was_disconnecting)
        __debugbreak();
    /* Well... this seems like an easy one... just recycle the socket for now
     * without bothering to check for errors and whatnot. */
    if (s->parent)
        InterlockedDecrement(&s->parent->clients_disconnecting);
    PxSocket_RECYCLE(s);

start:
    ODS(L"start:\n");

    assert(s->protocol);

//maybe_shutdown_send_or_recv:
    if (!PxSocket_CAN_RECV(s)) {
        if (shutdown(s->sock_fd, SD_RECEIVE) == SOCKET_ERROR) {
            wsa_error = WSAGetLastError();
            switch (wsa_error) {
                case WSAECONNABORTED:
                case WSAECONNRESET:
                    /* In both of these cases, MSDN says "the application
                     * should close the socket as it is no longer usable".
                     */
                    PxSocket_RECYCLE(s);
            }
            PxSocket_WSAERROR("shutdown(SD_RECEIVE)");
        }
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_RECV_SHUTDOWN;
        if (s->recv_shutdown) {
            assert(0 == "xxx todo: recv_shutdown");
        }
    }

    if (PxSocket_SHUTDOWN_SEND(s)) {
        if (shutdown(s->sock_fd, SD_SEND) == SOCKET_ERROR) {
            wsa_error = WSAGetLastError();
            switch (wsa_error) {
                case WSAECONNABORTED:
                case WSAECONNRESET:
                    /* In both of these cases, MSDN says "the application
                     * should close the socket as it is no longer usable".
                     */
                    PxSocket_RECYCLE(s);
            }
            PxSocket_WSAERROR("shutdown(SD_SEND)");
        }
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_SEND_SHUTDOWN;
        if (s->send_shutdown) {
            assert(0 == "xxx todo: send_shutdown");
        }
    }

    if ((Px_SOCKFLAGS(s) & Px_SOCKFLAGS_RECV_SHUTDOWN) &&
        (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_SEND_SHUTDOWN))
        goto do_disconnect;

    /* client and server entry point */
    if (PxSocket_IS_CLIENT(s))
        goto maybe_do_connection_made;

    /* server (accept socket) entry point */
    assert(PxSocket_IS_SERVERCLIENT(s));
    assert(!(Px_SOCKFLAGS(s) & Px_SOCKFLAGS_SENDING_INITIAL_BYTES));

    /* This will have been initialized at the do_accept: stage above. */
    if (s->initial_bytes_to_send) {
        DWORD *len;

        assert(!snapshot);
        snapshot = PxContext_HeapSnapshot(c);
        if (!PxSocket_LoadInitialBytes(s)) {
            //PxContext_RollbackHeap(c, &snapshot);
            PxSocket_EXCEPTION();
        }

        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_SENDING_INITIAL_BYTES;

        w = &s->initial_bytes;
        len = &w->len;

        if (!PxSocket_NEW_SBUF(c, s, snapshot, len, w->buf, 0, &sbuf, 0)) {
            //PxContext_RollbackHeap(c, &snapshot);
            if (!PyErr_Occurred())
                PyErr_SetString(PyExc_ValueError,
                                "failed to extract sendable object from "
                                "initial_bytes_to_send");
            PxSocket_EXCEPTION();
        }

        goto do_send;
    }

    /* Intentional follow-on to maybe_do_connection_made. */
maybe_do_connection_made:
    ODS(L"maybe_do_connection_made:\n");

    assert(
        s->last_io_op == PxSocket_IO_ACCEPT ||
        s->last_io_op == PxSocket_IO_CONNECT
    );

    if (s->connection_made &&
       !(Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CALLED_CONNECTION_MADE))
        goto definitely_do_connection_made;

    goto try_recv;

definitely_do_connection_made:
    ODS(L"definitely_do_connection_made:\n");
    assert(!(Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CALLED_CONNECTION_MADE));
    func = s->connection_made;
    assert(func);

    snapshot = PxContext_HeapSnapshot(c);

    /* xxx todo: add peer argument */
    args = PyTuple_Pack(1, s);
    if (!args) {
        //PxContext_RollbackHeap(c, &snapshot);
        PxSocket_FATAL();
    }

    result = PyObject_CallObject(func, args);
    if (result)
        assert(!PyErr_Occurred());
    if (PyErr_Occurred())
        assert(!result);
    if (!result) {
        //PxContext_RollbackHeap(c, &snapshot);
        PxSocket_EXCEPTION();
    }

    Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_CALLED_CONNECTION_MADE;

    if (PxSocket_IS_SENDFILE_SCHEDULED(s)) {
        if (result != Py_None) {
            PyErr_SetString(PyExc_RuntimeError,
                            "data_received callback scheduled sendfile but "
                            "returned non-None data");
            PxSocket_EXCEPTION();
        }
    }

    if (result == Py_None) {
        if (PxSocket_IS_SENDFILE_SCHEDULED(s)) {
            s->sendfile_snapshot = snapshot;
            snapshot = NULL;
            goto do_sendfile;
        }
        PxContext_RollbackHeap(c, &snapshot);
        goto try_recv;
    }

    sbuf = NULL;
    if (!PxSocket_NEW_SBUF(c, s, snapshot, 0, 0, result, &sbuf, 0)) {
        //PxContext_RollbackHeap(c, &snapshot);
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_ValueError,
                            "connection_made() did not return a sendable "
                            "object (bytes, bytearray or unicode)");
        PxSocket_EXCEPTION();
    }

    if (PyErr_Occurred())
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_CLOSE_SCHEDULED;

    if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CLOSE_SCHEDULED)
        goto do_disconnect;

    /* Intentional follow-on to do_send. */

do_send:
    ODS(L"do_send:\n");
    assert(sbuf);

    s->this_io_op = PxSocket_IO_SEND;
    c->io_result = NO_ERROR;

    if (!s->in_overlapped_callback)
        goto do_async_send;

    if (PxSocket_THROUGHPUT(s) || PxSocket_LOW_LATENCY(s)) {
        n = s->max_sync_send_attempts;
        goto try_synchronous_send;
    }

    n = 1;
    if (PxSocket_IS_HOG(s) && _PxSocket_ActiveHogs >= _PyParallel_NumCPUs-1)
        goto do_async_send;
    else if (_PxSocket_ActiveIOLoops >= _PyParallel_NumCPUs)
        goto do_async_send;
    else if (PxSocket_CONCURRENCY(s))
        goto do_async_send;

try_synchronous_send:
    ODS(L"try_synchronous_send:\n");
    s->send_id++;
    /* Should have been set above. */
    if (s->this_io_op != PxSocket_IO_SEND)
        __debugbreak();

    if (c->io_result != NO_ERROR)
        __debugbreak();

    err = SOCKET_ERROR;
    wsa_error = NO_ERROR;
    for (i = 1; i <= n; i++) {
        s->num_bytes_just_sent = 0;
        err = WSASend(s->sock_fd,
                      &sbuf->w,
                      1,        /* number of buffers, currently always 1 */
                      &s->num_bytes_just_sent,
                      0,        /* flags */
                      NULL,     /* overlapped */
                      NULL);    /* completion routine */

        if (err == NO_ERROR)
            break;

        wsa_error = WSAGetLastError();
        /* Balk if we get anything other than EWOULDBLOCK. */
        if (wsa_error != WSAEWOULDBLOCK)
            break;

        if (s->num_bytes_just_sent > 0) {
            /* Some, but not all of the bytes were sent. */
            DWORD remaining = 0;
            char *buf = NULL;

            if (s->num_bytes_just_sent == sbuf->w.len)
                /* Shouldn't ever happen. */
                __debugbreak();

            remaining = sbuf->w.len - s->num_bytes_just_sent;
            if (remaining <= 0)
                __debugbreak();

            buf = (char *)(_Py_PTR_ADD(sbuf->w.buf, remaining));

            /* Adjust counters. */
            s->total_bytes_sent += s->num_bytes_just_sent;
            s->num_bytes_just_sent = 0;

            /* Adjust buffer. */
            sbuf->w.len = remaining;
            sbuf->w.buf = buf;
        }
        if (i < n) {
            YieldProcessor();
            continue;
        } else
            break;
    }

    if (err != SOCKET_ERROR) {
        /* Send completed synchronously. */

        /* Hmmm... what if num_bytes_just_sent doesn't equal w->len here?
         * That implies only some of the bytes were sent synchronously...
         * can that happen?  Will we need to do a resend dance?
         *
         * (I would think that... no, this shouldn't happen, but hey, when
         *  in doubt, assert!  Or __debugbreak() as the case may be.)
         *
         *  Update: now that we've fixed the loop above to properly deal with
         *  partial sends, s->num_bytes_just_sent should definitely match
         *  sbuf->w.len at this point.
         */
        if (s->num_bytes_just_sent != sbuf->w.len)
            __debugbreak();

        s->last_io_op = PxSocket_IO_SEND;
        s->this_io_op = 0;

        s->total_bytes_sent += s->num_bytes_just_sent;
        /* snapshot won't be set if we've just sent static "next bytes". */
        if (sbuf->snapshot) {
            snapshot = sbuf->snapshot;
            PxContext_RollbackHeap(c, &sbuf->snapshot);
            if (s->rbuf->snapshot == snapshot)
                s->rbuf->snapshot = NULL;
        }
        w = NULL;
        sbuf = NULL;
        snapshot = NULL;
        goto send_completed;
    } else if (wsa_error == WSAEWOULDBLOCK) {
        s->send_id--;
        goto do_async_send;
    } else {
        s->send_id--;
        PxContext_RollbackHeap(c, &sbuf->snapshot);
        w = NULL;
        sbuf = NULL;
        snapshot = NULL;
        switch (wsa_error) {
            case WSAENETRESET:
            case WSAECONNABORTED:
            case WSAECONNRESET:
                PxSocket_RECYCLE(s);
        }
        PxSocket_WSAERROR("WSASend(synchronous)");
    }

    ASSERT_UNREACHABLE();

do_async_send:
    ODS(L"do_async_send:\n");
    /* There's some unavoidable code duplication between do_send: above and
     * do_async_send: below.  If you change one, check to see if you need to
     * change the other. */
    assert(sbuf);
    w = &sbuf->w;

    s->send_id++;

    if (s->this_io_op != PxSocket_IO_SEND)
        __debugbreak();

    if (c->io_result != NO_ERROR)
        __debugbreak();

    //PxOverlapped_Reset(&sbuf->ol);
    PxOverlapped_Reset(&s->overlapped_wsasend);
    if (!PxSocket_UpdateConnectTime(s)) {
        err = WSAGetLastError();
        if (err == WSAENOTSOCK)
            PxSocket_RECYCLE(s);
        else
            __debugbreak();
    }
    if (s->connect_time == -1)
        __debugbreak();
    StartThreadpoolIo(s->tp_io);

    if (sbuf != s->sbuf) {
        if (sbuf != (SBUF *)s->rbuf)
            __debugbreak();
    }

    if (sbuf->w.len == 0)
        __debugbreak();

    if (!sbuf->w.buf || !sbuf->w.buf[0])
        __debugbreak();

    err = WSASend(s->sock_fd,
                  &sbuf->w,
                  1,    /* number of buffers, currently always 1 */
                  NULL, /* number of bytes sent, must be null because we're
                           specifying an overlapped structure */
                  0,    /* flags (MSG_DONTROUTE, MSG_OOB, MSG_PARTIAL) */
                  &s->overlapped_wsasend,
                  NULL); /* completion routine; presumably null as we're
                          * using threadpool I/O instead */

    if (err == NO_ERROR) {
        /* Send completed synchronously.  Completion packet will be queued. */
        goto end;
    } else {
        wsa_error = WSAGetLastError();
        if (wsa_error == WSA_IO_PENDING)
            /* Overlapped IO successfully initiated; completion packet will
             * eventually get queued (when the IO completes or an error
             * occurs). */
            goto end;

        /* The overlapped send attempt failed.  No completion packet will
         * ever be queued, so we need to take care of cleanup here. */
        s->send_id--;
        PxContext_RollbackHeap(c, &sbuf->snapshot);
        switch (wsa_error) {
            case WSAENETRESET:
            case WSAECONNABORTED:
            case WSAECONNRESET:
                PxSocket_RECYCLE(s);
        }
        PxSocket_WSAERROR("WSASend");
    }

    ASSERT_UNREACHABLE();

overlapped_send_callback:
    ODS(L"overlapped_send_callback:\n");
    /* Entry point for an overlapped send. */

    sbuf = s->sbuf ? s->sbuf : (SBUF *)s->rbuf;

    if (c->io_result != NO_ERROR) {
#ifdef Py_DEBUG
        if (s->overlapped_wsasend.Internal == NO_ERROR)
            __debugbreak();

        if (s->wsa_error == NO_ERROR)
            __debugbreak();
#endif

        s->send_id--;

        if (c->io_result == 64)
            PxSocket_RECYCLE(s);

        if (c->io_result == WSA_OPERATION_ABORTED)
            PxSocket_RECYCLE(s);

        switch (s->wsa_error) {
            case WSAENETRESET:
            case WSAECONNABORTED:
            case WSAECONNRESET:
                PxSocket_RECYCLE(s);
        }
#ifdef Py_DEBUG
        PxSocket_OVERLAPPED_ERROR("WSASend");
#else
        PxSocket_RECYCLE(s);
#endif
    }

    s->num_bytes_just_sent = (DWORD)s->overlapped_wsasend.InternalHigh;

    if (s->num_bytes_just_sent > 0) {
        if (s->num_bytes_just_sent != sbuf->w.len)
            /* Do we need to post multiple sends here? */
            __debugbreak();
    }

    snapshot = sbuf->snapshot;
    if (snapshot)
        PxContext_RollbackHeap(c, &sbuf->snapshot);

    if (s->rbuf->snapshot == snapshot)
        s->rbuf->snapshot = NULL;

    if (sbuf->snapshot)
        __debugbreak();

    if (s->rbuf->snapshot)
        __debugbreak();

    snapshot = NULL;

    /* 0 bytes sent = EOF */
    if (s->num_bytes_just_sent == 0)
        goto eof_received;

    s->total_bytes_sent += s->num_bytes_just_sent;

    /* Intentional follow-on to send_complete... */

send_completed:
    ODS(L"send_completed:\n");

    if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_SENDING_NEXT_BYTES)
        Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_SENDING_NEXT_BYTES;

    func = s->send_complete;
    if (!func)
        goto try_recv;

    snapshot = PxContext_HeapSnapshot(c);

    args = PyTuple_Pack(2, s, PyLong_FromSize_t(s->send_id));
    if (!args) {
        //PxContext_RollbackHeap(c, &snapshot);
        PxSocket_FATAL();
    }

    result = PyObject_CallObject(func, args);
    if (result)
        assert(!PyErr_Occurred());
    if (PyErr_Occurred())
        assert(!result);
    if (!result) {
        //PxContext_RollbackHeap(c, &snapshot);
        PxSocket_EXCEPTION();
    }

    if (PxSocket_IS_SENDFILE_SCHEDULED(s)) {
        if (result != Py_None) {
            PyErr_SetString(PyExc_RuntimeError,
                            "send_complete callback scheduled sendfile but "
                            "returned non-None data");
            PxSocket_EXCEPTION();
        }
    }

    if (result == Py_None) {
        if (PxSocket_IS_SENDFILE_SCHEDULED(s)) {
            s->sendfile_snapshot = snapshot;
            snapshot = NULL;
            goto do_sendfile;
        }
        PxContext_RollbackHeap(c, &snapshot);
        goto try_recv;
    }

    sbuf = NULL;
    if (!PxSocket_NEW_SBUF(c, s, snapshot, 0, 0, result, &sbuf, 0)) {
        //PxContext_RollbackHeap(c, &snapshot);
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_ValueError,
                            "send_complete() did not return a sendable "
                            "object (bytes, bytearray or unicode)");
        PxSocket_EXCEPTION();
    }

    if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_SENDING_INITIAL_BYTES) {
        if (s->connection_made) {
            char *msg = "protocol's connection_made() callback "        \
                        "may never be called (because send_complete() " \
                        "is sending more data on the back of the "      \
                        "successful sending of the initial_bytes)";
            PyErr_WarnEx(PyExc_RuntimeWarning, msg, 1);
        }
        Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_SENDING_INITIAL_BYTES;
    }

    if (!(Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CHECKED_DR_UNREACHABLE)) {
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_CHECKED_DR_UNREACHABLE;
        if (PxSocket_CAN_RECV(s)) {
            char *msg = "protocol has data_received|lines_received " \
                        "callback, but send_complete() is sending " \
                        "more data, so it may never be called";
            PyErr_WarnEx(PyExc_RuntimeWarning, msg, 1);
        }
    }

    if (!(PxSocket_IS_HOG(s))) {
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_HOG;
        InterlockedIncrement(&_PxSocket_ActiveHogs);
    }

    goto do_send;

//send_failed:
//    /* Temporarily disabled... nothing is goto'ing here at the moment. */
//    assert(0);
//    assert(wsa_error);
//    func = s->send_failed;
//    if (func) {
//        /* xxx todo */
//        assert(0);
//    }
//
//    syscall = "WSASend";
//
//    goto handle_error;

eof_received:
    /*
     * "EOF received" = we called WSARecv() against the socket, and it
     * returned successfully (i.e. no error was indicated), but the number of
     * received bytes was 0.
     *
     * This indicates a graceful disconnect by the client.
     *
     * (I don't particularly like the name "EOF received"; I'd prefer
     *  something like "graceful disconnect".  But it's what Twisted (and
     *  Tulip/asyncio) use, so eh.)
     */
    s->client_disconnected = 1;
do_disconnect:
    ODS(L"do_disconnect:\n");

    s->this_io_op = PxSocket_IO_DISCONNECT;

    /* Determine if we want to use the TF_REUSE_SOCKET flag. */
    if (s->client_disconnected && s->sock_type == SOCK_STREAM)
        s->reused_socket = 1;
    else
        /* Eh, let's try reusing it always if we're not a client. */
        s->reused_socket = 1;

    if (s->reused_socket) {
        //assert(PxSocket_IS_SERVERCLIENT(s));
        s->disconnectex_flags = TF_REUSE_SOCKET;
    }

    PxOverlapped_Reset(&s->overlapped_disconnectex);
    StartThreadpoolIo(s->tp_io);
    s->was_connected = 0;
    s->was_disconnecting = 1;
    success = DisconnectEx(s->sock_fd,
                           &s->overlapped_disconnectex,
                           s->disconnectex_flags,
                           0);      /* reserved */

    if (success) {
        /* We shouldn't ever hit this when doing an overlapped DisconnectEx. */
        __debugbreak();

    } else {
        wsa_error = WSAGetLastError();
        if (wsa_error == WSA_IO_PENDING) {
            /* Overlapped IO successfully initiated; completion packet will
             * eventually get queued (when the IO completes or an error
             * occurs). */
            if (s->parent)
                InterlockedIncrement(&s->parent->clients_disconnecting);
            goto end;
        }

        s->reused_socket = 0;

        switch (wsa_error) {
            case WSAENETRESET:
            case WSAECONNABORTED:
            case WSAECONNRESET:
            case WSAENOTCONN:
            case WSAESHUTDOWN:
            case WSAEHOSTDOWN:
            case WSAEHOSTUNREACH:
                PxSocket_RECYCLE(s);
        }
        //__debugbreak();
        PxSocket_WSAERROR("DisconnectEx");
    }

    ASSERT_UNREACHABLE();

disconnected:
    ODS(L"disconnected:\n");
//connection_closed:
    Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_CLOSE_SCHEDULED;
    Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_CONNECTED;
    Px_SOCKFLAGS(s) |=  Px_SOCKFLAGS_CLOSED;

    if (PyErr_Occurred())
        PxSocket_EXCEPTION();

    func = s->connection_closed;
    if (func) {
        /* xxx todo */
        assert(0);
    }

    if (PxSocket_IS_SERVERCLIENT(s)) {
        /* xxx todo */
        PxSocket_RECYCLE(s);
        //__debugbreak();
        //PxServerSocket_ClientClosed(s);
    }

    goto end;

try_recv:
    ODS(L"try_recv:\n");

    if ((Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CLOSE_SCHEDULED) ||
        (!(PxSocket_CAN_RECV(s))))
        goto do_disconnect;

    if (s->last_io_op == PxSocket_IO_ACCEPT) {
        /*
         * This code path will cover a newly connected client that's just sent
         * some data.
         */
        assert(!s->initial_bytes_to_send);
        goto process_data_received;
    }

do_recv:
    ODS(L"do_recv:\n");
    assert(!PxSocket_RECV_MORE(s));
    assert(PxSocket_CAN_RECV(s));

    assert(!rbuf);
    assert(!w);
    assert(!snapshot);

    rbuf = s->rbuf;
    if (rbuf->snapshot)
        __debugbreak();
    w = &rbuf->w;
    /* Reset our rbuf. */
    w->len = s->recvbuf_size;
    w->buf = (char *)rbuf->ob_sval;

    s->this_io_op = PxSocket_IO_RECV;
    c->io_result = NO_ERROR;

    if (PxSocket_IS_CLIENT(s) && s->recv_id == 0) {
        /* Fast-path for newly connected clients. */
        n = 1;
        goto try_synchronous_recv;
    }

    if (!s->in_overlapped_callback)
        goto do_async_recv;

    if (PxSocket_THROUGHPUT(s) || PxSocket_LOW_LATENCY(s)) {
        n = s->max_sync_recv_attempts;
        goto try_synchronous_recv;
    }

    n = 1;
    if (_PxSocket_ActiveIOLoops >= _PyParallel_NumCPUs-1)
        goto do_async_recv;
    else if (PxSocket_CONCURRENCY(s))
        goto do_async_recv;

try_synchronous_recv:
    ODS(L"try_synchronous_recv:\n");
    s->recv_id++;

    assert(recv_flags == 0);

    err = SOCKET_ERROR;
    wsa_error = NO_ERROR;
    s->num_bytes_just_received = 0;

    for (i = 1; i <= n; i++) {
        err = WSARecv(s->sock_fd,
                      w,
                      1,
                      &s->num_bytes_just_received,
                      &recv_flags,
                      0,
                      0);
        if (err == SOCKET_ERROR) {
            wsa_error = WSAGetLastError();
            if (wsa_error == WSAEWOULDBLOCK) {
                if (s->num_bytes_just_received > 0) {
                    /* WSAEWOULDBLOCK is returned even when we received
                     * data if the amount was less than our socket buffer.
                     * (At least, I've seen it do this.)
                     */
                    if (s->num_bytes_just_received > (DWORD)s->recvbuf_size)
                        /* Now *this* should never happen. */
                        __debugbreak();
                    else {
                        w = NULL;
                        rbuf = NULL;
                        goto process_data_received;
                    }
                }
                if (i < n) {
                    YieldProcessor();
                    continue;
                } else
                    break;
            } else if (wsa_error == WSA_IO_PENDING) {
                __debugbreak();
                break;
            }
        }
        else
            break;
    }

    if (err != SOCKET_ERROR) {
        /* Receive completed synchronously. */
        s->last_io_op = PxSocket_IO_RECV;
        s->this_io_op = 0;
        if (s->num_bytes_just_received == 0)
            goto eof_received;
        w = NULL;
        rbuf = NULL;
        goto process_data_received;
    } else if (wsa_error == WSAEWOULDBLOCK) {
        s->recv_id--;
        goto do_async_recv;
    } else {
        s->recv_id--;
        switch (wsa_error) {
            case WSAENETRESET:
            case WSAECONNABORTED:
            case WSAECONNRESET:
                PxSocket_RECYCLE(s);
        }
        PxSocket_WSAERROR("WSARecv");
    }

    ASSERT_UNREACHABLE();

do_async_recv:
    ODS(L"do_async_recv:\n");
    assert(rbuf);
    assert(w);

    s->recv_id++;
    StartThreadpoolIo(s->tp_io);
    //PxOverlapped_Reset(&rbuf->ol);
    PxOverlapped_Reset(&s->overlapped_wsarecv);

    err = WSARecv(fd, w, 1, 0, &recv_flags, &s->overlapped_wsarecv, NULL);
    if (err == NO_ERROR) {
        /* Recv completed synchronously.  Completion packet will be queued. */
        goto end;
    } else {
        wsa_error = WSAGetLastError();
        if (wsa_error == WSA_IO_PENDING)
            /* Overlapped IO successfully initiated; completion packet will be
             * queued once data is received or an error occurs. */
            goto end;

        /* Overlapped receive attempt failed.  No completion packet will be
         * queued, so we need to take care of cleanup here. */
        s->recv_id--;
        if (rbuf->snapshot)
            PxContext_RollbackHeap(c, &rbuf->snapshot);
        switch (wsa_error) {
            case WSAENETRESET:
            case WSAECONNABORTED:
            case WSAECONNRESET:
                PxSocket_RECYCLE(s);
        }
        PxSocket_WSAERROR("WSARecv");
    }

    ASSERT_UNREACHABLE();

overlapped_recv_callback:
    ODS(L"overlapped_recv_callback:\n");
    /* Entry point for an overlapped recv. */
    assert(!snapshot);
    rbuf = s->rbuf;

    if (c->io_result != NO_ERROR) {
#ifdef Py_DEBUG
        if (s->overlapped_wsarecv.Internal == NO_ERROR)
            __debugbreak();

        if (s->wsa_error == NO_ERROR && c->io_result != 64)
            __debugbreak();
#endif

        s->recv_id--;

        if (rbuf->snapshot)
            PxContext_RollbackHeap(c, &rbuf->snapshot);

        if (c->io_result == 64)
            PxSocket_RECYCLE(s);

        if (c->io_result == WSA_OPERATION_ABORTED)
            PxSocket_RECYCLE(s);

        switch (s->wsa_error) {
            case WSAENETRESET:
            case WSAECONNABORTED:
            case WSAECONNRESET:
                PxSocket_RECYCLE(s);
        }
#ifdef Py_DEBUG
        __debugbreak();
        PxSocket_OVERLAPPED_ERROR("WSARecv");
#else
        PxSocket_RECYCLE(s);
#endif
    }

    s->num_bytes_just_received = (DWORD)s->overlapped_wsarecv.InternalHigh;

    /* do_data_received_callback: expects rbuf to be clear. */
    rbuf = NULL;

    if (s->num_bytes_just_received == 0)
        goto eof_received;

    /* Intentional follow-on to process_data_received... */

process_data_received:
    ODS(L"process_data_received:\n");
    /*
     * So, this is the point where we need to check the data we've received
     * for the sole purpose of seeing if we need to a) receive more data, or
     * b) invoke the protocol's (data|line)_received callback with the data.
     *
     * The former situation will occur when receive filters have been set on
     * the protocol, such as 'lines_mode' (we keep recv'ing until we find a
     * linebreak) or one of the 'expect_*' filters (expect_command, expect_
     * regex etc).  Or any number of other filters that allow us to determine
     * within C code (i.e. within this IO loop) whether or not we've received
     * enough data (without the need to call back into Python).
     *
     * Now, with all that being said, none of that functionality is
     * implemented yet, so the code below simply unsets the 'receive more'
     * flag and continues on to 'do data received callback'.
     *
     * (Which is why the next two lines look pointless.)
     *
     * Actually... here is the place we could also put some pre-Python
     * protocol helpers, like an HTTP request parser.
     */
    Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_RECV_MORE;

    goto do_data_received_callback;

do_data_received_callback:
    ODS(L"do_data_received_callback:\n");

    assert(s->num_bytes_just_received > 0);
    s->total_bytes_received += s->num_bytes_just_received;

    assert(!rbuf);
    rbuf = s->rbuf;

    if (s->next_bytes_to_send) {
        /* We bypass the data_received() callback if next_bytes_to_send is
         * provided.  This basically means we're completely ignoring whatever
         * the other side has just sent us.  (Useful for baseline benchmarks
         * where you want to take the PyObject_CallObject overhead out of the
         * equation.) */
        DWORD *len;

        assert(!snapshot);

        if (s->next_bytes_callable)
            snapshot = PxContext_HeapSnapshot(c);

        if (!PxSocket_LoadNextBytes(s))
            PxSocket_EXCEPTION();

        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_SENDING_NEXT_BYTES;

        w = &s->next_bytes;
        len = &w->len;

        if (!s->next_bytes_callable) {
            /* Re-use the rbuf if we're sending static data. */
            sbuf = (SBUF *)rbuf;
            rbuf = NULL;
            s->sbuf = sbuf;
            sbuf->w.len = w->len;
            sbuf->w.buf = w->buf;
            goto do_send;
        }

        if (!PxSocket_NEW_SBUF(c, s, snapshot, len, w->buf, 0, &sbuf, 0)) {
            if (!PyErr_Occurred())
                PyErr_SetString(PyExc_ValueError,
                                "failed to extract sendable object from "
                                "next_bytes_to_send");
            PxSocket_EXCEPTION();
        }

        rbuf = NULL;
        goto do_send;
    }

    if (s->num_bytes_just_received < (DWORD)s->recvbuf_size) {
        if (rbuf->ob_sval[s->num_bytes_just_received] != 0) {
            PyBytesObject *bytes;
            size_t size;
            size_t trailing;
            bytes = R2B(rbuf);
            size = Py_SIZE(bytes);
            trailing = size - s->num_bytes_just_received;
            if (size > s->num_bytes_just_received)
                SecureZeroMemory(&bytes->ob_sval[s->num_bytes_just_received],
                                 trailing);
            else
                __debugbreak();
        }
    }

    if (rbuf->ob_sval[s->num_bytes_just_received] != 0)
        __debugbreak();

    if (PxSocket_LINES_MODE_ACTIVE(s))
        goto do_lines_received_callback;

    assert(!rbuf->snapshot);
    rbuf->snapshot = PxContext_HeapSnapshot(c);

    func = s->data_received;
    assert(func);

    /* For now, num_rbufs should only ever be 1. */
    assert(s->num_rbufs == 1);
    if (s->num_rbufs == 1) {
        PyObject *n;
        PyObject *o;
        PyTypeObject *tp = &PyBytes_Type;
        bytes = R2B(rbuf);
        o = (PyObject *)bytes;
        Py_PXFLAGS(bytes) = Py_PXFLAGS_MIMIC;
        n = init_object(c, o, tp, s->num_bytes_just_received);
        assert(n == o);
        assert(Py_SIZE(bytes) == s->num_bytes_just_received);
        args = PyTuple_Pack(2, s, o);
        if (!args) {
            //PxContext_RollbackHeap(c, &rbuf->snapshot);
            PxSocket_FATAL();
        }
    } else {
        XXX_IMPLEMENT_ME();
    }

    if (PyErr_Occurred())
        __debugbreak();

    result = PyObject_CallObject(func, args);
    if (result)
        assert(!PyErr_Occurred());
    if (PyErr_Occurred())
        assert(!result);
    if (!result) {
        //PxContext_RollbackHeap(c, &rbuf->snapshot);
        PxSocket_EXCEPTION();
    }

    if (PxSocket_IS_SENDFILE_SCHEDULED(s)) {
        if (result != Py_None) {
            PyErr_SetString(PyExc_RuntimeError,
                            "data_received callback scheduled sendfile but "
                            "returned non-None data");
            PxSocket_EXCEPTION();
        }
    }

    if (result == Py_None) {
        if (PxSocket_IS_SENDFILE_SCHEDULED(s)) {
            s->sendfile_snapshot = rbuf->snapshot;
            rbuf->snapshot = NULL;
            goto do_sendfile;
        }
        PxContext_RollbackHeap(c, &rbuf->snapshot);
        if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CLOSE_SCHEDULED)
            goto do_disconnect;
        /* Nothing to send, no close requested, so try recv again. */
        w = NULL;
        rbuf = NULL;
        snapshot = NULL;
        goto do_recv;
    }

    w = &rbuf->w;
    sbuf = (SBUF *)rbuf;
    if (Px_PTR(result) == Px_PTR(bytes)) {
        /* Special case for echo, we don't need to convert anything. */
        sbuf->w.len = s->num_bytes_just_received;
    } else {
        if (!PyObject2WSABUF(result, w)) {
            PyErr_SetString(PyExc_ValueError,
                            "data_received() did not return a sendable "
                            "object (bytes, bytearray or unicode)");
            PxSocket_EXCEPTION();
        }

        sbuf->w.len = w->len;
        sbuf->w.buf = w->buf;
    }

    w = NULL;
    rbuf = NULL;
    snapshot = NULL;
    goto do_send;

do_sendfile:
    ODS(L"do_sendfile:\n");

    s->send_id++;
    s->this_io_op = PxSocket_IO_SENDFILE;

    s->sendfile_flags = 0;
    if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CLOSE_SCHEDULED)
        s->sendfile_flags = TF_DISCONNECT | TF_REUSE_SOCKET;

    StartThreadpoolIo(s->tp_io);
    /*PxOverlapped_Reset(&s->overlapped_sendfile);*/
    success = TransmitFile(s->sock_fd,
                           s->sendfile_handle,
                           s->sendfile_num_bytes_to_send,
                           s->sendfile_bytes_per_send,
                           &(s->overlapped_sendfile),
                           &(s->sendfile_tfbuf),
                           s->sendfile_flags);

    if (success) {
        /* We shouldn't ever hit this as we're doing an overlapped
         * TransmitFile. */
        __debugbreak();

    } else {
        wsa_error = WSAGetLastError();
        if (wsa_error == WSA_IO_PENDING)
            /* Overlapped transmit file request successfully initiated;
             * completion packet will be queued once transmission completes
             * (or an error occurs). */
            goto end;

        /* Overlapped transmit file attempt failed.  No completion packet will
         * be queued, so we need to take care of cleanup ourselves. */
        if (s->sendfile_snapshot)
            PxContext_RollbackHeap(c, &s->sendfile_snapshot);

        CloseHandle(s->sendfile_handle);

        s->send_id--;

        switch (wsa_error) {
            case WSAENETRESET:
            case WSAECONNABORTED:
            case WSAECONNRESET:
                PxSocket_RECYCLE(s);
        }
        PxSocket_WSAERROR("TransmitFile");
    }

    ASSERT_UNREACHABLE();

overlapped_sendfile_callback:
    ODS(L"overlapped_sendfile_callback:\n");
    /* Entry point for an overlapped TransmitFile */

    CloseHandle(s->sendfile_handle);

    if (c->io_result != NO_ERROR) {
        if (s->overlapped_sendfile.Internal == NO_ERROR)
            __debugbreak();

        /* xxx todo: hitting this where io_result == 64 */
        if (s->sendfile_wsa_error == NO_ERROR) {
            if (c->io_result != 64)
                __debugbreak();
        }

        s->send_id--;

        if (s->sendfile_snapshot)
            PxContext_RollbackHeap(c, &s->sendfile_snapshot);

        if (c->io_result == 64)
            PxSocket_RECYCLE(s);

        if (c->io_result == WSA_OPERATION_ABORTED)
            PxSocket_RECYCLE(s);

        /* xxx todo: call send(file?)_failed() if applicable */
        switch (s->sendfile_wsa_error) {
            case WSAENETRESET:
            case WSAECONNABORTED:
            case WSAECONNRESET:
                PxSocket_RECYCLE(s);
                ASSERT_UNREACHABLE();
        }
        __debugbreak();
        PxSocket_OVERLAPPED_ERROR("TransmitFile");
    }

    if (s->sendfile_snapshot)
        PxContext_RollbackHeap(c, &s->sendfile_snapshot);

    s->total_bytes_sent += s->sendfile_nbytes;
    s->sendfile_nbytes = 0;
    s->sendfile_handle = 0;
    s->sendfile_offset = 0;
    s->sendfile_num_bytes_to_send = 0;
    PxOverlapped_Reset(&s->overlapped_sendfile);
    SecureZeroMemory(&s->sendfile_tfbuf, sizeof(TRANSMIT_FILE_BUFFERS));
    Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_SENDFILE_SCHEDULED;

    goto send_completed;

do_lines_received_callback:
    ODS(L"do_lines_received_callback:\n");
    func = s->lines_received;
    assert(func);

    /* Not yet implemented. */
    assert(0);

end:
    /* s may be null if PxSocket_RECYCLE() decided the socket didn't need
     * recycling... (Although I can't remember if s->wsa_error = NO_ERROR
     * was strictly necessary.) */
    if (s)
        s->wsa_error = NO_ERROR;
    ODS(L"end:\n");
    InterlockedDecrement(&_PxSocket_ActiveIOLoops);

    return;

}

PxSocketBuf *
new_pxsocketbuf(Context *c, size_t nbytes)
{
    size_t size;
    PxSocketBuf *sbuf;
    PyBytesObject *pbuf;

    size = nbytes + sizeof(PxSocketBuf);

    sbuf = (PxSocketBuf *)_PyHeap_Malloc(c, size, 0, 0);
    if (!sbuf)
        return NULL;

    pbuf = PxSocketBuf2PyBytesObject(sbuf);
    (void)init_object(c, (PyObject *)pbuf, &PyBytes_Type, nbytes);

    sbuf->signature = _PxSocketBufSignature;
    sbuf->w.len = (ULONG)nbytes;
    sbuf->w.buf = PyBytes_AsString((PyObject *)pbuf);
    return sbuf;
}

PxSocketBuf *
new_pxsocketbuf_from_bytes(Context *c, PyBytesObject *o)
{
    size_t size;
    Py_ssize_t nbytes;
    PxSocketBuf *sbuf;
    char *buf;

    if (PyBytes_AsStringAndSize((PyObject *)o, &buf, &nbytes) == -1)
        return NULL;

    size = sizeof(PxSocketBuf);

    sbuf = (PxSocketBuf *)_PyHeap_Malloc(c, size, 0, 0);
    if (!sbuf)
        return NULL;

    sbuf->w.len = (ULONG)nbytes;
    sbuf->w.buf = buf;
    return sbuf;
}

PxSocketBuf *
new_pxsocketbuf_from_unicode(Context *c, PyUnicodeObject *o)
{
    size_t size;
    Py_ssize_t nbytes;
    PxSocketBuf *sbuf;
    char *buf;

    assert(!PyErr_Occurred());
    buf = PyUnicode_AsUTF8AndSize((PyObject *)o, &nbytes);
    if (PyErr_Occurred())
        return NULL;

    size = sizeof(PxSocketBuf);

    sbuf = (PxSocketBuf *)_PyHeap_Malloc(c, size, 0, 0);
    if (!sbuf)
        return NULL;

    sbuf->w.len = (ULONG)nbytes;
    sbuf->w.buf = buf;
    return sbuf;
}

void
PxContext_CleanupThreadpool(Context *c)
{
    Context *x = ctx;
    return;
    __debugbreak();

    if (!c->tp_ctx)
        return;

    if (c->tp_ctx != c)
        return;

    CloseThreadpoolCleanupGroup(c->ptp_cg);
    DestroyThreadpoolEnvironment(c->ptp_cbe);
    CloseThreadpool(c->ptp);

    c->tp_ctx = NULL;
    c->ptp = NULL;
    c->ptp_cg = NULL;
    c->ptp_cgcb = NULL;
    c->ptp_cbe = NULL;
}

void
PxContext_CallbackComplete(Context *c)
{
    PxContext_CleanupThreadpool(c);
    if (c->io_obj)
        __debugbreak();
    if (PyErr_Occurred())
        __debugbreak();
    c->callback_completed->from = c;
    PxList_TimestampItem(c->callback_completed);
    PxList_Push(c->px->completed_callbacks, c->callback_completed);
    //InterlockedExchange(&c->done, 1);
    SetEvent(c->px->wakeup);
}

void
PxContext_ErrbackComplete(Context *c)
{
    PxContext_CleanupThreadpool(c);
    if (c->io_obj)
        __debugbreak();
    if (PyErr_Occurred())
        __debugbreak();
    c->errback_completed->from = c;
    PxList_TimestampItem(c->errback_completed);
    PxList_Push(c->px->completed_errbacks, c->errback_completed);
    //InterlockedExchange(&c->done, 1);
    SetEvent(c->px->wakeup);
}

void
PxSocket_CallbackComplete(PxSocket *s)
{
    Context *c = s->ctx;
    PxState *px = c->px;
    if (PyErr_Occurred())
        __debugbreak();
    if (Px_PTR(s) != Px_PTR(c->io_obj))
        __debugbreak();
    if (s->sock_fd != INVALID_SOCKET)
        closesocket(s->sock_fd);
    c->io_obj = NULL;
    //InterlockedDecrement(&px->contexts_active);
    //PxContext_CleanupThreadpool(c);
    PxContext_CallbackComplete(c);
    return;
    /*
    if (c->px_link.Flink) {
        EnterCriticalSection(&px->contexts_cs);
        RemoveEntryList(&c->px_link);
        LeaveCriticalSection(&px->contexts_cs);
    }
    */
    HeapDestroy(c->heap_handle);
    free(c);
}

void
PxSocket_ErrbackComplete(PxSocket *s)
{
    Context *c = s->ctx;
    if (PyErr_Occurred())
        __debugbreak();
    if (Px_PTR(s) != Px_PTR(c->io_obj))
        __debugbreak();
    if (s->sock_fd != INVALID_SOCKET)
        closesocket(s->sock_fd);
    c->io_obj = NULL;
    PxContext_CallbackComplete(c);
    if (s->sock_fd != INVALID_SOCKET) {
        closesocket(s->sock_fd);
        s->sock_fd = INVALID_SOCKET;
    }
    PxContext_ErrbackComplete(c);
}

void
PxSocket_HandleException(Context *c, const char *syscall, int fatal)
{
    PxSocket *s = (PxSocket *)c->io_obj;
    PyObject *exc, *args, *func, *result;
    PxState *px;
    PyThreadState *pstate;
    PxListItem *item;
    PxListHead *list;

    assert(PyErr_Occurred());

    pstate = c->pstate;
    px = c->px;

    if (fatal)
        goto error;

    READ_LOCK(s);
    func = s->exception_handler;
    READ_UNLOCK(s);

    if (!func)
        goto error;

    exc = PyTuple_Pack(3, pstate->curexc_type,
                          pstate->curexc_value,
                          pstate->curexc_traceback);
    if (!exc)
        goto error;

    PyErr_Clear();
    args = Py_BuildValue("(OsO)", s, syscall, exc);
    if (!args)
        goto error;

    result = PyObject_CallObject(func, args);
    if (null_with_exc_or_non_none_return_type(result, pstate))
        goto error;

    assert(!pstate->curexc_type);

    /* XXX TODO: ratify possible socket states. */
    goto end;

    list = px->completed_errbacks;
    item = c->errback_completed;
    goto done;

error:
    assert(pstate->curexc_type);
    list = px->errors;
    item = c->error;
    item->p1 = pstate->curexc_type;
    item->p2 = pstate->curexc_value;
    item->p3 = pstate->curexc_traceback;

    if (fatal) {
        if (s->sock_fd != INVALID_SOCKET) {
            closesocket(s->sock_fd);
            s->sock_fd = INVALID_SOCKET;
            /* closesocket() should obviate the need for a separate
             * Cancel/CloseThreadPoolIo() (I think). */
            /*
            if (s->tp_io) {
                //CancelThreadpoolIo(s->tp_io);
                CloseThreadpoolIo(s->tp_io);
                s->tp_io = NULL;
            }
            */
        }

        /*
        if (s->client_connected_tp_wait)
            CloseThreadpoolWait(s->client_connected_tp_wait);

        if (s->shutdown_tp_wait)
            CloseThreadpoolWait(s->shutdown_tp_wait);

        if (s->low_memory_tp_wait)
            CloseThreadpoolWait(s->low_memory_tp_wait);

        if (s->fd_accept_tp_wait)
            CloseThreadpoolWait(s->fd_accept_tp_wait);
         */

        if (s->fd_accept)
            CloseHandle(s->fd_accept);

        if (s->client_connected)
            CloseHandle(s->client_connected);

        if (s->low_memory)
            CloseHandle(s->low_memory);

        if (s->shutdown)
            CloseHandle(s->shutdown);

        if (s->high_memory)
            CloseHandle(s->high_memory);
    }

done:
    InterlockedExchange(&(c->done), 1);
    item->from = c;
    PxList_TimestampItem(item);
    PxList_Push(list, item);
    SetEvent(px->wakeup);
end:
    return;
}

static __inline
void
PxSocketServer_LinkChildren(PxSocket *s)
{
    PxListItem *item, *next;
    PxSocket *child;
    PLIST_ENTRY entry;

    /* Disable for now... */
    ASSERT_UNREACHABLE();

    EnterCriticalSection(&s->children_cs);
    CheckListEntry(&s->children);
    item = PxList_Flush(&s->link_child);
    while (item) {
        next = PxList_Next(item);
        child = CONTAINING_RECORD(item, PxSocket, link);
        entry = &child->child_entry;
        if (entry->Flink == NULL) {
            if (entry->Blink != NULL)
                __debugbreak();
            //__debugbreak();
            PxList_Push(&s->link_child, &child->link);
        } else {
            InsertTailList(&s->children, entry);
            ++s->num_children_entries;
        }
        item = next;
    }
    LeaveCriticalSection(&s->children_cs);
}

static __inline
void
PxSocketServer_UnlinkChildren(PxSocket *s)
{
    PxListItem *item, *next;
    PxSocket *child;

    item = PxList_Flush(&s->unlink_child);
    while (item) {
        next = PxList_Next(item);
        child = CONTAINING_RECORD(item, PxSocket, link);
        PxSocket_CallbackComplete(child);
        item = next;
    }
}

static __inline
void
PxSocketServer_UnlinkChildrenOld(PxSocket *s)
{
    PxListItem *item, *next;
    PxSocket *child;
    PLIST_ENTRY entry;

    EnterCriticalSection(&s->children_cs);
    item = PxList_Flush(&s->unlink_child);
    while (item) {
        next = PxList_Next(item);
        child = CONTAINING_RECORD(item, PxSocket, link);
        entry = &child->child_entry;
        if (entry->Flink != NULL) {
            if (entry->Blink == NULL)
                __debugbreak();
            //__debugbreak();
            PxList_Push(&s->unlink_child, &child->link);
        } else {
            RemoveEntryList(entry);
            --s->num_children_entries;
            PxSocket_CallbackComplete(child);
        }
        item = next;
    }
    LeaveCriticalSection(&s->children_cs);
}

static __inline
void
PxSocketServer_UpdateChildLinks(PxSocket *s)
{
    PxSocketServer_UnlinkChildren(s);
    PxSocketServer_LinkChildren(s);
}

void
pxsocket_dealloc(PxSocket *s)
{
    /* I think all of this needs to be ripped out... it was taken verbatim
     * from the socketmodule.c from memory.  I'm not even sure if it's being
     * hit currently. */
    __debugbreak();

    if (s->sock_fd != -1) {
        PyObject *exc, *val, *tb;
        Py_ssize_t old_refcount = Py_REFCNT(s);
        /* ++Py_REFCNT(self); */
        Py_INCREF(s);
        PyErr_Fetch(&exc, &val, &tb);
        if (PyErr_WarnFormat(PyExc_ResourceWarning, 1,
                             "unclosed %R", s))
            /* Spurious errors can appear at shutdown */
            if (PyErr_ExceptionMatches(PyExc_Warning))
                PyErr_WriteUnraisable((PyObject *) s);
        PyErr_Restore(exc, val, tb);
        closesocket(s->sock_fd);
        Py_REFCNT(s) = old_refcount;
    }
    if (s->ip)
        free(s->ip);

    if (s->heap_override)
        HeapDestroy(s->heap_override);

    Py_TYPE(s)->tp_free((PyObject *)s);
}

void
NTAPI
PxSocket_IOCallback(
    PTP_CALLBACK_INSTANCE instance,
    void *context,
    void *overlapped,
    ULONG io_result,
    ULONG_PTR nbytes,
    TP_IO *tp_io
);

PyObject *
create_pxsocket(
    PyObject *args,
    PyObject *kwds,
    ULONGLONG flags,
    PxSocket *parent,
    Context *use_this_context
)
{
    char *val;
    int len = sizeof(int);
    int nonblock = 1;
    PxSocket *s;
    SOCKET fd = INVALID_SOCKET;
    char *host;
    Context *c;
    Heap *old_heap = NULL;
    Py_ssize_t hostlen;
    int rbuf_size;
    RBUF *rbuf;

    PyTypeObject *tp = &PxSocket_Type;

    /* xxx: we could support pipelining here by creating a context for ncpu;
     * then when we're in the PxSocket_IOLoop after we've AcceptEx()'d a call,
     * we'd post ncpu overlapped WSARecv()s, binding each one to one of our
     * contexts. */

    /* First step is to create a new context object that'll encapsulate the
     * socket for its entire lifetime.  (xxx: `use_this_context` is a quick
     * hack; PxSocketServer_CreateClientSocket() needs to create a context as
     * soon as it can in order to pass it to _PyParallel_EnterCallback(),
     * which needs to be done first because it's the point we do per-thread
     * TLS initialization if needed.)
     *
     * (Actually, we use `use_this_context` for socket recycling as well.)
     */
    if (use_this_context)
        c = use_this_context;
    else {
        if (_PyParallel_HitHardMemoryLimit())
            return PyErr_NoMemory();
        c = new_context_for_socket(0);
        /* Ugh, really need to put all these PxState counters in one place.
         * (Or at the very least, document which ones you're meant to alter
         * when.) */
        InterlockedIncrement(&c->px->active);
    }

    if (!c)
        return NULL;

    if (c->io_obj) {
        /*
         * If we hit this code path, the context and socket have already been
         * created, but the socket was abnormally terminated for whatever
         * reason, so the underlying sock_fd needs to be discarded and a new
         * socket created.
         */
        PxSocket old;
        if (c->io_type != Px_IOTYPE_SOCKET)
            __debugbreak();

        s = (PxSocket *)c->io_obj;
        memcpy(&old, s, sizeof(PxSocket));

        /* Mimic the applicable invariants in PxSocket_Reuse()... */
        if (!s->startup_socket_snapshot)
            __debugbreak();

        if (!s->startup_heap_snapshot)
            __debugbreak();

        if (old.ctx != s->ctx)
            __debugbreak();

        if (old.ctx != s->startup_heap_snapshot->ctx)
            __debugbreak();

        if (old.ctx != s->rbuf->ctx)
            __debugbreak();

        if (s->rbuf->s != s)
            __debugbreak();

        /* Disable this for now... it's causing TppIopValidateIo() to fail. */
        /*
        if (s->tp_io) {
            CloseThreadpoolIo(s->tp_io);
            s->tp_io = NULL;
        }
        */
        if (s->sock_fd != INVALID_SOCKET) {
            int retval = closesocket(s->sock_fd);
            s->sock_fd = INVALID_SOCKET;
        }

        /* Copy the startup socket snapshot over the socket. */
        memcpy(s, s->startup_socket_snapshot, sizeof(PxSocket));

        /* Copy the state of the critical section back. */
        memcpy(&old.cs, &s->cs, sizeof(CRITICAL_SECTION));

        _PxContext_Rewind(s->ctx, s->startup_heap_snapshot);

        s->sock_fd = INVALID_SOCKET;
        s->tp_io = NULL;

        /* Clear sbuf.  The rbuf will be reset below. */
        s->sbuf = NULL;

        if (s->reused_socket)
            __debugbreak();

        if (s->reused)
            __debugbreak();

        /*
        s->reused = 0;
        s->reused_socket = 0;
        */
        s->recycled = 1;
        goto create_socket;

    }

    assert(!c->io_obj);

    c->io_type = Px_IOTYPE_SOCKET;
    s = (PxSocket *)_PyHeap_Malloc(c, sizeof(PxSocket), 0, 0);

    if (!s)
        return NULL;

    c->io_obj = (PyObject *)s;

    if (!init_object(c, c->io_obj, tp, 0))
        PxSocket_FATAL();

    _protect(c->io_obj);

    InitializeCriticalSectionAndSpinCount(&s->cs, CS_SOCK_SPINCOUNT);

    InitializeListHead(&s->children);
    assert(IsListEmpty(&s->children));

    s->ctx = c;

    s->flags = flags;

    if (parent) {
        assert(PxSocket_IS_SERVERCLIENT(s));

        /* Fast path for accept sockets; the initial bytes/protocol stuff will
         * have already been done by the listen socket, so we can just copy
         * what we need.
         */
        s->protocol_type = parent->protocol_type;
        s->protocol = parent->protocol;
        s->data_received = parent->data_received;
        s->lines_received = parent->lines_received;
        s->lines_mode = parent->lines_mode;
        s->exception_handler = parent->exception_handler;
        s->max_sync_send_attempts = parent->max_sync_send_attempts;
        s->max_sync_recv_attempts = parent->max_sync_recv_attempts;
        s->initial_bytes_to_send = parent->initial_bytes_to_send;
        s->initial_bytes_callable = parent->initial_bytes_callable;
        s->next_bytes_to_send = parent->next_bytes_to_send;
        s->next_bytes_callable = parent->next_bytes_callable;
        s->send_complete = parent->send_complete;
        s->connection_made = parent->connection_made;
        s->connection_closed = parent->connection_closed;

        /* xxx: eh, we should be able to just copy the exact value for flags.
         * (We can't, because it's being overloaded with different
         * applications currently, i.e. indicating current protocol state as
         * well as protocol flags.)
         */
        if (PxSocket_CONCURRENCY(parent))
            Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_CONCURRENCY;

        if (PxSocket_THROUGHPUT(parent))
            Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_THROUGHPUT;

        if (PxSocket_LOW_LATENCY(parent))
            Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_LOW_LATENCY;

        if (s->data_received || s->lines_received)
            Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_CAN_RECV;

        if (s->next_bytes_to_send)
            Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_CAN_RECV;

        if (Px_SOCKFLAGS(parent) & Px_SOCKFLAGS_INITIAL_BYTES_CALLABLE)
            Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_INITIAL_BYTES_CALLABLE;

        if (Px_SOCKFLAGS(parent) & Px_SOCKFLAGS_NEXT_BYTES_CALLABLE)
            Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_NEXT_BYTES_CALLABLE;

    } else {
        assert(
            ( PxSocket_IS_CLIENT(s) && !PxSocket_IS_SERVER(s)) ||
            (!PxSocket_IS_CLIENT(s) &&  PxSocket_IS_SERVER(s))
        );
    }

    s->sock_fd = fd;
    s->sock_timeout = -1.0;
    s->sock_family = AF_INET;
    s->sock_type = SOCK_STREAM;

    if (PxSocket_IS_SERVERCLIENT(s)) {
        assert(!PxSocket_IS_SERVER(s));
        assert(!PxSocket_IS_CLIENT(s));
        goto serverclient;
    }

    /* xxx todo: all of this Python-specific stuff should really be moved out
     * into a separate method, such that this method becomes a non-Python
     * specific implementation called PxSocket_Create() or something.
     */
    assert(args);
    //assert(kwds);

    if (!PyArg_ParseTupleAndKeywords(PxSocket_PARSE_ARGS))
        PxSocket_FATAL();

    if (s->sock_family != AF_INET) {
        PyErr_SetString(PyExc_ValueError, "family must be AF_INET");
        PxSocket_FATAL();
    }

    if (s->sock_type != SOCK_STREAM) {
        PyErr_SetString(PyExc_ValueError, "sock type must be SOCK_STREAM");
        PxSocket_FATAL();
    }

    if (s->port < 0 || s->port > 0xffff) {
        PyErr_SetString(PyExc_OverflowError, "socket: port must be 0-65535");
        PxSocket_FATAL();
    }

    if (host[0] >= '0' && host[0] <= '9') {
        int d1, d2, d3, d4;
        char ch;

        if (sscanf(host, "%d.%d.%d.%d%c", &d1, &d2, &d3, &d4, &ch) == 4 &&
            0 <= d1 && d1 <= 255 && 0 <= d2 && d2 <= 255 &&
            0 <= d3 && d3 <= 255 && 0 <= d4 && d4 <= 255)
        {
            struct sockaddr_in *sin;

            /* xxx todo: now that we've switched to having a context
             * encapsulate the socket, we should change these char[n]
             * arrays into pointers that are _PyHeap_Malloc'd with the
             * socket's context. */
            memset(&s->ip[0], 0, 16);
            strncpy(&s->ip[0], host, 15);
            assert(s->ip[15] == '\0');
            s->host = &(s->ip[0]);

            if (PxSocket_IS_CLIENT(s)) {
                sin = &(s->remote_addr.in);
                s->remote_addr_len = sizeof(struct sockaddr_in);
            } else if (PxSocket_IS_SERVER(s)) {
                sin = &(s->local_addr.in);
                s->local_addr_len = sizeof(struct sockaddr_in);
            } else
                assert(0);

            sin->sin_addr.s_addr = htonl(
                ((long)d1 << 24) | ((long)d2 << 16) |
                ((long)d3 << 8)  | ((long)d4 << 0)
            );

            sin->sin_family = AF_INET;
            sin->sin_port = htons((short)s->port);
        } else {
            PyErr_SetString(PyExc_ValueError, "invalid IPv4 address");
            PxSocket_FATAL();
        }
    } else {
        strncpy(s->host, host, hostlen);
        assert(!s->ip);
    }

serverclient:
    if (s->sock_fd != INVALID_SOCKET)
        goto setnonblock;

create_socket:
    s->sock_fd = socket(s->sock_family, s->sock_type, s->sock_proto);
    if (s->sock_fd == INVALID_SOCKET)
        PxSocket_WSAERROR("socket()");

setnonblock:
    fd = s->sock_fd;
    if (ioctlsocket(fd, FIONBIO, (ULONG *)&nonblock) == SOCKET_ERROR)
        PxSocket_WSAERROR("ioctlsocket(FIONBIO)");

    if (PxSocket_THROUGHPUT(s)) {
        size_t sizeof_rbuf = sizeof(RBUF);
        size_t aligned_sizeof_rbuf = Px_PTR_ALIGN(sizeof_rbuf);
        s->recvbuf_size = 65536 - aligned_sizeof_rbuf;
    } else if (PxSocket_IS_SERVERCLIENT(s) || PxSocket_CONCURRENCY(s)) {
        /* This is about 1152 bytes at the time of writing. */
        s->recvbuf_size = s->ctx->h->remaining - Px_PTR_ALIGN(sizeof(RBUF));
        /* If it gets below 512, break.  We want to keep the entire buffer
         * within the same page used by the heap. */
        if (s->recvbuf_size < 512)
            __debugbreak();
        //s->recvbuf_size = 512;
        s->sendbuf_size = 0;
    }

    if (!s->recvbuf_size) {
        val = (char *)&(s->recvbuf_size);
        if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, val, &len) == SOCKET_ERROR)
            PxSocket_WSAERROR("setsockopt(SO_RCVBUF)");
    } else {
        val = (char *)&(s->recvbuf_size);
        if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, val, len) == SOCKET_ERROR)
            PxSocket_WSAERROR("setsockopt(SO_RCVBUF)");
    }

    /* Every socket* gets a recv buf assigned to it.  This won't be transient;
     * i.e. it doesn't participate in the snapshot/rollback dance like the
     * send buffers do.
     *
     * [*] except listen sockets.
     */
    if (PxSocket_IS_SERVER(s))
        goto set_other_sockopts;

    if (!s->rbuf) {
        rbuf_size = s->recvbuf_size + Px_PTR_ALIGN(sizeof(RBUF));
        rbuf = (RBUF *)_PyHeap_Malloc(s->ctx, rbuf_size, 0, 0);
        if (!rbuf)
            PxSocket_FATAL();

        rbuf->s = s;
        rbuf->ctx = s->ctx;
        rbuf->w.len = s->recvbuf_size;
        rbuf->w.buf = (char *)&rbuf->ob_sval[0];
        s->num_rbufs = 1;
        s->rbuf = rbuf;
    } else {
        //PxOverlapped_Reset(&s->rbuf->ol);

        if (s->sbuf)
            __debugbreak();

        assert(!s->sbuf);

        PxSocket_ResetBuffers(s);
    }

    if (s->sbuf)
        __debugbreak();

    if (s->rbuf->w.len != s->recvbuf_size)
        __debugbreak();

    if (s->rbuf->w.buf != &s->rbuf->ob_sval[0])
        __debugbreak();

    /* Send buffers get allocated on demand via PxSocket_NEW_SBUF()
     * during the PxSocket_IOLoop(), so we don't explicitly reserve
     * space for one like we do for the receive buffer above. */

    /* xxx: temp experiment using 0-byte send buffers (now that we're doing
     * overlapped sends for everything). */
    s->sendbuf_size = 0;
    val = (char *)&(s->sendbuf_size);
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, val, &len) == SOCKET_ERROR)
        PxSocket_WSAERROR("setsockopt(SO_SNDBUF)");

set_other_sockopts:
    if (s->sock_type == SOCK_STREAM) {
        int i = 1;
        int err;

        if (!s->no_tcp_nodelay) {
            err = setsockopt(fd,
                             SOL_SOCKET,
                             TCP_NODELAY,
                             (char *)&i,
                             sizeof(int));
            if (err == SOCKET_ERROR)
                PxSocket_WSAERROR("setsockopt(TCP_NODELAY)");
        }

        if (PxSocket_IS_SERVER(s) && !s->no_exclusive_addr_use) {
            err = setsockopt(fd,
                             SOL_SOCKET,
                             SO_EXCLUSIVEADDRUSE,
                             (char *)&i,
                             sizeof(int));
            if (err == SOCKET_ERROR)
                PxSocket_WSAERROR("setsockopt(SO_EXCLUSIVEADDRUSE)");
        }
    }

    s->tp_io = CreateThreadpoolIo((HANDLE)s->sock_fd,
                                  PxSocket_IOCallback,
                                  s->ctx,
                                  c->ptp_cbe);
    if (!s->tp_io)
        PxSocket_SYSERROR("CreateThreadpoolIo(PyParallel_IOCallback)");

    if (s->recycled)
        goto done;

    if (PxSocket_IS_SERVERCLIENT(s))
        s->acceptex_addr_len = sizeof(SOCKADDR) + 16;

    s->parent = parent;
    if (!s->parent) {
        /* Inherit our parent from the active context if applicable. */
        if (Py_PXCTX() && ctx->io_obj && ctx->io_type == Px_IOTYPE_SOCKET)
            s->parent = (PxSocket *)ctx->io_obj;
    }

    if (s->parent) {
        /*
        if (s->child_entry.Flink != NULL)
            __debugbreak();
        if (s->child_entry.Blink != NULL)
            __debugbreak();
         */
        PxSocketServer_LinkChild(s);
    }


    if (!PxSocket_IS_SERVER(s)) {
        PxSocket *socket_snapshot;
        Heap     *heap_snapshot;

        /* I haven't thought about (read: implemented any support for) how
         * client socket (i.e. not accept sockets) re-use would work yet, so
         * let's just make sure we only hit this path if we're a server client
         * for now.
         */
        /*
        if (!PxSocket_IS_SERVERCLIENT(s))
            __debugbreak();
        */

        assert(!s->startup_socket_snapshot);
        assert(!s->startup_heap_snapshot);

        s->startup_socket_flags = s->flags;

        socket_snapshot = _PyHeap_Malloc(s->ctx, sizeof(PxSocket), 0, 0);
        if (!socket_snapshot)
            __debugbreak();

        heap_snapshot = _PyHeap_Malloc(s->ctx, sizeof(Heap), 0, 0);
        if (!heap_snapshot)
            __debugbreak();

        s->startup_socket_snapshot = socket_snapshot;
        s->startup_heap_snapshot   = heap_snapshot;

        memcpy(s->startup_socket_snapshot, s, sizeof(PxSocket));
        /* This next line is (at least at the time of writing) identical to
         * what `_PxContext_HeapSnapshot(c, s->startup_heap_snapshot)` would
         * perform.  I've used memcpy() directly here instead for two reasons:
         *      1. It's more obvious what's happening.
         *      2. It's identical to the above line where we copy the socket.
         */
        memcpy(s->startup_heap_snapshot, c->h, sizeof(Heap));
    }
done:
    return (PyObject *)s;

end:
    /* Ugh, this logic is not even remotely correct. */
    __debugbreak();
    assert(PyErr_Occurred());
    if (PyErr_Occurred()) {
        assert(s->sock_fd == INVALID_SOCKET);
        assert(!s->tp_io);
    }
    return NULL;
}

PyDoc_STRVAR(pxsocket_accept_doc, "x\n");
PyDoc_STRVAR(pxsocket_bind_doc, "x\n");
PyDoc_STRVAR(pxsocket_connect_doc, "x\n");
PyDoc_STRVAR(pxsocket_close_doc, "x\n");
PyDoc_STRVAR(pxsocket_listen_doc, "x\n");
PyDoc_STRVAR(pxsocket_write_doc, "XXX TODO\n");

PyObject *
pxsocket_accept(PxSocket *s, PyObject *args)
{
    Py_RETURN_NONE;
}

void
PxSocket_HandleCallback(
    Context *c,
    const char *name,
    const char *format,
    ...
)
{
    va_list va;
    PyObject *func, *args, *result;
    PxSocket *s = (PxSocket *)c->io_obj;
    PyObject *o = (PyObject *)s;
    PyObject *protocol = s->protocol;

    if (!strcmp(name, "connection_made"))
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_CALLED_CONNECTION_MADE;

    READ_LOCK(o);
    func = PxSocket_GET_ATTR(name);
    READ_UNLOCK(o);

    if (!func || func == Py_None)
        goto end;

    va_start(va, format);
    args = Py_VaBuildValue(format, va);
    va_end(va);

    /*
    if (!PxContext_Snapshot(c))
        PxSocket_EXCEPTION();
    */

    result = PyObject_CallObject(func, args);

    if (result)
        assert(!PyErr_Occurred());

    if (PyErr_Occurred())
        assert(!result);

    if (!result)
        PxSocket_EXCEPTION();

    if (result == Py_None)
        goto end;

end:
    return;
}

void
disabled_PxServerSocket_ClientClosed(PxSocket *o)
{
    Context  *x = o->ctx;
    PxSocket *s = o->parent;

    x->io_obj = NULL;

    if (PxSocket_IS_HOG(o)) {
        Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_HOG;
        InterlockedDecrement(&_PxSocket_ActiveHogs);
    }

    /* temp stats for chargen */
    /*
    {
        size_t lines, lps;
        double Bs, KBs, MBs;
        SOCKET fd = o->sock_fd;

        lines = o->total_bytes_sent / 73;

        if (o->connect_time <= 0) {
            printf("[%d/%d/%d] client sent %d bytes (%d lines)\n",
                   s->nchildren, o->child_id, fd, o->total_bytes_sent, lines);
        } else {
            Bs = (double)o->total_bytes_sent / o->connect_time;
            KBs = Bs / 1024.0;
            MBs = KBs / 1024.0;
            lines = o->total_bytes_sent / 73;
            lps = lines / o->connect_time;

            printf("[%d/%d/%d] client sent %d bytes total, connect time: "
                   "%d seconds, %.3fb/s, %.3fKB/s, %.3fMB/s, "
                   "lines: %d, lps: %d\n",
                   s->nchildren, o->child_id, fd, o->total_bytes_sent,
                   o->connect_time, Bs, KBs, MBs, lines, lps);
        }
    }
    */

    //PxSocket_CallbackComplete(o);

    o->ctx = NULL;

    if (!(Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CLEAN_DISCONNECT))
        o->sock_fd = -1;

    //InterlockedDecrement(&(s->nchildren));

    //SetEvent(s->more_accepts);
}

PxSocketBuf *
_try_extract_something_sendable_from_object(
    Context *c,
    PyObject *o,
    int depth)
{
    PxSocketBuf *b;

    if (depth > 10) {
        PyErr_SetString(PyExc_ValueError, "call depth exceeded trying to "
                                          "extract sendable object");
        return NULL;
    }

    if (!o)
        return NULL;

    if (PyBytes_Check(o)) {
        b = PyBytesObject2PxSocketBuf(o);
        if (!b)
            b = new_pxsocketbuf_from_bytes(c, (PyBytesObject *)o);

    } else if (PyUnicode_Check(o)) {
        b = new_pxsocketbuf_from_unicode(c, (PyUnicodeObject *)o);

    } else if (PyCallable_Check(o)) {
        PyObject *r = PyObject_CallObject(o, NULL);
        if (r)
            b = _try_extract_something_sendable_from_object(c, r, depth+1);
    } else {
        PyObject *s = PyObject_Str(o);
        if (s)
            b = _try_extract_something_sendable_from_object(c, s, depth);
    }

    if (!b)
        assert(PyErr_Occurred());

    return b;
}

PxSocketBuf *
_pxsocket_initial_bytes_to_send(Context *c, PxSocket *s)
{
    PyObject *i = PxSocket_GET_ATTR("initial_bytes_to_send");
    if (i == Py_None)
        return NULL;
    return _try_extract_something_sendable_from_object(c, i, 0);
}

/* These stubs are useful when debugging as they allow you to quickly see what
 * overlapped entry point a given thread/breakpoint/callstack is in (rather
 * than trying to trace back through s->*_io_op).
 */

void PxSocket_IOLoop_Connect(PxSocket *s) { PxSocket_IOLoop(s); }
void PxSocket_IOLoop_Accept(PxSocket *s) { PxSocket_IOLoop(s); }
void PxSocket_IOLoop_OverlappedConnectEx(PxSocket *s) { PxSocket_IOLoop(s); }
void PxSocket_IOLoop_OverlappedAcceptEx(PxSocket *s) { PxSocket_IOLoop(s); }
void PxSocket_IOLoop_OverlappedWSASend(PxSocket *s) { PxSocket_IOLoop(s); }
void PxSocket_IOLoop_OverlappedWSARecv(PxSocket *s) { PxSocket_IOLoop(s); }
void PxSocket_IOLoop_OverlappedTransmitFile(PxSocket *s) {PxSocket_IOLoop(s);}
void PxSocket_IOLoop_OverlappedDisconnectEx(PxSocket *s) {PxSocket_IOLoop(s);}

void
NTAPI
PxSocket_IOCallback(
    PTP_CALLBACK_INSTANCE instance,
    void *context,
    void *overlapped,
    ULONG io_result,
    ULONG_PTR nbytes,
    TP_IO *tp_io
)
{
    Context *c = (Context *)context;
    PxSocket *s = (PxSocket *)c->io_obj;
    OVERLAPPED *ol = (OVERLAPPED *)overlapped;

    if (_PyParallel_Finalized)
        return;

    /* AcceptEx() callbacks are a little quirky as the threadpool I/O is
     * actually submitted against the listen socket, not the accept socket.
     * Easy fix though; detect if we're a listen socket, then switch over to
     * the accept socket if so.
     */
    if (s->this_io_op == PxSocket_IO_LISTEN) {
        PxSocket *listen_socket = s;
        s = _Py_CAST_BACK(ol, PxSocket *, PxSocket, overlapped_acceptex);
        c = s->ctx;

        if (s->parent != listen_socket)
            __debugbreak();

        if (s->this_io_op != PxSocket_IO_ACCEPT)
            __debugbreak();
    }

    /* If we're shutting down, ignore everything and return immediately. */
    if (s->shutdown_count || (s->parent && s->parent->shutdown_count))
        return;

    if (s->parent && s->parent->shutting_down)
        return;

    ENTERED_IO_CALLBACK();

    PxSocket_StopwatchStart(s);

    if (s->break_on_iocp_enter)
        __debugbreak();

    /*
    if (!TryEnterCriticalSection(&(s->cs)))
        __debugbreak();
    */

    /* Would overlapped ever be null? */
    if (!ol)
        __debugbreak();
    else {
        /* And if not... would the Internal (status code) ever differ from
         * io_result, or InternalHigh (bytes transferred) ever differ from
         * nbytes?
         */
        if (io_result && ol->Internal != io_result) {
            /* NFI what the significance of this value is... but I've seen
             * it during debugging sessions. */
            DWORD nfi_internal = 0xC0000120;
            if (ol->Internal == nfi_internal) {
                if (io_result) {
                    /* Hrm, I saw instances of this where io_result was 64 and
                     * ol->Internal was some random big number.  Always during
                     * overlapped DisconnectEx() callbacks I believe.
                     */
                    if (io_result == 64)
                        ;
                    /* Also saw WSA_OPERATION_ABORTED... */
                    else if (io_result == WSA_OPERATION_ABORTED)
                        ;
                    else
                        __debugbreak();
                } else
                    __debugbreak();
            } else {
                /* Eh, this keeps getting tripped by 0xC0000xxxx values...
                 * let's stop __debugbreak()'ing. */
                /* (Note from future self: 0xC00nnnnn-type error codes are
                 *  system errors.) */
                //__debugbreak();
                ;
            }
        }
        else if (ol->InternalHigh != nbytes)
            __debugbreak();

        /*
         * Or would it ever not match the overlapped struct corresponding to
         * the current I/O action (i.e. what s->this_io_op indicates).
         */
        /* xxx todo */
    }


    /* Would tp_io ever be null? */
    if (!tp_io)
        __debugbreak();
    else {
        /* And if not... would tp_io ever not equal s->tp_io, or
         * s->parent->tp_io if s is a server socket and s->this_io_op ==
         * PxSocket_IO_ACCEPT.
         */

        switch (s->this_io_op) {
            case PxSocket_IO_ACCEPT:
                if (!s->parent)
                    __debugbreak();
                break;
            case PxSocket_IO_LISTEN:
                if (tp_io != s->tp_io)
                    __debugbreak();
        }
    }

    if (s->was_status_pending)
        __debugbreak();

    EnterCriticalSection(&(s->cs));

    if (io_result != NO_ERROR) {
        int failed;
        int errval;
        int errlen = sizeof(int);
        failed = getsockopt(s->sock_fd,
                            SOL_SOCKET,
                            SO_ERROR,
                            (char *)&errval,
                            &errlen);
        if (failed) {
            DWORD wsa_error;
            assert(failed == SOCKET_ERROR);
            wsa_error = WSAGetLastError();
            if (wsa_error != WSAENOTSOCK)
                __debugbreak();
            /* Eh, just set it to something that's not NO_ERROR. */
            s->wsa_error = SOCKET_ERROR;
        } else
            s->wsa_error = errval;
    } else
        s->wsa_error = NO_ERROR;

    /* Heh.  All this duplication just for some useful names when viewing
     * stack traces. */
    if (s->next_io_op) {
        switch (s->next_io_op) {
            case PxSocket_IO_ACCEPT:
                s->acceptex_wsa_error = s->wsa_error;
                PxSocket_IOLoop_Accept(s);
                break;

            case PxSocket_IO_CONNECT:
                s->connectex_wsa_error = s->wsa_error;
                PxSocket_IOLoop_Connect(s);
                break;

            default:
                ASSERT_UNREACHABLE();
        }
    } else {
        assert(s->this_io_op);
        switch (s->this_io_op) {
            case PxSocket_IO_ACCEPT:
                s->acceptex_wsa_error = s->wsa_error;
                PxSocket_IOLoop_OverlappedAcceptEx(s);
                break;

            case PxSocket_IO_CONNECT:
                s->connectex_wsa_error = s->wsa_error;
                PxSocket_IOLoop_OverlappedConnectEx(s);
                break;

            case PxSocket_IO_SEND:
                s->send_wsa_error = s->wsa_error;
                PxSocket_IOLoop_OverlappedWSASend(s);
                break;

            case PxSocket_IO_SENDFILE:
                s->sendfile_wsa_error = s->wsa_error;
                PxSocket_IOLoop_OverlappedTransmitFile(s);
                break;

            case PxSocket_IO_RECV:
                s->recv_wsa_error = s->wsa_error;
                PxSocket_IOLoop_OverlappedWSARecv(s);
                break;

            case PxSocket_IO_DISCONNECT:
                s->disconnectex_wsa_error = s->wsa_error;
                PxSocket_IOLoop_OverlappedDisconnectEx(s);
                break;

            default:
                ASSERT_UNREACHABLE();

        }
    }

    __try {
        if (s->ctx->io_obj == (PyObject *)s)
            LeaveCriticalSection(&s->cs);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        InterlockedIncrement(&_PyParallel_SEH_EAV_InIoCallback);
    }

end:
    return;
}

void
NTAPI
PyParallel_IOCallback(
    PTP_CALLBACK_INSTANCE instance,
    void *context,
    void *overlapped,
    ULONG io_result,
    ULONG_PTR nbytes,
    TP_IO *tp_io
)
{
    Context *c = (Context *)context;

    ENTERED_IO_CALLBACK();

    /* xxx todo: could just use function pointers for this stuff instead of
     * hard-coding the type checks. */
    switch (c->io_type) {
        case Px_IOTYPE_SOCKET: {
            PxSocket *s = (PxSocket *)c->io_obj;
            EnterCriticalSection(&(s->cs));
            PxSocket_IOLoop(s);
            LeaveCriticalSection(&(s->cs));
            break;
        }

        default:
            assert(0);
    }
}

/* objects */
/* xxx: current thinking re: send_failed/recv_failed etc: need to tighten up
 * the error handling logic.  Do we need individual (send|recv)_failed calls?
 * Such failures will typically be one of WSAENOTCONN, WSAEABORT, WSAENETRESET
 * and the like... may make sense just to bundle all these up and refer to
 * them via 'connection_lost', whilst 'connection_closed' is used to indicate
 * a clean connection close. */

/* (Also, are we sure we want to be using _Py_IDENTIFIER here?  Not sure if
 *  there are some issues with the Py_TLS static stuff and the relationship
 *  between our server socket stuff (where the protocol is initialized at the
 *  listen socket level, then cloned in an ad-hoc fashion (in create_pxsocket)
 *  to the accept socket.)
 */
_Py_IDENTIFIER(send_failed);
_Py_IDENTIFIER(recv_failed);
_Py_IDENTIFIER(send_shutdown);
_Py_IDENTIFIER(recv_shutdown);
_Py_IDENTIFIER(send_complete);
_Py_IDENTIFIER(data_received);
_Py_IDENTIFIER(lines_received);
_Py_IDENTIFIER(connection_made);
_Py_IDENTIFIER(connection_closed);
_Py_IDENTIFIER(exception_handler);
_Py_IDENTIFIER(initial_bytes_to_send);
_Py_IDENTIFIER(next_bytes_to_send);

/* bools */
_Py_IDENTIFIER(lines_mode);
_Py_IDENTIFIER(throughput);
_Py_IDENTIFIER(concurrency);
_Py_IDENTIFIER(low_latency);
_Py_IDENTIFIER(shutdown_send);

/* ints */
_Py_IDENTIFIER(max_sync_send_attempts);
_Py_IDENTIFIER(max_sync_recv_attempts);


/* 0 = failure, 1 = success */
int
PxSocket_InitProtocol(PxSocket *s)
{
    PyObject *p;
    assert(s->protocol_type);
    assert(!s->protocol);

    assert(!PyErr_Occurred());

    s->protocol = PyObject_CallObject(s->protocol_type, NULL);
    if (!s->protocol)
        return 0;

    p = s->protocol;

    if (!_protect(p))
        return 0;

    assert(!PyErr_Occurred());

#define _PxSocket_RESOLVE_OBJECT(name) do {             \
    PyObject *o = _PyObject_GetAttrId(p, &PyId_##name); \
    if (!o)                                             \
        PyErr_Clear();                                  \
    else if (!PyCallable_Check(o)) {                    \
        PyErr_SetString(                                \
            PyExc_ValueError,                           \
            "protocol attribute '" #name "' "           \
            "is not a callable object"                  \
        );                                              \
        return 0;                                       \
    }                                                   \
    s->##name = o;                                      \
} while (0)

#define _PxSocket_RESOLVE_BOOL(name) do {               \
    int b = 0;                                          \
    PyObject *o = _PyObject_GetAttrId(p, &PyId_##name); \
    if (!o)                                             \
        PyErr_Clear();                                  \
    else                                                \
        b = PyObject_IsTrue(o);                         \
    if (b)                                              \
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_##name;         \
    else                                                \
        Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_##name;        \
} while (0)

#define _PxSocket_RESOLVE_INT(name) do {                \
    int i = 0;                                          \
    PyObject *o = _PyObject_GetAttrId(p, &PyId_##name); \
    if (!o) {                                           \
        PyErr_Clear();                                  \
        Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_##name;        \
    } else {                                            \
        i = PyLong_AsLong(o);                           \
        if (PyErr_Occurred())                           \
            return 0;                                   \
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_##name;         \
        s->##name = i;                                  \
    }                                                   \
} while (0)

#define _PxSocket_RESOLVE(name) do {                    \
    PyObject *o = _PyObject_GetAttrId(p, &PyId_##name); \
    if (!o)                                             \
        PyErr_Clear();                                  \
    s->##name = o;                                      \
} while (0)


    _PxSocket_RESOLVE_OBJECT(lines_mode);
    _PxSocket_RESOLVE_OBJECT(send_failed);
    _PxSocket_RESOLVE_OBJECT(recv_failed);
    _PxSocket_RESOLVE_OBJECT(send_shutdown);
    _PxSocket_RESOLVE_OBJECT(recv_shutdown);
    _PxSocket_RESOLVE_OBJECT(send_complete);
    _PxSocket_RESOLVE_OBJECT(data_received);
    _PxSocket_RESOLVE_OBJECT(lines_received);
    _PxSocket_RESOLVE_OBJECT(connection_made);
    _PxSocket_RESOLVE_OBJECT(connection_closed);
    _PxSocket_RESOLVE_OBJECT(exception_handler);

    /* This is initialized in more detail during PxSocket_InitInitialBytes. */
    _PxSocket_RESOLVE(initial_bytes_to_send);
    /* This is initialized in more detail during PxSocket_InitNextBytes. */
    _PxSocket_RESOLVE(next_bytes_to_send);

    _PxSocket_RESOLVE_BOOL(throughput);
    _PxSocket_RESOLVE_BOOL(concurrency);
    _PxSocket_RESOLVE_BOOL(low_latency);
    _PxSocket_RESOLVE_BOOL(shutdown_send);

    _PxSocket_RESOLVE_INT(max_sync_send_attempts);
    _PxSocket_RESOLVE_INT(max_sync_recv_attempts);

    assert(!PyErr_Occurred());

    if (s->data_received || s->lines_received)
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_CAN_RECV;

    /* Having next_bytes_to_send set implies that we want to keep the
     * connection open, which currently requires us to toggle the CAN_RECV
     * flag. */
    if (s->next_bytes_to_send)
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_CAN_RECV;

    if (s->lines_mode && !s->lines_received) {
        PyErr_SetString(PyExc_ValueError,
                        "protocol has 'lines_mode' set to True but no "
                        "'lines_received' callback");
        return 0;
    }

    if (s->lines_received && !s->lines_mode) {
        PyErr_SetString(PyExc_ValueError,
                        "protocol has 'lines_received' callback but "
                        "no 'lines_mode' attribute");
        return 0;
    }

    assert(!PyErr_Occurred());

    if (s->lines_mode && PyObject_IsTrue(s->lines_mode))
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_LINES_MODE_ACTIVE;
    else
        Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_LINES_MODE_ACTIVE;

    if (PyErr_Occurred())
        return 0;

    if (PxSocket_CONCURRENCY(s) && PxSocket_THROUGHPUT(s)) {
        PyErr_SetString(PyExc_ValueError,
                        "protocol has both 'concurrency' and "
                        "'throughput' set to True; only one or "
                        "the other is permitted");
        return 0;
    }

    if (!PxSocket_THROUGHPUT(s) && !PxSocket_LOW_LATENCY(s)) {
        if (PxSocket_MAX_SYNC_SEND_ATTEMPTS(s)) {
            PyErr_SetString(PyExc_ValueError,
                            "protocol has 'max_sync_send_attempts' "
                            "set without 'throughput' or 'low_latency' "
                            "set to True");
            return 0;
        }
        if (PxSocket_MAX_SYNC_RECV_ATTEMPTS(s)) {
            PyErr_SetString(PyExc_ValueError,
                            "protocol has 'max_sync_recv_attempts' "
                            "set without 'throughput' or 'low_latency' "
                            "set to True");
            return 0;
        }
    } else {
        if (!PxSocket_MAX_SYNC_SEND_ATTEMPTS(s))
            s->max_sync_send_attempts = _PxSocket_MaxSyncSendAttempts;
        else {
            if (s->max_sync_send_attempts < 0) {
                PyErr_SetString(PyExc_ValueError,
                                "protocol has 'max_sync_send_attempts' "
                                "set to a value less than 0");
                return 0;
            } else if (s->max_sync_send_attempts == 0)
                s->max_sync_send_attempts = INT_MAX;
        }

        if (!PxSocket_MAX_SYNC_RECV_ATTEMPTS(s))
            s->max_sync_recv_attempts = _PxSocket_MaxSyncSendAttempts;
        else {
            if (s->max_sync_recv_attempts < 0) {
                PyErr_SetString(PyExc_ValueError,
                                "protocol has 'max_sync_recv_attempts' "
                                "set to a value less than 0");
                return 0;
            } else if (s->max_sync_recv_attempts == 0)
                s->max_sync_recv_attempts = INT_MAX;
        }
    }

    return 1;
}


/* 0 = failure, 1 = success */
int
PxSocket_SetProtocolType(PxSocket *s, PyObject *protocol_type)
{
    if (!protocol_type) {
        PyErr_SetString(PyExc_ValueError, "missing protocol value");
        return 0;
    }

    if (!PyType_CheckExact(protocol_type)) {
        PyErr_SetString(PyExc_ValueError, "protocol must be a class");
        return 0;
    }

    s->protocol_type = protocol_type;
    return PxSocket_InitProtocol(s);
}

#define INVALID_INITIAL_BYTES                                           \
    "initial_bytes_to_send must be one of the following types: bytes, " \
    "unicode or callable"

#define INVALID_NEXT_BYTES                                              \
    "next_bytes_to_send must be one of the following types: bytes, "    \
    "unicode or callable"

/* 0 = failure (error will be set), 1 = no error occurred */
int
PxSocket_InitInitialBytes(PxSocket *s)
{
    Context *c = s->ctx;
    PyObject *o, *t = s->protocol;
    int is_static = 0;
    Heap *snapshot = NULL;

    assert(t);
    assert(!PyErr_Occurred());

    o = s->initial_bytes_to_send;

    if (!o || o == Py_None)
        return 1;

    is_static = (
        PyBytes_Check(o)        ||
        PyByteArray_Check(o)    ||
        PyUnicode_Check(o)
    );

    if (is_static)
        Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_INITIAL_BYTES_CALLABLE;
    else if (PyCallable_Check(o))
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_INITIAL_BYTES_CALLABLE;
    else {
        PyErr_SetString(PyExc_ValueError, INVALID_INITIAL_BYTES);
        return 0;
    }

    if (PxSocket_IS_CLIENT(s)) {
        s->connectex_snapshot = PxContext_HeapSnapshot(c);
        snapshot = s->connectex_snapshot;
    } else
        snapshot = PxContext_HeapSnapshot(c);

    if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_INITIAL_BYTES_CALLABLE) {
        WSABUF w;
        PyObject *r;
        int error = 0;
        r = PyObject_CallObject(o, NULL);
        if (!r) {
            //PyErr_PrintEx(0);
            //PxContext_RollbackHeap(c, &snapshot);
            return 0;
        }
        if (!PyObject2WSABUF(r, &w)) {
            //PxContext_RollbackHeap(c, &snapshot);
            PyErr_SetString(PyExc_ValueError,
                            "initial_bytes_to_send() callable did not return "
                            "a sendable object (bytes, bytearray or unicode)");
            return 0;
        }
        s->initial_bytes_callable = o;
    } else {
        s->initial_bytes_callable = NULL;
        if (!PxSocket_IS_SERVERCLIENT(s)) {
            assert(!s->initial_bytes.buf);

            if (!PyObject2WSABUF(o, &s->initial_bytes)) {
                //PxContext_RollbackHeap(c, &snapshot);
                PyErr_SetString(PyExc_ValueError,
                                "initial_bytes_to_send is not a sendable "
                                "object (bytes, bytearray or unicode)");
                return 0;
            }

        } else {
            s->initial_bytes.len = s->parent->initial_bytes.len;
            s->initial_bytes.buf = s->parent->initial_bytes.buf;
        }

        assert(s->initial_bytes.buf);
    }

    /* Clients will be using the initial bytes during the ConnectEx() call, so
     * we need to keep the heap around. */
    if (!PxSocket_IS_CLIENT(s))
        PxContext_RollbackHeap(c, &snapshot);

    return 1;
}

/* 0 = failure (error will be set), 1 = no error occurred */
int
PxSocket_InitNextBytes(PxSocket *s)
{
    Context *c = s->ctx;
    PyObject *o, *t = s->protocol;
    int is_static = 0;
    Heap *snapshot = NULL;

    assert(t);
    assert(!PyErr_Occurred());

    o = s->next_bytes_to_send;

    if (!o || o == Py_None)
        return 1;

    is_static = (
        PyBytes_Check(o)        ||
        PyByteArray_Check(o)    ||
        PyUnicode_Check(o)
    );

    if (is_static)
        Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_NEXT_BYTES_CALLABLE;
    else if (PyCallable_Check(o))
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_NEXT_BYTES_CALLABLE;
    else {
        PyErr_SetString(PyExc_ValueError, INVALID_NEXT_BYTES);
        return 0;
    }

    if (PxSocket_IS_CLIENT(s)) {
        s->connectex_snapshot = PxContext_HeapSnapshot(c);
        snapshot = s->connectex_snapshot;
    } else
        snapshot = PxContext_HeapSnapshot(c);

    if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_NEXT_BYTES_CALLABLE) {
        WSABUF w;
        PyObject *r;
        int error = 0;
        r = PyObject_CallObject(o, NULL);
        if (!r) {
            //PyErr_PrintEx(0);
            //PxContext_RollbackHeap(c, &snapshot);
            return 0;
        }
        if (!PyObject2WSABUF(r, &w)) {
            //PxContext_RollbackHeap(c, &snapshot);
            PyErr_SetString(PyExc_ValueError,
                            "next_bytes_to_send() callable did not return "
                            "a sendable object (bytes, bytearray or unicode)");
            return 0;
        }
        s->next_bytes_callable = o;
    } else {
        s->next_bytes_callable = NULL;
        if (!PxSocket_IS_SERVERCLIENT(s)) {
            assert(!s->next_bytes.buf);

            if (!PyObject2WSABUF(o, &s->next_bytes)) {
                //PxContext_RollbackHeap(c, &snapshot);
                PyErr_SetString(PyExc_ValueError,
                                "next_bytes_to_send is not a sendable "
                                "object (bytes, bytearray or unicode)");
                return 0;
            }

        } else {
            s->next_bytes.len = s->parent->next_bytes.len;
            s->next_bytes.buf = s->parent->next_bytes.buf;
        }

        assert(s->next_bytes.buf);
    }

    /* Clients will be using the initial bytes during the ConnectEx() call, so
     * we need to keep the heap around. */
    /* xxx: do we need this logic for next bytes? */
    if (!PxSocket_IS_CLIENT(s))
        PxContext_RollbackHeap(c, &snapshot);

    return 1;
}


/* 0 = failure, 1 = success */
int
PxSocket_LoadInitialBytes(PxSocket *s)
{
    Context *c = s->ctx;
    PyObject *o, *r;

    if (!s->initial_bytes_callable)
        return 1;

    assert(Px_SOCKFLAGS(s) & Px_SOCKFLAGS_INITIAL_BYTES_CALLABLE);
    o = s->initial_bytes_callable;

    r = PyObject_CallObject(o, NULL);
    if (!r)
        return 0;

    if (!PyObject2WSABUF(r, &s->initial_bytes)) {
        PyErr_SetString(PyExc_ValueError,
                        "initial_bytes_to_send() callable did not return "
                        "a sendable object (bytes, bytearray or unicode)");
        return 0;
    }

    return 1;
}

/* 0 = failure, 1 = success */
int
PxSocket_LoadNextBytes(PxSocket *s)
{
    Context *c = s->ctx;
    PyObject *o, *r;

    if (!s->next_bytes_callable)
        return 1;

    assert(Px_SOCKFLAGS(s) & Px_SOCKFLAGS_NEXT_BYTES_CALLABLE);
    o = s->next_bytes_callable;

    r = PyObject_CallObject(o, NULL);
    if (!r)
        return 0;

    if (!PyObject2WSABUF(r, &s->next_bytes)) {
        PyErr_SetString(PyExc_ValueError,
                        "next_bytes_to_send() callable did not return "
                        "a sendable object (bytes, bytearray or unicode)");
        return 0;
    }

    return 1;
}


void
PxSocket_InitExceptionHandler(PxSocket *s)
{
    Heap *old_heap = NULL;
    Context *c = s->ctx;
    PyObject *eh;
    assert(s->protocol);
    assert(!PyErr_Occurred());
    if (!s->exception_handler) {
        assert(Py_PXCTX());
        old_heap = ctx->h;
        ctx->h = s->ctx->h;
        eh = PyObject_GetAttrString(s->protocol, "exception_handler");
        if (eh && eh != Py_None)
            s->exception_handler = eh;
        else
            PyErr_Clear();
    }
    if (old_heap)
        ctx->h = old_heap;
    assert(!PyErr_Occurred());
}

void
PxSocketServer_LinkChild(PxSocket *child)
{
    PxSocket *parent = child->parent;

    assert(child->parent);
    assert(child->parent != child);

    InterlockedIncrement(&parent->num_children);
    return;

    EnterCriticalSection(&parent->children_cs);
    InsertTailList(&parent->children, &child->child_entry);
    ++parent->num_children_entries;
    LeaveCriticalSection(&parent->children_cs);
    /*
    InterlockedIncrement(&parent->num_children);
    PxList_Push(&parent->link_child, &child->link);
    */
    /*
    if (!TryEnterCriticalSection(&parent->children_cs)) {
        PxList_Push(&parent->link_child, &child->link);
    } else {
        PxSocket *x;
        PLIST_ENTRY entry = &child->child_entry;
        x = CONTAINING_RECORD(entry, PxSocket, child_entry);
        if (x != child)
            __debugbreak();
        InsertTailList(&parent->children, &child->child_entry);
        LeaveCriticalSection(&parent->children_cs);
    }
    */
    return;
}

void
PxSocketServer_UnlinkChild(PxSocket *child)
{
    PxSocket *parent = child->parent;

    assert(child->parent);
    assert(child->parent != child);

    InterlockedDecrement(&parent->num_children);
    InterlockedIncrement(&parent->retired_clients);
    if (ReleaseSemaphore(&parent->accepts_sem, 1, NULL)) {
        InterlockedIncrement(&parent->sem_released);
        InterlockedIncrement(&parent->sem_count);
    } else {
        InterlockedIncrement(&parent->sem_release_err);
    }
    PxSocket_CallbackComplete(child);
    return;



    PxList_Push(&parent->unlink_child, &child->link);
    SetEvent(parent->free_children);
    return;


    EnterCriticalSection(&parent->children_cs);
    RemoveEntryList(&child->child_entry);
    --parent->num_children_entries;
    LeaveCriticalSection(&parent->children_cs);
    //PxSocket_CallbackComplete(child);

    PxList_Push(&parent->unlink_child, &child->link);

    /*
    if (!TryEnterCriticalSection(&parent->children_cs)) {
        PxList_Push(&parent->unlink_child, &child->link);
    } else {
        RemoveEntryList(&child->child_entry);
        LeaveCriticalSection(&parent->children_cs);
    }
    */
    return;
}

void
NTAPI
PxSocket_Connect(PTP_CALLBACK_INSTANCE instance, void *context)
{
    Context *c = (Context *)context;
    PxState *px;
    char failed = 0;
    struct sockaddr *sa;
    int len;
    DWORD result;
    char *buf = NULL;
    PxSocket *s = (PxSocket *)c->io_obj;
    PyTypeObject *tp = &PxSocket_Type;
    PxListItem *item;
    PxSocket *child;
    struct sockaddr_in *sin;

    ENTERED_CALLBACK();

    if (_PyParallel_HitHardMemoryLimit()) {
        PyErr_NoMemory();
        PxSocket_FATAL();
    }

    assert(s->protocol_type);
    assert(s->protocol);

    PxSocket_InitExceptionHandler(s);

    assert(PxSocket_IS_CLIENT(s));

    assert(s->protocol);
    assert(!PyErr_Occurred());

    s->shutdown = CreateEvent(0, 0, 0, 0);
    if (!s->shutdown)
        PxSocket_SYSERROR("CreateEvent(s->shutdown)");

    sin = &(s->local_addr.in);
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = INADDR_ANY;
    sin->sin_port = 0;
    if (bind(s->sock_fd, (struct sockaddr *)sin, sizeof(*sin)))
        PxSocket_WSAERROR("bind");

    px = c->px;
    c->io_type = Px_IOTYPE_SOCKET;
    s->next_io_op = PxSocket_IO_CONNECT;

    EnterCriticalSection(&s->cs);
    PxSocket_IOLoop(s);
    if (_PyParallel_Finalized)
        return;
    LeaveCriticalSection(&s->cs);
end:
    return;
}

void
CALLBACK
PxSocketServer_CreateClientSocket(
    PTP_CALLBACK_INSTANCE instance,
    PVOID context,
    PTP_WORK work
)
{
    Context  *c = (Context *)context;
    PxSocket *s = (PxSocket *)c->io_obj;

    PxSocket *o; /* client socket */
    Context  *x; /* client socket's context */
    int flags = Px_SOCKFLAGS_SERVERCLIENT;

    if (_PyParallel_Finalized)
        return;

    ctx = NULL;
    tmp_ctx = c;
    x = new_context(0);
    tmp_ctx = NULL;
    if (!x) {
        /* Can't do much else here. */
        InterlockedIncrement(&s->memory_failures);
        SetEvent(&s->low_memory);
        return;
    }

    _PyParallel_EnteredCallback(x, instance);

    o = (PxSocket *)create_pxsocket(NULL, NULL, flags, s, x);

    if (_PyParallel_Finalized)
        return;

    if (!o) {
        __debugbreak();
        XXX_IMPLEMENT_ME();
        return;
    }

    assert(PxSocket_IS_SERVERCLIENT(o));
    assert(s->protocol_type);
    assert(o->parent == s);

    assert(Px_PTR(x->io_obj) == Px_PTR(o));

    o->next_io_op = PxSocket_IO_ACCEPT;

    EnterCriticalSection(&(o->cs));
    PxSocket_IOLoop(o);
    if (_PyParallel_Finalized)
        return;
    LeaveCriticalSection(&(o->cs));
    return;
}

int
PxSocketServer_SlowlorisProtection(PxSocket *s, BOOL is_low_memory)
{
    int seconds;
    int bytes = sizeof(seconds);
    int result = 0;
    char *b = (char *)&seconds;
    int  *n = &bytes;
    int closed = 0;
    PLIST_ENTRY head;
    PLIST_ENTRY entry;
    DWORD err = NO_ERROR;
    SOCKET fd = INVALID_SOCKET;
    PxSocket *child;
    BOOL remove = FALSE;
    BOOL first_pass = TRUE;
    BOOL second_pass = FALSE;

    assert(s);
    assert(PxSocket_IS_SERVER(s));

    EnterCriticalSection(&s->children_cs);
    if (IsListEmpty(&s->children))
        goto end;
    head = &s->children;
    entry = head->Flink;
    do {
        child = CONTAINING_RECORD(entry, PxSocket, child_entry);
        assert(child);
        if (!TryEnterCriticalSection(&child->cs)) {
            entry = entry->Flink;
            continue;
        }
        if (child->total_bytes_received > 0) {
            if (child->recv_id <= 0)
                /* Broken invariant somewhere. */
                __debugbreak();
            entry = entry->Flink;
            LeaveCriticalSection(&child->cs);
            continue;
        }

        fd = child->sock_fd;
        err = getsockopt(fd, SOL_SOCKET, SO_CONNECT_TIME, b, n);
        if (err != NO_ERROR) {
            BOOL unexpected_wsa_error = TRUE;
            DWORD wsa_error = WSAGetLastError();
            switch (wsa_error) {
                case WSAENOTSOCK:
                case WSAENOTCONN:
                case WSAENETRESET:
                case WSAECONNRESET:
                case WSAECONNABORTED:
                case WSA_IO_PENDING:
                case WSA_OPERATION_ABORTED:
                    unexpected_wsa_error = FALSE;
            }
            if (unexpected_wsa_error)
                __debugbreak();
            else
                remove = TRUE;
        } else {
            child->connect_time = seconds;

            if (child->connect_time == -1) {
                ++s->negative_child_connect_time_count;
                remove = TRUE;
            } else if (child->connect_time >= s->slowloris_protection_seconds) {
                ++s->num_times_sloworis_protection_triggered;
                remove = TRUE;
            }

            if (!remove) {
                entry = entry->Flink;
                continue;
            }

            ++closed;
            RemoveEntryList(entry);
            entry = entry->Flink;
            PxSocket_CallbackComplete(child);
        }
    } while (entry != head);
end:
    LeaveCriticalSection(&s->children_cs);
    return closed;
}

void
CALLBACK
PxSocketServer_SlowlorisProtectionTimerCallback(
    _Inout_ PTP_CALLBACK_INSTANCE instance,
    _Inout_opt_ PVOID context,
    _Inout_ PTP_TIMER timer
)
{
    Context *c = (Context *)context;
    PxSocket *s = (PxSocket *)c->io_obj;
    PxSocketServer_SlowlorisProtection(s, FALSE);
}


void
PxSocketServer_LowMemory(PxSocket *s)
{
    assert(s);
    assert(PxSocket_IS_SERVER(s));
}

void
NTAPI
PxSocketServer_Start(PTP_CALLBACK_INSTANCE instance, void *context)
{
    Context *c = (Context *)context;
    PxState *px;
    char failed = 0;
    struct sockaddr *sa;
    int len;
    DWORD result;
    char *buf = NULL;
    PxSocket *s = (PxSocket *)c->io_obj;
    PyTypeObject *tp = &PxSocket_Type;
    int low_memory_wait = 0;
    PxListItem *item;
    PxSocket *child;
    int batched_accepts = 0;

    ENTERED_CALLBACK();

    if (_PyParallel_HitHardMemoryLimit()) {
        PyErr_NoMemory();
        PxSocket_FATAL();
    }

    assert(s->protocol_type);
    assert(s->protocol);

    PxSocket_InitExceptionHandler(s);

    assert(PxSocket_IS_SERVER(s));

    assert(s->protocol);
    assert(!PyErr_Occurred());

    if (!PxSocket_InitInitialBytes(s))
        PxSocket_FATAL();

    if (!PxSocket_InitNextBytes(s))
        PxSocket_FATAL();

    px = c->px;
    s->this_io_op = PxSocket_IO_LISTEN;

    s->client_connected = CreateEvent(0, 0, 0, 0);
    if (!s->client_connected)
        PxSocket_SYSERROR("CreateEvent(s->client_connected)");

    s->free_children = CreateEvent(0, 0, 0, 0);
    if (!s->free_children)
        PxSocket_SYSERROR("CreateEvent(s->free_children)");

    s->preallocate_children_tp_work = (
        CreateThreadpoolWork(PxSocketServer_CreateClientSocket,
                             context,
                             c->ptp_cbe)
    );

    if (!s->preallocate_children_tp_work)
        PxSocket_SYSERROR("CreateThreadpoolWork(preallocate_children_tp_work)");

    /*
    s->slowloris_protection_tp_timer = (
        CreateThreadpoolTimer(PxSocketServer_SlowlorisProtectionTimerCallback,
                              context,
                              c->ptp_cbe)
    );

    if (!s->slowloris_protection_tp_timer)
        PxSocket_SYSERROR("CreateThreadpoolTimer(slowloris_protection)");
    */

    InitializeCriticalSectionAndSpinCount(&s->children_cs, 12);

    /*
    s->client_connected_tp_wait = CreateThreadpoolWait(
        PxSocketServer_ClientConnectedWaitCallback,
        s->ctx,
        NULL
    );

    if (!s->client_connected_tp_wait)
        PxSocket_SYSERROR("CreateThreadpoolWait("
                          "PxSocketServer_ClientConnectedWaitCallback)");
    */

    s->shutdown = CreateEvent(0, 0, 0, 0);
    if (!s->shutdown)
        PxSocket_SYSERROR("CreateEvent(s->shutdown)");

    s->low_memory = CreateMemoryResourceNotification(
        LowMemoryResourceNotification
    );
    if (!s->low_memory)
        PxSocket_SYSERROR("CreateMemoryResourceNotification(LowMemory)");

    s->high_memory = CreateMemoryResourceNotification(
        HighMemoryResourceNotification
    );
    if (!s->high_memory)
        PxSocket_SYSERROR("CreateMemoryResourceNotification(HighMemory)");

    s->fd_accept = WSACreateEvent();
    if (!s->fd_accept)
        PxSocket_WSAERROR("WSACreateEvent(s->fd_accept)");

    s->wait_handles[0] = s->fd_accept;
    s->wait_handles[1] = s->client_connected;
    s->wait_handles[2] = s->free_children;
    s->wait_handles[3] = s->low_memory;
    s->wait_handles[4] = s->shutdown;
    s->wait_handles[5] = s->high_memory;

    if (s->target_accepts_posted <= 0)
        s->target_accepts_posted = _PyParallel_NumCPUs * 4;
        //s->target_accepts_posted = 1;

    s->accepts_sem = CreateSemaphore(
        NULL,                           /* semaphore attributes */
        s->target_accepts_posted,       /* initial count        */
        s->target_accepts_posted * 2,   /* maximum count        */
        NULL                            /* name (opt)           */
    );
    if (!s->accepts_sem)
        PxSocket_SYSERROR("CreateSemaphore");


//do_bind:
    /* xxx todo: handle ADDRINUSE etc. */
    sa = (struct sockaddr *)&(s->local_addr.in);
    len = s->local_addr_len;
    if (bind(s->sock_fd, sa, len) == SOCKET_ERROR)
        PxSocket_WSAERROR("bind");

//do_listen:
    if (listen(s->sock_fd, SOMAXCONN) == SOCKET_ERROR)
        PxSocket_WSAERROR("listen");

    if (WSAEventSelect(s->sock_fd, s->fd_accept, FD_ACCEPT) == SOCKET_ERROR)
        PxSocket_WSAERROR("WSAEventSelect(FD_ACCEPT)");

post_accepts:
    s->num_accepts_to_post = s->target_accepts_posted;
    while (--s->num_accepts_to_post) {
        ++s->total_accepts_attempted;
        SubmitThreadpoolWork(s->preallocate_children_tp_work);
    }
    goto wait;

    /* Old approach. */

    s->num_accepts_to_post = (
        s->target_accepts_posted -
        s->accepts_posted
    );
    if (s->num_accepts_to_post < 0) {
        ++s->negative_accepts_to_post_count;
        goto update_children;
    } else if (!s->num_accepts_to_post)
        goto update_children;

    /* Now that we're using explicit threadpool groups per parent/server,
     * which we set min/max threads on, we could review the whole "dispatch
     * accept posting to thread pool" (commented out below) versus the inline
     * creation version we ended up with.  There will be a threshold where
     * posting the accepts in parallel will be more efficient... */
    /*
    while (num_accepts_to_post--) {
        s->total_accepts_attempted++;
        if (!TrySubmitThreadpoolCallback(work_cb, c, NULL))
            PxSocket_SYSERROR("TrySubmitThreadpoolCallback("
                              "PxSocketServer_CreateClientSocket)");
    }
    */
    do {
        item = PxList_Pop(&s->unlink_child);
        if (item) {
            ++s->recycled_unlinked_child;
            child = CONTAINING_RECORD(item, PxSocket, link);
            if (!child)
                __debugbreak();
            PxSocket_Recycle(&child, /* force recycle */ TRUE);
        } else {
            /* No free sockets/contexts available, create a new one. */
            ++s->total_accepts_attempted;
            SubmitThreadpoolWork(s->preallocate_children_tp_work);

            //PxSocketServer_CreateClientSocket(NULL, c);
            /* The call above will alter ctx (the TLS variable), so make
             * sure we reset it straight after it returns. */
            //ctx = c;
            //if (PyErr_Occurred())
            //    PxSocket_EXCEPTION();
        }
    } while (--s->num_accepts_to_post);

update_children:
    //PxSocketServer_LinkChildren(s);
    //PxSocketServer_UnlinkChildren(s);

wait:
    if (_PyParallel_Finalized) {
        s->shutting_down = TRUE;
        goto shutdown;
    }

    if (low_memory_wait)
        /* Only wait on shutdown and high memory events. */
        /* (Note the differences in first arg (2/4) and handles (4/0).) */
        result = WaitForMultipleObjects(3, &(s->wait_handles[4]), 0, 0);
    else
        /* Wait on everything except high memory. */
        result = WaitForMultipleObjects(5, &(s->wait_handles[0]), 0, 1000);

    if (_PyParallel_Finalized) {
        s->shutting_down = TRUE;
        goto shutdown;
    }

    switch (result) {
        case WAIT_OBJECT_0:
            /* fd_accept */
            ++s->fd_accept_count;
            batched_accepts = 0;
            while (s->accepts_posted < s->target_accepts_posted) {
                ++s->total_accepts_attempted;
                SubmitThreadpoolWork(s->preallocate_children_tp_work);
                if (++batched_accepts == s->target_accepts_posted)
                    break;
            }
            ResetEvent(s->fd_accept);
            goto wait;

        case WAIT_OBJECT_0 + 1:
            /* client_connected */
            ++s->client_connected_count;
            if (WaitForSingleObject(s->accepts_sem, 0) == WAIT_OBJECT_0) {
                ++s->total_accepts_attempted;
                InterlockedIncrement(&s->sem_acquired);
                InterlockedDecrement(&s->sem_count);
                SubmitThreadpoolWork(s->preallocate_children_tp_work);
            } else {
                InterlockedIncrement(&s->sem_timeout);
            }

            goto wait;

        case WAIT_OBJECT_0 + 2:
            /* free_children */
            PxSocketServer_UnlinkChildren(s);
            goto wait;

        case WAIT_OBJECT_0 + 3:
            /* low memory */
            ++s->low_memory_count;
            s->is_low_memory = TRUE;
            low_memory_wait = 1;
            goto low_memory;

        case WAIT_OBJECT_0 + 4:
            /* shutdown event */
            InterlockedIncrement(&s->shutdown_count);
            s->shutting_down = TRUE;
            goto shutdown;

        case WAIT_OBJECT_0 + 5:
            /* high memory */
            ++s->high_memory_count;
            s->is_low_memory = FALSE;
            low_memory_wait = 0;
            goto wait;

        case WAIT_TIMEOUT:
            ++s->wait_timeout_count;
            //PxSocketServer_LinkChildren(s);
            //PxSocketServer_UnlinkChildren(s);
            if (_PyParallel_Finalized) {
                s->shutting_down = TRUE;
                goto shutdown;
            }
            if (s->wait_timeout_count > 5) {
                batched_accepts = 0;
                while (s->accepts_posted < s->target_accepts_posted) {
                    ++s->total_accepts_attempted;
                    SubmitThreadpoolWork(s->preallocate_children_tp_work);
                    if (++batched_accepts == s->target_accepts_posted)
                        break;
                }
            }
            //PxSocketServer_SlowlorisProtection(s, FALSE);
            goto timeout;

        case WAIT_ABANDONED_0:
        case WAIT_ABANDONED_0 + 1:
        case WAIT_ABANDONED_0 + 2:
        case WAIT_ABANDONED_0 + 3:
        case WAIT_ABANDONED_0 + 4:
        case WAIT_ABANDONED_0 + 5:
            goto shutdown;

        case WAIT_FAILED:
            /* Saw this happen as soon as low memory condition hit. */
            if (WaitForSingleObject(&s->wait_handles[3], 0) == WAIT_OBJECT_0) {
                low_memory_wait = 1;
                goto wait;
            }
            goto shutdown;

        default:
            assert(0);
    }

shutdown:
    /* Close our listen socket so we stop accepting new connections. */
    //goto cleanup_threadpool;

    //PxSocketServer_LinkChildren(s);
    //PxSocketServer_UnlinkChildren(s);

#if 0
    /* Walk all our sockets and close them... */
    if (!TryEnterCriticalSection(&s->children_cs))
        /* Hmmm... there shouldn't be any contention here. */
        __debugbreak();

    if (IsListEmpty(&s->children))
        goto cleaned_up_children;

    do {
        if (IsListEmpty(&s->children))
            break;
        entry = RemoveHeadList(&s->children);
        child = CONTAINING_RECORD(entry, PxSocket, child_entry);
        EnterCriticalSection(&child->cs);
        PxSocket_CallbackComplete(child);
        InterlockedDecrement(&s->num_children);
    } while (!IsListEmpty(&s->children));

cleaned_up_children:
    LeaveCriticalSection(&s->children_cs);
    if (s->num_children != 0)
        __debugbreak();
#endif


    if (s->preallocate_children_tp_work)
        CloseThreadpoolWork(s->preallocate_children_tp_work);

    if (s->slowloris_protection_tp_timer)
        CloseThreadpoolTimer(s->slowloris_protection_tp_timer);

    if (s->fd_accept)
        CloseHandle(s->fd_accept);

    if (s->client_connected)
        CloseHandle(s->client_connected);

    if (s->free_children)
        CloseHandle(s->free_children);

    if (s->low_memory)
        CloseHandle(s->low_memory);

    if (s->shutdown)
        CloseHandle(s->shutdown);

    if (s->high_memory)
        CloseHandle(s->high_memory);

    if (s->accepts_sem)
        CloseHandle(s->accepts_sem);

cleanup_threadpool:
    CloseThreadpoolCleanupGroupMembers(c->ptp_cg,
                                       FALSE, /* cancel pending callbacks */
                                       s);   /* I'm not sure if we need to
                                                pass 's' here... */

    PxSocket_CallbackComplete(s);
    return;

    InterlockedDecrement(&px->contexts_active);
    HeapDestroy(c->heap_handle);
    free(c);
    return;

    PxSocket_CallbackComplete(s);


    CancelThreadpoolIo(s->tp_io);
    closesocket(s->sock_fd);
    s->sock_fd = INVALID_SOCKET;

    /* (Do we need to check for anything else here?) */
    goto end;


low_memory:
    PxSocketServer_LowMemory(s);
    goto wait;

timeout:
    /* Another xxx todo... close idle sockets... */
    goto wait;

end:
    if (s->sock_fd != INVALID_SOCKET) {
        closesocket(s->sock_fd);
        s->sock_fd = INVALID_SOCKET;
    }
    PxSocket_CallbackComplete(s);
}

PyObject *
PxSocket_Register(PyObject *transport, PyObject *protocol_type)
{
    PxState *px = PXSTATE();
    PxSocket *s = (PxSocket *)transport;
    Context *c = s->ctx;
    PxListItem *item;
    PTP_SIMPLE_CALLBACK cb;

    assert(c);

    if (PyErr_Occurred()) {
        __debugbreak();
        PyErr_PrintEx(0);
        PyErr_Clear();
    }

    assert(!PyErr_Occurred());

    if (!PxSocket_SetProtocolType(s, protocol_type))
        return NULL;

    if (PxSocket_IS_CLIENT(s))
        cb = PxSocket_Connect;
    else
        cb = PxSocketServer_Start;

    if (Py_PXCTX() || PxSocket_IS_CLIENT(s)) {
        /* If we're a parallel thread, we can just palm off the work to
         * TrySubmitThreadpoolCallback directly.
         */
        if (!TrySubmitThreadpoolCallback(cb, c, c->ptp_cbe))
            PxSocket_SYSERROR("TrySubmitThreadpoolCallback");

    } else {
        /* We can't call TrySubmitThreadpoolCallback() here; we need to
         * have it called after our calling Python code invokes async.run().
         * Otherwise, parallel contexts will start running immediately.
         *
         * (Actually, now that I think about it, there's nothing wrong with
         *  a parallel context running immediately -- the problem is when you
         *  forget/don't to call async.run()/async.run_once() at all and the
         *  main thread just ends up terminating because of that.  The proper
         *  fix should really just be to call async.run_once() as part of
         *  cleanup/shutdown.)
         */

        item = _PyHeap_NewListItem(c);
        if (!item)
            return NULL;

        item->from = c;
        item->p1 = cb;

        Py_INCREF(transport);
        Py_INCREF(protocol_type);
        /* _async_run_once() will decrement whatever's left in p2->p4 if the
         * TrySubmitThreadpoolCallback(cb, c, NULL) call fails. */
        item->p2 = transport;
        item->p3 = protocol_type;

        PxList_Push(px->new_threadpool_work, item);
    }

    Py_RETURN_NONE;

end:
    /* For PxSocket_SYSERROR's goto. */
    return NULL;
}


PyObject *
pxsocket_close(PxSocket *s, PyObject *args)
{
    WRITE_LOCK(s);
    Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_CLOSE_SCHEDULED;
    WRITE_UNLOCK(s);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pxsocket_shutdown_doc, "xxx todo\n");

PyObject *
pxsocket_shutdown(PxSocket *s, PyObject *args)
{
    if (s->shutdown)
        SetEvent(s->shutdown);
    else if (s->parent)
        /* Should definitely put some sort of key protection
           in here. */
        SetEvent(s->parent->shutdown);

    Py_RETURN_NONE;
}

PyDoc_STRVAR(pxsocket_shutdown_server_doc, "xxx todo\n");

PyObject *
pxsocket_shutdown_server(PxSocket *s, PyObject *args)
{
    if (s->parent && s->parent->shutdown)
        SetEvent(s->parent->shutdown);

    Py_RETURN_NONE;
}



PyDoc_STRVAR(pxsocket_next_send_id_doc, "xxx todo\n");

PyObject *
pxsocket_next_send_id(PxSocket *s, PyObject *args)
{
    return PyLong_FromUnsignedLongLong(s->send_id+1);
}

PyDoc_STRVAR(
    pxsocket_sendfile_doc,
    "sendfile(before, path, after)\n\n"
);

PyObject *
pxsocket_sendfile(PxSocket *s, PyObject *args)
{
    PyObject *result = NULL;
    LPCWSTR name;
    Py_UNICODE *uname;
    int name_len;
    int access = GENERIC_READ;
    int share = FILE_SHARE_READ;
    int create_flags = OPEN_EXISTING;
    int file_flags = (
        FILE_FLAG_OVERLAPPED    |
        FILE_ATTRIBUTE_READONLY |
        FILE_FLAG_SEQUENTIAL_SCAN
    );
    HANDLE h;
    LARGE_INTEGER size;
    TRANSMIT_FILE_BUFFERS *tf;

    char *before_bytes = NULL, *after_bytes = NULL;
    int before_len = 0, after_len = 0;
    DWORD max_fsize = INT_MAX - 1;

    //Px_GUARD();

    if (PxSocket_IS_SENDFILE_SCHEDULED(s)) {
        PyErr_SetString(PyExc_RuntimeError,
                        "sendfile already scheduled for this callback");
        goto done;
    }

    assert(!s->sendfile_handle);

    if (!PyArg_ParseTuple(args, "z#u#z#:sendfile",
                          &before_bytes, &before_len,
                          &uname, &name_len,
                          &after_bytes, &after_len))
        goto done;

    name = (LPCWSTR)uname;

    h = CreateFile(name, access, share, 0, create_flags, file_flags, 0);
    if (!h || (h == INVALID_HANDLE_VALUE)) {
        PyErr_SetFromWindowsErrWithUnicodeFilename(0, uname);
        goto done;
    }

    if (!GetFileSizeEx(h, &size)) {
        CloseHandle(h);
        PyErr_SetFromWindowsErrWithUnicodeFilename(0, uname);
        goto done;
    }

    /* Subtract before/after buffer sizes from maximum sendable file size. */
    max_fsize -= before_len;
    max_fsize -= after_len;

    if ((size.QuadPart > (long long)INT_MAX) || (size.LowPart > max_fsize)) {
        CloseHandle(h);
        PyErr_SetString(PyExc_FileTooLargeError,
                        "file is too large to send via sendfile()");
        goto done;
    }

    tf = &s->sendfile_tfbuf;
    SecureZeroMemory(&s->sendfile_tfbuf, sizeof(TRANSMIT_FILE_BUFFERS));
    if (before_len) {
        tf->Head = before_bytes;
        tf->HeadLength = before_len;
    }
    if (after_len) {
        tf->Tail = after_bytes;
        tf->TailLength = after_len;
    }

    s->sendfile_nbytes = size.LowPart + before_len + after_len;
    s->sendfile_handle = h;
    Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_SENDFILE_SCHEDULED;
    result = Py_None;

done:
    if (!result)
        assert(PyErr_Occurred());

    return result;
}

PyDoc_STRVAR(
    pxsocket_sendfile_ranged_doc,
    "sendfile_ranged(before, path, after, offset, num_bytes_to_send)\n\n"
);

PyObject *
pxsocket_sendfile_ranged(PxSocket *s, PyObject *args)
{
    PyObject *result = NULL;
    LPCWSTR name;
    Py_UNICODE *uname;
    int name_len;
    int access = GENERIC_READ;
    int share = FILE_SHARE_READ;
    int create_flags = OPEN_EXISTING;
    int file_flags = (
        FILE_FLAG_OVERLAPPED    |
        FILE_ATTRIBUTE_READONLY |
        FILE_FLAG_RANDOM_ACCESS
    );
    HANDLE h;
    LARGE_INTEGER size;
    TRANSMIT_FILE_BUFFERS *tf;
    OVERLAPPED *ol = NULL;

    char *before_bytes = NULL, *after_bytes = NULL;
    int before_len = 0, after_len = 0;
    DWORD max_fsize = INT_MAX - 1;

    ULONGLONG num_bytes_to_send = 0;
    ULONGLONG offset = 0;
    DWORD offset_lo = 0;
    DWORD offset_hi = 0;

    //Px_GUARD();

    if (PxSocket_IS_SENDFILE_SCHEDULED(s)) {
        PyErr_SetString(PyExc_RuntimeError,
                        "sendfile already scheduled for this callback");
        goto done;
    }

    assert(!s->sendfile_handle);

    if (!PyArg_ParseTuple(args, "z#u#z#KK:sendfile_ranged",
                          &before_bytes, &before_len,
                          &uname, &name_len,
                          &after_bytes, &after_len,
                          &offset,
                          &num_bytes_to_send))
        goto done;

    name = (LPCWSTR)uname;

    h = CreateFile(name, access, share, 0, create_flags, file_flags, 0);
    if (!h || (h == INVALID_HANDLE_VALUE)) {
        PyErr_SetFromWindowsErrWithUnicodeFilename(0, uname);
        goto done;
    }

    /* Subtract before/after buffer sizes from maximum sendable file size. */
    max_fsize -= before_len;
    max_fsize -= after_len;

    if (num_bytes_to_send > (ULONGLONG)max_fsize) {
        CloseHandle(h);
        PyErr_SetString(PyExc_InvalidFileRangeError,
                        "range request exceeds maximum size (>2GB)");
        goto done;
    }

    s->sendfile_offset = offset;
    s->sendfile_num_bytes_to_send = (DWORD)num_bytes_to_send;
    s->overlapped_sendfile.Pointer = (PVOID)offset;

    tf = &s->sendfile_tfbuf;
    SecureZeroMemory(&s->sendfile_tfbuf, sizeof(TRANSMIT_FILE_BUFFERS));
    if (before_len) {
        tf->Head = before_bytes;
        tf->HeadLength = before_len;
    }
    if (after_len) {
        tf->Tail = after_bytes;
        tf->TailLength = after_len;
    }

    s->sendfile_nbytes = num_bytes_to_send + before_len + after_len;
    s->sendfile_handle = h;
    Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_SENDFILE_SCHEDULED;
    result = Py_None;

done:
    if (!result)
        assert(PyErr_Occurred());

    return result;
}

PyDoc_STRVAR(pxsocket_writefile_doc, "xxx todo\n");

PyObject *
pxsocket_writefile(PxSocket *s, PyObject *args)
{
    PyObject *result = NULL;
    return result;
}

PyDoc_STRVAR(pxsocket_readfile_doc, "xxx todo\n");

PyObject *
pxsocket_readfile(PxSocket *s, PyObject *args)
{
    PyObject *result = NULL;
    return result;
}


PyDoc_STRVAR(pxsocket_elapsed_doc, "xxx todo\n");

PyObject *
pxsocket_elapsed(PxSocket *s, PyObject *args)
{
    return PyLong_FromUnsignedLongLong(PxSocket_StopwatchStop(s));
}

#define _PXSOCKET(n, a) _METHOD(pxsocket, n, a)
#define _PXSOCKET_N(n) _PXSOCKET(n, METH_NOARGS)
#define _PXSOCKET_O(n) _PXSOCKET(n, METH_O)
#define _PXSOCKET_V(n) _PXSOCKET(n, METH_VARARGS)

static PyMethodDef PxSocketMethods[] = {
    _PXSOCKET_N(close),
    _PXSOCKET_N(elapsed),
    _PXSOCKET_N(shutdown),
    _PXSOCKET_N(shutdown_server),
    _PXSOCKET_V(sendfile),
    _PXSOCKET_V(sendfile_ranged),
    _PXSOCKET_V(writefile),
    _PXSOCKET_V(readfile),
    _PXSOCKET_N(next_send_id),
    { NULL, NULL }
};

#define _MEMBER(n, t, c, f, d) {#n, t, offsetof(c, n), f, d}
#define _PXSOCKETMEM(n, t, f, d)  _MEMBER(n, t, PxSocket, f, d)
#define _PXSOCKET_CB(n)        _PXSOCKETMEM(n, T_OBJECT,    0, #n " callback")
#define _PXSOCKET_ATTR_O(n)    _PXSOCKETMEM(n, T_OBJECT_EX, 0, #n " callback")
#define _PXSOCKET_ATTR_OR(n)   _PXSOCKETMEM(n, T_OBJECT_EX, 1, #n " callback")
#define _PXSOCKET_ATTR_I(n)    _PXSOCKETMEM(n, T_INT,       0, #n " attribute")
#define _PXSOCKET_ATTR_IR(n)   _PXSOCKETMEM(n, T_INT,       1, #n " attribute")
#define _PXSOCKET_ATTR_UI(n)   _PXSOCKETMEM(n, T_UINT,      0, #n " attribute")
#define _PXSOCKET_ATTR_UIR(n)  _PXSOCKETMEM(n, T_UINT,      1, #n " attribute")
#define _PXSOCKET_ATTR_LL(n)   _PXSOCKETMEM(n, T_LONGLONG,  0, #n " attribute")
#define _PXSOCKET_ATTR_LLR(n)  _PXSOCKETMEM(n, T_LONGLONG,  1, #n " attribute")
#define _PXSOCKET_ATTR_ULL(n)  _PXSOCKETMEM(n, T_ULONGLONG, 0, #n " attribute")
#define _PXSOCKET_ATTR_ULLR(n) _PXSOCKETMEM(n, T_ULONGLONG, 1, #n " attribute")
#define _PXSOCKET_ATTR_B(n)    _PXSOCKETMEM(n, T_BOOL,      0, #n " attribute")
#define _PXSOCKET_ATTR_BR(n)   _PXSOCKETMEM(n, T_BOOL,      1, #n " attribute")
#define _PXSOCKET_ATTR_D(n)    _PXSOCKETMEM(n, T_DOUBLE,    0, #n " attribute")
#define _PXSOCKET_ATTR_DR(n)   _PXSOCKETMEM(n, T_DOUBLE,    1, #n " attribute")
#define _PXSOCKET_ATTR_S(n)    _PXSOCKETMEM(n, T_STRING,    0, #n " attribute")

static PyMemberDef PxSocketMembers[] = {
    _PXSOCKET_ATTR_S(ip),
    _PXSOCKET_ATTR_S(host),
    _PXSOCKET_ATTR_IR(port),

    /* underlying socket (readonly) */
    _PXSOCKET_ATTR_IR(sock_family),
    _PXSOCKET_ATTR_IR(sock_type),
    _PXSOCKET_ATTR_IR(sock_proto),
    _PXSOCKET_ATTR_DR(sock_timeout),
    /*_PXSOCKET_ATTR_IR(sock_backlog),*/

    _PXSOCKET_ATTR_OR(parent),

    _PXSOCKET_ATTR_IR(num_children),
    _PXSOCKET_ATTR_IR(accepts_posted),
    _PXSOCKET_ATTR_IR(retired_clients),
    _PXSOCKET_ATTR_IR(fd_accept_count),
    _PXSOCKET_ATTR_IR(clients_connected),
    _PXSOCKET_ATTR_IR(clients_disconnecting),
    _PXSOCKET_ATTR_IR(num_accepts_to_post),
    _PXSOCKET_ATTR_IR(total_clients_reused),
    _PXSOCKET_ATTR_IR(total_clients_recycled),
    _PXSOCKET_ATTR_IR(target_accepts_posted),
    _PXSOCKET_ATTR_IR(client_connected_count),
    _PXSOCKET_ATTR_IR(total_accepts_attempted),
    _PXSOCKET_ATTR_IR(negative_accepts_to_post_count),

    _PXSOCKET_ATTR_IR(sem_acquired),
    _PXSOCKET_ATTR_IR(sem_released),
    _PXSOCKET_ATTR_IR(sem_timeout),
    _PXSOCKET_ATTR_IR(sem_count),
    _PXSOCKET_ATTR_IR(sem_release_err),

    _PXSOCKET_ATTR_ULL(stopwatch_frequency),

    _PXSOCKET_ATTR_ULL(stopwatch_start),
    _PXSOCKET_ATTR_ULL(stopwatch_stop),
    _PXSOCKET_ATTR_ULL(stopwatch_elapsed),

    _PXSOCKET_ATTR_UIR(num_bytes_just_sent),
    _PXSOCKET_ATTR_UIR(num_bytes_just_received),
    _PXSOCKET_ATTR_ULLR(total_bytes_sent),
    _PXSOCKET_ATTR_ULLR(total_bytes_received),

    _PXSOCKET_ATTR_IR(recvbuf_size),
    _PXSOCKET_ATTR_IR(sendbuf_size),

    _PXSOCKET_ATTR_LLR(send_id),
    _PXSOCKET_ATTR_LLR(recv_id),

    _PXSOCKET_ATTR_IR(ioloops),
    _PXSOCKET_ATTR_IR(last_thread_id),
    _PXSOCKET_ATTR_IR(this_thread_id),
    _PXSOCKET_ATTR_ULLR(thread_seq_id_bitmap),

    _PXSOCKET_ATTR_O(protocol),
    /*
    _PXSOCKET_ATTR_ULL(stopwatch_utc_start),
    _PXSOCKET_ATTR_ULL(stopwatch_utc_stop),
    _PXSOCKET_ATTR_ULL(stopwatch_utc_elapsed),
    */


    ///* handler */
    //_PXSOCKET_ATTR_O(handler),

    ///* callbacks */
    //_PXSOCKET_CB(connection_made),
    //_PXSOCKET_CB(data_received),
    //_PXSOCKET_CB(lines_received),
    //_PXSOCKET_CB(eof_received),
    //_PXSOCKET_CB(connection_lost),
    //_PXSOCKET_CB(exception_handler),
    //_PXSOCKET_CB(initial_connection_error),

    ///* attributes */
    //_PXSOCKET_ATTR_S(eol),
    //_PXSOCKET_ATTR_B(lines_mode),
    //_PXSOCKET_ATTR_BR(is_client),
    //_PXSOCKET_ATTR_I(wait_for_eol),
    //_PXSOCKET_ATTR_BR(is_connected),
    //_PXSOCKET_ATTR_I(max_line_length),

    { NULL }
};

static PyTypeObject PxSocket_Type = {
    PyVarObject_HEAD_INIT(0, 0)
    "_async.socket",                            /* tp_name */
    sizeof(PxSocket),                           /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)pxsocket_dealloc,               /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_reserved */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    PyObject_GenericSetAttr,                    /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                         /* tp_flags */
    "Asynchronous Socket Objects",              /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    PxSocketMethods,                            /* tp_methods */
    PxSocketMembers,                            /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    0,                                          /* tp_init */
    PyType_GenericAlloc,                        /* tp_alloc */
    0,                                          /* tp_new */
    0,                                          /* tp_free */
};

PyObject *
_async_client_or_server(PyObject *self, PyObject *args,
                        PyObject *kwds, char is_client)
{
    Py_RETURN_NONE;
}

/* addrinfo */



/* mod _async */
PyObject *
_async_client(PyObject *self, PyObject *args, PyObject *kwds)
{
    return create_pxsocket(args,
                           kwds,
                           Px_SOCKFLAGS_CLIENT,
                           NULL,  /* parent */
                           NULL); /* use_this_context hack */
}

PyObject *
_async_server(PyObject *self, PyObject *args, PyObject *kwds)
{
    return create_pxsocket(args,
                           kwds,
                           Px_SOCKFLAGS_SERVER,
                           NULL,  /* parent */
                           NULL); /* use_this_context hack */
}

PyObject *
_async_read(PyObject *self, PyObject *args, PyObject *kwds)
{
    return NULL;
}

PyObject *
_async_write(PyObject *self, PyObject *args, PyObject *kwds)
{
    return NULL;
}

PyDoc_STRVAR(_async_print_doc, "xxx todo\n");
PyObject *
_async_print(PyObject *self, PyObject *args)
{
    PyObject_Print(args, stdout, 0);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(_async_stdout_doc, "xxx todo\n");
PyObject *
_async_stdout(PyObject *self, PyObject *o)
{
    Py_INCREF(o);
    PyObject_Print(o, stdout, Py_PRINT_RAW);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(_async_stderr_doc, "xxx todo\n");
PyObject *
_async_stderr(PyObject *self, PyObject *o)
{
    Py_INCREF(o);
    PyObject_Print(o, stderr, Py_PRINT_RAW);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(_async_register_doc,
"register(transport=object, protocol=object) -> None\n\
\n\
Register an asynchronous transport object with the given protocol.");
PyObject *
_async_register(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *transport;
    PyObject *protocol_type;

    static const char *kwlist[] = { "transport", "protocol", NULL };
    static const char *fmt = "OO:register";

    Py_GUARD();

    if (!PyArg_ParseTupleAndKeywords(args, kwds, fmt, (char **)kwlist,
                                     &transport, &protocol_type))
        return NULL;

    if (PxSocket_Check(transport))
        return PxSocket_Register(transport, protocol_type);
    else {
        PyErr_SetString(PyExc_ValueError, "unsupported async object");
        return NULL;
    }
}

PyDoc_STRVAR(_async_transport_doc, "transport() -> active socket transport\n");

PyObject *
_async_transport(PyObject *self, PyObject *args)
{
    Context *c = ctx;
    if (!_PyParallel_IsParallelContext() ||
        (!c || (!c->io_obj)))
        Py_RETURN_NONE;

    if (c->io_type == Px_IOTYPE_SOCKET)
        return (PyObject *)c->io_obj;

    Py_RETURN_NONE;
}

static PyObject *_asyncmodule_obj;

PyDoc_STRVAR(_async_refresh_memory_stats_doc, "xxx todo\n");
PyObject *
_async_refresh_memory_stats(PyObject *obj, PyObject *args)
{
    PyObject *m;
    MEMORYSTATUSEX ms;
    Py_GUARD();

    m = _asyncmodule_obj;

    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);

    /* Erm, this is not the way this should be done (from a CPython
     * perspective).  (I'm sure I should be reflecting the C-struct
     * somehow.  Something to look into...)
     */

#define _refresh(n, ull)                        \
    do {                                        \
        PyObject *o = PyLong_FromLongLong(ull); \
        if (PyModule_AddObject(m, n, o))        \
            return NULL;                        \
    } while (0)

    _refresh("_memory_load", ms.dwMemoryLoad);
    _refresh("_memory_total_virtual", ms.ullTotalVirtual);
    _refresh("_memory_avail_virtual", ms.ullAvailVirtual);
    _refresh("_memory_total_physical", ms.ullTotalPhys);
    _refresh("_memory_avail_physical", ms.ullAvailPhys);
    _refresh("_memory_total_page_file", ms.ullTotalPageFile);
    _refresh("_memory_avail_page_file", ms.ullAvailPageFile);

    Py_RETURN_NONE;
}

/* 0 = success, 1 = failure */
int
_set_privilege(TCHAR *priv, BOOL enable)
{
    TOKEN_PRIVILEGES tp;
    HANDLE ph, th;
    DWORD access, error;
    BOOL status;
    int result = 1;

    ph = GetCurrentProcess();
    access = TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY;
    if (!OpenProcessToken(ph, access, &th))
        return 1;

    if (!LookupPrivilegeValue(NULL, priv, &tp.Privileges[0].Luid))
        goto close_th;

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = (enable ? SE_PRIVILEGE_ENABLED : 0);

    status = AdjustTokenPrivileges(th, FALSE, &tp, 0, NULL, 0);
    error = GetLastError();
    if (!status || (error != ERROR_SUCCESS))
        goto close_th;

    result = 0;

close_th:
    CloseHandle(th);

    return result;
}

PyDoc_STRVAR(_async_load_sys_info_doc, "xxx todo\n");
PyObject *
_async_load_sys_info(PyObject *obj, PyObject *args)
{
    PyObject *m;
    SYSTEM_INFO si;
    MEMORYSTATUSEX ms;
    size_t lpage_sz, min_fscache_sz, max_fscache_sz;
    DWORD fscache_flags;

    Py_GUARD();

    m = _asyncmodule_obj;

    _refresh("_sys_large_page_minimum", GetLargePageMinimum());

#define _set_obj(n, o)                          \
    do {                                        \
        if (PyModule_AddObject(m, n, o))        \
            return NULL;                        \
    } while (0)
#define _set_true(n) _set_obj(n, Py_True)
#define _set_false(n) _set_obj(n, Py_False)

    if (_set_privilege(SE_LOCK_MEMORY_NAME, TRUE))
        _set_false("_sys_large_pages_available");
    else
        _set_true("_sys_large_pages_available");

    if (!GetSystemFileCacheSize(&min_fscache_sz,
                                &max_fscache_sz,
                                &fscache_flags))
    {
        PyErr_SetFromWindowsErr(0);
        return NULL;
    }

    _refresh("_sys_min_filesystem_cache_size", min_fscache_sz);
    _refresh("_sys_max_filesystem_cache_size", max_fscache_sz);

    if (fscache_flags & FILE_CACHE_MAX_HARD_ENABLE)
        _set_true("_sys_file_cache_max_hard_enable");
    else
        _set_false("_sys_file_cache_max_hard_enable");

    if (fscache_flags & FILE_CACHE_MIN_HARD_ENABLE)
        _set_true("_sys_file_cache_min_hard_enable");
    else
        _set_false("_sys_file_cache_min_hard_enable");

#define _set_lpvoid(n, pv)                      \
    do {                                        \
        PyObject *o = PyLong_FromVoidPtr(pv);   \
        if (PyModule_AddObject(m, n, o))        \
            return NULL;                        \
    } while (0)

#define _set_dword(n, ul)                       \
    do {                                        \
        PyObject *o = PyLong_FromLong(ul);      \
        if (PyModule_AddObject(m, n, o))        \
            return NULL;                        \
    } while (0)

#define _set_word(n, w)                         \
    do {                                        \
        DWORD dw = (DWORD)w;                    \
        PyObject *o = PyLong_FromLong(w);       \
        if (PyModule_AddObject(m, n, o))        \
            return NULL;                        \
    } while (0)

    GetSystemInfo(&si);

    _set_lpvoid("_sys_min_address_allocation", si.lpMinimumApplicationAddress);
    _set_lpvoid("_sys_max_address_allocation", si.lpMaximumApplicationAddress);
    _set_dword("_sys_allocation_granularity", si.dwAllocationGranularity);

    _set_dword("_sys_cpu_type", si.dwProcessorType);
    _set_dword("_sys_num_cpus", si.dwNumberOfProcessors);
    _set_dword("_sys_active_cpu_mask", si.dwActiveProcessorMask);
    _set_dword("_sys_page_size", si.dwPageSize);

    _set_word("_sys_cpu_level", si.wProcessorLevel);
    _set_word("_sys_cpu_revision", si.wProcessorRevision);

    Py_RETURN_NONE;
}

PyDoc_STRVAR(_async_debug_doc, "debug() -> print debug string\n");

PyObject *
_async_debug(PyObject *self, PyObject *o)
{
    Py_ssize_t nbytes;
    char *buf = NULL;
    if (PyBytes_Check(o)) {
        buf = (char *)((PyBytesObject *)o)->ob_sval;
    } else if (PyByteArray_Check(o)) {
        buf = ((PyByteArrayObject *)o)->ob_bytes;
    } else if (PyUnicode_Check(o)) {
        buf = PyUnicode_AsUTF8AndSize(o, &nbytes);
    } else {
        PyErr_SetString(PyExc_ValueError, "must be bytes/bytearray/str");
        return NULL;
    }

    if (!PyErr_Occurred()) {
        assert(buf);
        OutputDebugStringA(buf);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyDoc_STRVAR(_async_debugbreak_doc,
             "debugbreak() -> set breakpoint (int3)\n");

PyObject *
_async_debugbreak(PyObject *self, PyObject *o)
{
    __debugbreak();
    Py_RETURN_NONE;
}

PyDoc_STRVAR(_async_debugbreak_on_next_exception_doc,
             "debugbreak() as soon as next exception occurs\n");

PyObject *
_async_debugbreak_on_next_exception(PyObject *self, PyObject *o)
{
    _PyParallel_SetDebugbreakOnNextException();
    Py_RETURN_NONE;
}

/* 0 on failure, 1 on success */
int
_extract_socket(Context *c, PxSocket **s)
{
    if (!c) {
        PyErr_SetString(PyExc_RuntimeError, "no active context");
        return 0;
    }

    if (!c->io_obj) {
        PyErr_SetString(PyExc_RuntimeError, "no active socket");
        return 0;
    }

    if (c->io_type != Px_IOTYPE_SOCKET) {
        PyErr_SetString(PyExc_RuntimeError, "active I/O object isn't socket");
        return 0;
    }

    *s = (PxSocket *)c->io_obj;

    return 1;
}

PyDoc_STRVAR(_async_enable_heap_override_doc,
             "enables heap override\n");

PyObject *
_async_enable_heap_override(PyObject *self, PyObject *o)
{
    Context  *c = ctx;
    PxSocket *s = NULL;

    if (!_extract_socket(c, &s))
        return NULL;

    if (_PyParallel_IsHeapOverrideActive()) {
        if (s->heap_override == _PyParallel_GetHeapOverride())
            Py_RETURN_NONE;
        else {
            PyErr_SetString(PyExc_RuntimeError,
                            "heap override already set");
            return NULL;
        }
    }

    if (!s->heap_override) {
        s->heap_override = HeapCreate(0, 0, 0);
        if (!s->heap_override) {
            PyErr_SetFromWindowsErr(0);
            return NULL;
        }
    }

    _PyParallel_SetHeapOverride(s->heap_override);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(_async_disable_heap_override_doc,
             "disables heap override\n");

PyObject *
_async_disable_heap_override(PyObject *self, PyObject *o)
{
    Context  *c = ctx;
    PxSocket *s = NULL;
    HANDLE    h = NULL;

    if (!_extract_socket(c, &s))
        return NULL;

    if (!_PyParallel_IsHeapOverrideActive()) {
        PyErr_SetString(PyExc_RuntimeError, "no heap override set");
        return NULL;
    }

    h = _PyParallel_GetHeapOverride();
    if (s->heap_override != h) {
        /* Could this happen? */
        __debugbreak();
    }

    _PyParallel_RemoveHeapOverride();
    Py_RETURN_NONE;
}

PyDoc_STRVAR(_async_is_heap_override_active_doc,
             "returns boolean indicating whether heap override is active\n");

PyObject *
_async_is_heap_override_active(PyObject *self, PyObject *o)
{
    if (_PyParallel_IsHeapOverrideActive())
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

PyDoc_STRVAR(_async_refresh_rio_availability_doc,
             "call after socket has been imported");
PyObject *
_async_refresh_rio_availability(PyObject *self, PyObject *o)
{
    PyObject *m = _asyncmodule_obj;
    Py_GUARD();
    if (RIOReceive) {
        _set_true("_sys_registered_io_available");
        _PyParallel_RegisteredIOAvailable = 1;
    } else
        _set_false("_sys_registered_io_available");
    Py_RETURN_NONE;
}

#define _ASYNC(n, a) _METHOD(_async, n, a)
#define _ASYNC_N(n) _ASYNC(n, METH_NOARGS)
#define _ASYNC_O(n) _ASYNC(n, METH_O)
#define _ASYNC_V(n) _ASYNC(n, METH_VARARGS)
#define _ASYNC_K(n) _ASYNC(n, METH_VARARGS | METH_KEYWORDS)
PyMethodDef _async_methods[] = {
    _ASYNC_V(map),
    _ASYNC_N(run),
    _ASYNC_O(wait),
    _ASYNC_V(read),
    _ASYNC_K(dict),
    _ASYNC_K(list),
    /*_ASYNC_N(xlist),*/
    //_ASYNC_V(open),
    _ASYNC_O(debug),
    _ASYNC_V(print),
    _ASYNC_V(write),
    _ASYNC_N(rdtsc),
    { "stdout", (PyCFunction)_async_stdout, METH_O, _async_stdout_doc },
    { "stderr", (PyCFunction)_async_stderr, METH_O, _async_stderr_doc },
    _ASYNC_O(_close),
    _ASYNC_O(signal),
    _ASYNC_K(client),
    _ASYNC_K(server),
    _ASYNC_O(protect),
    //_ASYNC_O(wait_any),
    //_ASYNC_O(wait_all),
    _ASYNC_O(prewait),
    _ASYNC_O(_address),
    _ASYNC_O(_rawfile),
    _ASYNC_K(register),
    _ASYNC_N(run_once),
    _ASYNC_N(cpu_count),
    _ASYNC_O(unprotect),
    _ASYNC_O(protected),
    _ASYNC_O(transport),
    _ASYNC_N(is_active),
    _ASYNC_V(submit_io),
    _ASYNC_O(read_lock),
    _ASYNC_V(_post_open),
    _ASYNC_N(debugbreak),
    _ASYNC_V(fileopener),
    _ASYNC_V(filecloser),
    _ASYNC_O(write_lock),
    _ASYNC_N(active_hogs),
    _ASYNC_V(submit_work),
    _ASYNC_V(submit_wait),
    _ASYNC_O(read_unlock),
    _ASYNC_O(write_unlock),
    _ASYNC_N(is_active_ex),
    _ASYNC_N(active_count),
    _ASYNC_O(_dbg_address),
    _ASYNC_V(submit_timer),
    _ASYNC_O(submit_class),
    _ASYNC_N(thread_seq_id),
    _ASYNC_O(submit_client),
    _ASYNC_O(submit_server),
    _ASYNC_O(try_read_lock),
    _ASYNC_O(try_write_lock),
    _ASYNC_V(submit_write_io),
    _ASYNC_V(signal_and_wait),
    _ASYNC_N(active_contexts),
    _ASYNC_N(active_io_loops),
    _ASYNC_N(is_parallel_thread),
    _ASYNC_N(persisted_contexts),
    _ASYNC_N(enable_heap_override),
    _ASYNC_N(refresh_memory_stats),
    _ASYNC_N(disable_heap_override),
    _ASYNC_V(call_from_main_thread),
    _ASYNC_N(seh_eav_in_io_callback),
    _ASYNC_N(is_heap_override_active),
    _ASYNC_N(refresh_rio_availability),
    _ASYNC_N(debugbreak_on_next_exception),
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
    PySocketModule_APIObject *socket_api;

    if (!PyType_Ready(&PxSocket_Type) < 0)
        return NULL;

    if (!PyType_Ready(&PyXList_Type) < 0)
        return NULL;

    m = PyModule_Create(&_asyncmodule);
    if (m == NULL)
        return NULL;

    _asyncmodule_obj = m;
    _async_refresh_memory_stats(NULL, NULL);
    if (!_async_load_sys_info(NULL, NULL))
        return NULL;

    socket_api = PySocketModule_ImportModuleAndAPI();
    if (!socket_api)
        return NULL;
    PySocketModule = *socket_api;

    if (PyModule_AddIntConstant(m, "_bits", Px_INTPTR_BITS))
        return NULL;

    if (PyModule_AddIntConstant(m, "_mem_page_size", Px_PAGE_SIZE))
        return NULL;

    if (PyModule_AddIntConstant(m, "_mem_cache_line_size",
                                Px_CACHE_ALIGN_SIZE))
        return NULL;

    if (PyModule_AddIntConstant(m, "_mem_large_page_size",
                                Px_LARGE_PAGE_SIZE))
        return NULL;

    if (PyModule_AddIntConstant(m, "_mem_default_heap_size",
                                Px_DEFAULT_HEAP_SIZE))
        return NULL;

    if (PyModule_AddIntConstant(m, "_mem_default_tls_heap_size",
                                Px_DEFAULT_TLS_HEAP_SIZE))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_Heap", sizeof(Heap)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_Context", sizeof(Context)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_ContextStats", sizeof(Stats)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_PxSocket", sizeof(PxSocket)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_PxSocketBuf", sizeof(PxSocketBuf)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_Context", sizeof(Context)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_PyObject", sizeof(PyObject)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_PxObject", sizeof(PxObject)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_PxState", sizeof(PxState)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_PxHeap", sizeof(PxHeap)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_PxPages", sizeof(PxPages)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_PxListItem", sizeof(PxListItem)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_PxListHead", sizeof(PxListHead)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_PxIO", sizeof(PxIO)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_TLS", sizeof(TLS)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_TLSBUF", sizeof(TLSBUF)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_SBUF", sizeof(SBUF)))
        return NULL;

    if (PyModule_AddIntConstant(m, "_sizeof_RBUF", sizeof(RBUF)))
        return NULL;

    if (PyModule_AddObject(m, "socket", (PyObject *)&PxSocket_Type))
        return NULL;

    if (PyModule_AddObject(m, "xlist", (PyObject *)&PyXList_Type))
        return NULL;

    PyExc_AsyncError = PyErr_NewException("_async.AsyncError", NULL, NULL);
    if (!PyExc_AsyncError)
        return NULL;

    PyExc_ProtectionError = \
        PyErr_NewException("_async.ProtectionError", PyExc_AsyncError, NULL);
    if (!PyExc_ProtectionError)
        return NULL;

    PyExc_UnprotectedError = \
        PyErr_NewException("_async.UnprotectedError", PyExc_AsyncError, NULL);
    if (!PyExc_UnprotectedError)
        return NULL;

    PyExc_AssignmentError = \
        PyErr_NewException("_async.AssignmentError", PyExc_AsyncError, NULL);
    if (!PyExc_AssignmentError)
        return NULL;

    PyExc_PersistenceError = \
        PyErr_NewException("_async.PersistenceError", PyExc_AsyncError, NULL);
    if (!PyExc_PersistenceError)
        return NULL;

    PyExc_NoWaitersError = \
        PyErr_NewException("_async.NoWaitersError", PyExc_AsyncError, NULL);
    if (!PyExc_NoWaitersError)
        return NULL;

    PyExc_WaitError = \
        PyErr_NewException("_async.WaitError", PyExc_AsyncError, NULL);
    if (!PyExc_WaitError)
        return NULL;

    PyExc_WaitTimeoutError = \
        PyErr_NewException("_async.WaitTimeoutError", PyExc_AsyncError, NULL);
    if (!PyExc_WaitTimeoutError)
        return NULL;

    PyExc_AsyncIOBuffersExhaustedError = \
        PyErr_NewException("_async.AsyncIOBuffersExhaustedError",
                           PyExc_AsyncError,
                           NULL);
    if (!PyExc_AsyncIOBuffersExhaustedError)
        return NULL;

    PyExc_InvalidFileRangeError = \
        PyErr_NewException("_async.InvalidFileRangeError",
                           PyExc_AsyncError,
                           NULL);
    if (!PyExc_InvalidFileRangeError)
        return NULL;

    PyExc_FileTooLargeError = \
        PyErr_NewException("_async.FileTooLargeError",
                           PyExc_AsyncError,
                           NULL);
    if (!PyExc_FileTooLargeError)
        return NULL;

    if (PyModule_AddObject(m, "AsyncError", PyExc_AsyncError))
        return NULL;

    if (PyModule_AddObject(m, "ProtectionError", PyExc_ProtectionError))
        return NULL;

    if (PyModule_AddObject(m, "UnprotectedError", PyExc_UnprotectedError))
        return NULL;

    if (PyModule_AddObject(m, "AssignmentError", PyExc_AssignmentError))
        return NULL;

    if (PyModule_AddObject(m, "PersistenceError", PyExc_PersistenceError))
        return NULL;

    if (PyModule_AddObject(m, "NoWaitersError", PyExc_NoWaitersError))
        return NULL;

    if (PyModule_AddObject(m, "WaitError", PyExc_WaitError))
        return NULL;

    if (PyModule_AddObject(m, "WaitTimeoutError", PyExc_WaitTimeoutError))
        return NULL;

    if (PyModule_AddObject(m, "AsyncIOBuffersExhaustedError",
                           PyExc_AsyncIOBuffersExhaustedError))
        return NULL;

    if (PyModule_AddObject(m, "InvalidFileRangeError",
                           PyExc_InvalidFileRangeError))
        return NULL;

    if (PyModule_AddObject(m, "FileTooLargeError",
                           PyExc_FileTooLargeError))
        return NULL;

    Py_INCREF(PyExc_AsyncError);
    Py_INCREF(PyExc_ProtectionError);
    Py_INCREF(PyExc_UnprotectedError);
    Py_INCREF(PyExc_AssignmentError);
    Py_INCREF(PyExc_PersistenceError);
    Py_INCREF(PyExc_NoWaitersError);
    Py_INCREF(PyExc_WaitError);
    Py_INCREF(PyExc_WaitTimeoutError);
    Py_INCREF(PyExc_AsyncIOBuffersExhaustedError);
    Py_INCREF(PyExc_InvalidFileRangeError);
    Py_INCREF(PyExc_FileTooLargeError);

    return m;
}

#ifdef __cpplus
}
#endif

/* vim:set ts=8 sw=4 sts=4 tw=78 et nospell:                                  */
