#include "Python.h"

#ifdef __cpplus
extern "C" {
#endif

#include <fcntl.h>

#include "pyparallel_private.h"
#include "fileio.h"
#include "frameobject.h"
#include "structmember.h"

//
// Calculate the address of the base of the structure given its type, and an
// address of a field within the structure.
//

#define _CONTAINING_RECORD(address, type, field) \
    ((type *)((PCHAR)(address) - (ULONG_PTR)(&((type *)0)->field)))

#define CS_SOCK_SPINCOUNT 4

__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
__declspec(thread) PyParallelContext *ctx = NULL;
#define _TMPBUF_SIZE 1024

__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
__declspec(thread) PyParallelIOContext *ioctx = NULL;

__declspec(align(SYSTEM_CACHE_ALIGNMENT_SIZE))
long Py_MainThreadId  = -1;
long Py_MainProcessId = -1;
long Py_ParallelContextsEnabled = -1;
size_t _PxObjectSignature = -1;
size_t _PxSocketSignature = -1;
size_t _PxSocketBufSignature = -1;
int _PxBlockingCallsThreshold = 5;

int _Py_CtrlCPressed = 0;
int _Py_InstalledCtrlCHandler = 0;

int _PxSocketServer_PreallocatedSockets = 1;

void *_PyHeap_Malloc(Context *c, size_t n, size_t align, int no_realloc);

static PyObject *PyExc_AsyncError;
static PyObject *PyExc_ProtectionError;
static PyObject *PyExc_NoWaitersError;
static PyObject *PyExc_WaitError;
static PyObject *PyExc_WaitTimeoutError;
static PyObject *PyExc_AsyncIOBuffersExhaustedError;
static PyObject *PyExc_PersistenceError;

static OVERLAPPED _NULL_OVERLAPPED;
#define IS_OVERLAPPED_NULL(ol) \
    (memcmp((ol), &_NULL_OVERLAPPED, sizeof(OVERLAPPED)) == 0)

#define _m_RESET_OVERLAPPED(ol) do {                                    \
    if (!IS_OVERLAPPED_NULL(ol)) {                                      \
        if (ol->hEvent) {                                               \
            if (!WSACloseEvent(ol->hEvent)) {                           \
                int last_error = WSAGetLastError();                     \
                if (last_error == WSA_INVALID_HANDLE)                   \
                    Py_FatalError("WSACloseEvent() -> INVALID_HANDLE"); \
                else                                                    \
                    Py_FatalError("WSACloseEvent() -> other");          \
            }                                                           \
        }                                                               \
        memset(ol, 0, sizeof(OVERLAPPED));                              \
    }                                                                   \
} while (0)

__inline
void
_RESET_OVERLAPPED(WSAOVERLAPPED *ol)
{
    if (!IS_OVERLAPPED_NULL(ol)) {
        if (ol->hEvent) {
            if (!WSACloseEvent(ol->hEvent)) {
                int last_error = WSAGetLastError();
                if (last_error == WSA_INVALID_HANDLE)
                    Py_FatalError("WSACloseEvent() -> INVALID_HANDLE");
                else
                    Py_FatalError("WSACloseEvent() -> other");
            }
        }
        memset(ol, 0, sizeof(OVERLAPPED));
    }
}

#define RESET_OVERLAPPED(ol) (_RESET_OVERLAPPED((WSAOVERLAPPED *)(ol)))


int _PyObject_GenericSetAttr(PyObject *o, PyObject *n, PyObject *v);
int _PyObject_SetAttrString(PyObject *, char *, PyObject *w);

PyObject *_PyObject_GenericGetAttr(PyObject *o, PyObject *n);
PyObject *_PyObject_GetAttrString(PyObject *, char *);

__inline
PyThreadState *
get_main_thread_state(void)
{
    return (PyThreadState *)_Py_atomic_load_relaxed(&_PyThreadState_Current);
}

__inline
PxState *
PXSTATE(void)
{
    register PyThreadState *pstate = get_main_thread_state();
    return (!pstate ? NULL : (PxState *)pstate->px);
}

void
_PyParallel_DisassociateCurrentThreadFromCallback(void)
{
    Context *c = ctx;
    if (c->disassociated)
        return;
    DisassociateCurrentThreadFromCallback((PTP_CALLBACK_INSTANCE)c->instance);
    c->disassociated = 1;
}

void
_PyParallel_BlockingCall(void)
{
    Context *c = ctx;
    Stats   *s = &(c->stats);
    Px_GUARD

    s->blocking_calls++;
    if (!c->disassociated && s->blocking_calls > _PxBlockingCallsThreshold)
        _PyParallel_DisassociateCurrentThreadFromCallback();
}

__inline
void *
_PyHeap_MemAlignedMalloc(Context *c, size_t n)
{
    return _PyHeap_Malloc(c, n, Px_MEM_ALIGN_SIZE, 0);
}

__inline
PxListItem *
_PyHeap_NewListItem(Context *c)
{
    return (PxListItem *)_PyHeap_MemAlignedMalloc(c, sizeof(PxListItem));
}

int
_Py_PXCTX(void)
{
    int active = (int)(Py_MainThreadId != _Py_get_current_thread_id());
    assert(Py_MainThreadId > 0);
    assert(Py_MainProcessId != -1);
    return active;
}

void
_PyObject_Dealloc(PyObject *o)
{
    PyTypeObject *tp;
    PyMappingMethods *mm;
    //PySequenceMethods *sm;
    destructor d;
#ifdef Py_DEBUG
    Py_GUARD_OBJ(o);
    Py_GUARD
#endif
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

__inline
char
_protected(PyObject *obj)
{
    return (obj->px_flags & Py_PXFLAGS_RWLOCK);
}

PyObject *
_async_protected(PyObject *self, PyObject *obj)
{
    Py_INCREF(obj);
    Py_RETURN_BOOL(_protected(obj));
}

__inline
void
_unprotect(PyObject *obj)
{
    if (_protected(obj)) {
        obj->px_flags &= ~Py_PXFLAGS_RWLOCK;
        obj->srw_lock = NULL;
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

__inline
PyObject *
_read_lock(PyObject *obj)
{
    AcquireSRWLockShared((PSRWLOCK)&(obj->srw_lock));
    return obj;
}
#define READ_LOCK(o) (_read_lock((PyObject *)o))

PyObject *
_async_read_lock(PyObject *self, PyObject *obj)
{
    Px_PROTECTION_GUARD(obj);
    Py_INCREF(obj);
    return _read_lock(obj);
}

__inline
PyObject *
_read_unlock(PyObject *obj)
{
    ReleaseSRWLockShared((PSRWLOCK)&(obj->srw_lock));
    return obj;
}
#define READ_UNLOCK(o) (_read_unlock((PyObject *)o))

PyObject *
_async_read_unlock(PyObject *self, PyObject *obj)
{
    Px_PROTECTION_GUARD(obj);
    Py_INCREF(obj);
    return _read_unlock(obj);
}

__inline
char
_try_read_lock(PyObject *obj)
{
    return TryAcquireSRWLockShared((PSRWLOCK)&(obj->srw_lock));
}

PyObject *
_async_try_read_lock(PyObject *self, PyObject *obj)
{
    Px_PROTECTION_GUARD(obj);
    Py_INCREF(obj);
    Py_RETURN_BOOL(_try_read_lock(obj));
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

PyObject *
_async_write_lock(PyObject *self, PyObject *obj)
{
    Px_PROTECTION_GUARD(obj);
    Py_INCREF(obj);
    return _write_lock(obj);
}

__inline
PyObject *
_write_unlock(PyObject *obj)
{
    ReleaseSRWLockExclusive((PSRWLOCK)&(obj->srw_lock));
    return obj;
}
#define WRITE_UNLOCK(o) (_write_unlock((PyObject *)o))

PyObject *
_async_write_unlock(PyObject *self, PyObject *obj)
{
    Px_PROTECTION_GUARD(obj);
    Py_INCREF(obj);
    return _write_unlock(obj);
}

__inline
char
_try_write_lock(PyObject *obj)
{
    return TryAcquireSRWLockExclusive((PSRWLOCK)&(obj->srw_lock));
}

PyObject *
_async_try_write_lock(PyObject *self, PyObject *obj)
{
    Px_PROTECTION_GUARD(obj);
    Py_INCREF(obj);
    Py_RETURN_BOOL(_try_write_lock(obj));
}

__inline
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

__inline
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

__inline
int
_Px_objobjargproc_ass(PyObject *o, PyObject *k, PyObject *v)
{
#ifdef Py_DEBUG
    if (!Py_PXCTX && !Py_PXCB) {
        assert(!Px_ISPX(o));
        assert(!Px_ISPX(k));
        assert(!Px_XISPX(v));
    }
#endif
    if (!Px_ISPY(o) || (!Py_PXCTX && !Py_PXCB))
        return 1;

    return !(!_Px_TryPersist(k) || !_Px_TryPersist(v));
}


int
_PyObject_GenericSetAttr(PyObject *o, PyObject *n, PyObject *v)
{
    PyTypeObject *tp;
    int result;
    assert(Py_ORIG_TYPE(o));

    if (!_Px_objobjargproc_ass(o, n, v))
        return -1;

    _Px_WRITE_LOCK(o);
    tp = Py_ORIG_TYPE_CAST(o);
    if (tp->tp_setattro)
        result = (*tp->tp_setattro)(o, n, v);
    else
        result = PyObject_GenericSetAttr(o, n, v);
    _Px_WRITE_UNLOCK(o);
    return result;
}

PyObject *
_PyObject_GenericGetAttr(PyObject *o, PyObject *n)
{
    PyTypeObject *tp;
    PyObject *result;
    assert(Py_ORIG_TYPE(o));

    _Px_READ_LOCK(o);
    tp = Py_ORIG_TYPE_CAST(o);
    if (tp->tp_getattro)
        result = (*tp->tp_getattro)(o, n);
    else
        result = PyObject_GenericGetAttr(o, n);
    _Px_READ_UNLOCK(o);

    return result;
}

int
_PyObject_SetAttrString(PyObject *o, char *n, PyObject *v)
{
    PyTypeObject *tp;
    int result;
    assert(Py_ORIG_TYPE(o));

    if (!_Px_objobjargproc_ass(o, NULL, v))
        return -1;

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
    if (!_Px_objobjargproc_ass(o, k, v))
        return -1;
    _Px_WRITE_LOCK(o);
    result = Py_ORIG_TYPE_CAST(o)->tp_as_mapping->mp_ass_subscript(o, k, v);
    _Px_WRITE_UNLOCK(o);
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
#ifdef Py_DEBUG
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
#endif
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

PyObject *
_async_wait(PyObject *self, PyObject *o)
{
    DWORD result;
    Px_PROTECTION_GUARD(o);
    Py_INCREF(o);
    if (!_PyEvent_TryCreate(o))
        return NULL;

    result = WaitForSingleObject((HANDLE)o->event, INFINITE);

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

__inline
PyObject *
_protect(PyObject *obj)
{
    if (!obj)
        return NULL;
    if (!_protected(obj)) {
        if (!_PyObject_PrepOrigType(obj, 0))
            return NULL;
        InitializeSRWLock((PSRWLOCK)&(obj->srw_lock));
        Py_PXFLAGS(obj) |= Py_PXFLAGS_RWLOCK;
    }
    return obj;
}

PyObject *
_async_protect(PyObject *self, PyObject *obj)
{
    Py_INCREF(obj);
    if (Py_ISPX(obj)) {
        PyErr_SetNone(PyExc_ProtectionError);
        return NULL;
    }
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

#ifdef Py_DEBUG
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

    lower = Px_PAGE_ALIGN_DOWN(p);
    upper = Px_PAGE_ALIGN(p);

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
        assert(x->count == 1);
        x->heaps[1] = h;
        x->count++;
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
    Py_GUARD

    InitializeSRWLock(&px->pages_srwlock);
}

void
_PxState_RegisterHeap(PxState *px, Heap *h, Context *c)
{
    int i;

    AcquireSRWLockExclusive(&px->pages_srwlock);

    assert((h->size % Px_PAGE_SIZE) == 0);

    for (i = 0; i < h->pages; i++) {
        void *p;
        Px_UINTPTR lower, upper;

        p = Px_PTR_ADD(h->base, (i * Px_PAGE_SIZE));

        lower = Px_PAGE_ALIGN_DOWN(p);
        upper = Px_PAGE_ALIGN(p);

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

    Py_GUARD

    px =  c->px;
    s  = &c->stats;

    AcquireSRWLockExclusive(&px->pages_srwlock);

    h = &c->heap;
    while (h) {
        heap_count++;

        assert((h->size % Px_PAGE_SIZE) == 0);

        for (i = 0; i < h->pages; i++) {
            void *p;
            Px_UINTPTR lower, upper;

            p = Px_PTR_ADD(h->base, (i * Px_PAGE_SIZE));

            lower = Px_PAGE_ALIGN_DOWN(p);
            upper = Px_PAGE_ALIGN(p);

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

__declspec(thread) int _Px_ObjectSignature_CallDepth = 0;
__declspec(thread) int _Px_SafeObjectSignatureTest_CallDepth = 0;

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

    AcquireSRWLockShared(&px->pages_srwlock);
    if (PxPages_Find(px->pages, m))
        signature = _MEMSIG_PX;
    ReleaseSRWLockShared(&px->pages_srwlock);

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
            return (Py_PXCTX ? 1 : ((s & _OBJSIG_PX) == _OBJSIG_PX) ? 1 : 0);
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

__inline
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
    unsigned long s;
    unsigned long o;

    assert(_MEMTEST(flags));

    if (m) {
        o = _Px_SafeObjectSignatureTest(m);
        s = _Px_MemorySignature(m);
        if (s & _MEMSIG_NOT_READY || (o > s))
            s = o;
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
            if (!(s & _MEMSIG_PX)) {
                printf("\ncouldn't find ptr: 0x%llx\n", m);
                PxPages_Dump((PXSTATE()->pages));
            } else {
                //printf("found ptr 0x%llx\n", m);
            }
            assert(s & _MEMSIG_PX);
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
    memset((void *)buf, 0, sizeof(buf));

    if (was_px_ctx)
        fmt = "%s called outside of parallel context (%s:%d)";
    else
        fmt = "%s called from within parallel context (%s:%d)";

    err = snprintf(buf, sizeof(buf), fmt, function, filename, lineno);
    if (err == -1)
        Py_FatalError("_PyParallel_ContextGuardFailure: snprintf failed");
    else
        Py_FatalError(buf);
}


#endif

#define Px_SIZEOF_HEAP        Px_CACHE_ALIGN(sizeof(Heap))
#define Px_USEABLE_HEAP_SIZE (Px_PAGE_ALIGN_SIZE - Px_SIZEOF_HEAP)
#define Px_NEW_HEAP_SIZE(n)  Px_PAGE_ALIGN((Py_MAX(n, Px_USEABLE_HEAP_SIZE)))

void *
Heap_Init(Context *c, size_t n)
{
    Heap  *h;
    Stats *s = &(c->stats);
    size_t size;
    int flags;

    if (n < Px_DEFAULT_HEAP_SIZE)
        size = Px_DEFAULT_HEAP_SIZE;
    else
        size = n;

    size = Px_PAGE_ALIGN(size);

    assert((size % Px_PAGE_SIZE) == 0);

    if (!c->h)
        /* First init. */
        h = &(c->heap);
    else {
        h = c->h->sle_next;
        h->sle_prev = c->h;
    }

    assert(h);
    h->pages = size / Px_PAGE_SIZE;

    h->size = size;
    flags = HEAP_ZERO_MEMORY;
    h->base = h->next = HeapAlloc(c->heap_handle, flags, h->size);
    if (!h->base)
        return PyErr_SetFromWindowsErr(0);
    h->last_alignment = Px_GET_ALIGNMENT(h->base);
    h->remaining = size;
    s->remaining += size;
    s->size += size;
    s->heaps++;
    c->h = h;
    h->sle_next = (Heap *)_PyHeap_Malloc(c, sizeof(Heap), 0, 0);
    assert(h->sle_next);
#ifdef Py_DEBUG
    _PxState_RegisterHeap(c->px, h, c);
#endif
    return h;
}

__inline
void *
_PyHeap_Init(Context *c, Py_ssize_t n)
{
    return Heap_Init(c, n);
}

void *
Heap_LocalMalloc(Context *c, size_t n, size_t align)
{
    void *next;
    wchar_t *fmt;
    size_t alignment_diff;
    size_t alignment = align;
    size_t requested_size = n;
    size_t aligned_size;

    if (!alignment)
        alignment = Px_PTR_ALIGN_SIZE;

    if (alignment > c->tbuf_last_alignment)
        alignment_diff = Px_PTR_ALIGN(alignment - c->tbuf_last_alignment);
    else
        alignment_diff = 0;

    aligned_size = Px_ALIGN(n, alignment);

    if (aligned_size < (c->tbuf_remaining-alignment_diff)) {
        if (alignment_diff) {
            c->tbuf_remaining -= alignment_diff;
            c->tbuf_allocated += alignment_diff;
            c->tbuf_alignment_mismatches++;
            c->tbuf_bytes_wasted += alignment_diff;
            c->tbuf_next = Px_PTR_ADD(c->tbuf_next, alignment_diff);
            assert(Px_PTR_ADD(c->tbuf_base, c->tbuf_allocated) == c->tbuf_next);
        }

        c->tbuf_mallocs++;
        c->tbuf_allocated += aligned_size;
        c->tbuf_remaining -= aligned_size;

        c->tbuf_bytes_wasted += (aligned_size - requested_size);

        c->tbuf_last_alignment = alignment;

        next = c->tbuf_next;
        c->tbuf_next = Px_PTR_ADD(c->tbuf_next, aligned_size);
        assert(Px_PTR_ADD(c->tbuf_base, c->tbuf_allocated) == c->tbuf_next);
        assert(_Py_IS_ALIGNED(next, alignment));

    } else {
        next = (void *)malloc(aligned_size);
        if (!next)
            return PyErr_NoMemory();

        memset(next, 0, aligned_size);

        c->leak_count++;
        c->leaked_bytes += aligned_size;
        c->last_leak = next;

        fmt = L"Heap_LocalMalloc: local buffer exhausted ("    \
              L"requested: %lld, available: %lld).  Resorted " \
              L"to malloc() -- note that memory will not be "  \
              L"freed!\n";
        fwprintf_s(stderr, fmt, aligned_size, c->tbuf_remaining);
    }

    return next;
}

void *
_PyHeap_Malloc(Context *c, size_t n, size_t align, int no_realloc)
{
    void  *next;
    Heap  *h;
    Stats *s = &c->stats;
    size_t alignment_diff;
    size_t alignment = align;
    size_t requested_size = n;
    size_t aligned_size;

    if (!alignment)
        alignment = Px_PTR_ALIGN_SIZE;

begin:
    h = c->h;

    if (alignment > h->last_alignment)
        alignment_diff = Px_PTR_ALIGN(alignment - h->last_alignment);
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

        h->last_alignment = alignment;

        next = h->next;
        h->next = Px_PTR_ADD(h->next, aligned_size);
        assert(Px_PTR_ADD(h->base, h->allocated) == h->next);
        assert(_Py_IS_ALIGNED(h->base, alignment));
        return next;
    }

    if (no_realloc)
        NULL;

    /* Force a resize. */
    if (!_PyHeap_Init(c, Px_NEW_HEAP_SIZE(aligned_size)))
        return Heap_LocalMalloc(c, aligned_size, alignment);

    goto begin;
}

__inline
void
_PyHeap_FastFree(Heap *h, Stats *s, void *p)
{
    h->frees++;
    s->frees++;
}

__inline
PxListHead *
_PyHeap_NewList(Context *c)
{
    PxListHead *l;

    l = (PxListHead *)_PyHeap_MemAlignedMalloc(c, sizeof(PxListHead));
    if (l)
        InitializeSListHead(l);

    return l;
}

void *
_PyHeap_Realloc(Context *c, void *p, size_t n)
{
    void  *r;
    Heap  *h = c->h;
    Stats *s = &c->stats;
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
    Heap  *h = c->h;
    Stats *s = &c->stats;
    Px_GUARD_MEM(p);

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
    PyObject *n;
    PxObject *x;
    Object   *o;
    size_t    object_size;
    size_t    total_size;
    size_t    bytes_to_copy;
    int       init_type;
    int       is_varobj = -1;

#define _INIT_NEW       1
#define _INIT_INIT      2
#define _INIT_RESIZE    3

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
        Py_REFCNT(n) = -1;
        if (is_varobj = (tp->tp_itemsize > 0)) {
            (c->stats.varobjs)++;
            Py_SIZE(n) = nitems;
        } else
            (c->stats.objects)++;

    } else {
        if (!Py_TYPE(p)) {
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
            Py_REFCNT(n) = -1;

            if (is_varobj = (tp->tp_itemsize > 0)) {
                (c->stats.varobjs)++;
                Py_SIZE(n) = nitems;
            } else
                (c->stats.objects)++;

        } else {
            /* Case 3: PyObject_GC_Resize called.  Object to resize may or may
             * not be from a parallel context.  Doesn't matter either way as
             * we don't really realloc anything behind the scenes -- we just
             * malloc another, larger chunk from our heap and copy over the
             * previous data. */
            init_type = _INIT_RESIZE;
            assert(!tp);
            assert(Py_TYPE(p) != NULL);
            tp = Py_TYPE(p);
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
            Py_REFCNT(n) = -1;

            if (Py_PXOBJ(p)) {
                /* XXX do we really need to do this?  (Original line of
                 * thinking was that we might need to treat the object
                 * differently down the track (i.e. during cleanup) if
                 * it was resized.) */
                Py_ASPX(p)->resized_to = n;
                x->resized_from = p;
            }

            (c->h->resizes)++;
            (c->stats.resizes)++;
        }
    }
    assert(tp);
    assert(Py_TYPE(n) == tp);
    assert(Py_REFCNT(n) == -1);
    assert(is_varobj == 0 || is_varobj == 1);

    Py_EVENT(n) = NULL;
    Py_PXFLAGS(n) = Py_PXFLAGS_ISPX;
    Py_ORIG_TYPE(n) = NULL;

    if (is_varobj)
        assert(Py_SIZE(n) == nitems);

    n->px = x;
    n->is_px = _Py_IS_PARALLEL;
    n->srw_lock = NULL;

    x->ctx = c;
    x->signature = _PxObjectSignature;

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

    return n;
}

__inline
PyObject *
Object_Init(PyObject *op, PyTypeObject *tp, Context *c)
{
    assert(tp->tp_itemsize == 0);
    return init_object(c, op, tp, 0);
}

__inline
PyObject *
Object_New(PyTypeObject *tp, Context *c)
{
    return init_object(c, NULL, tp, 0);
}

__inline
PyVarObject *
VarObject_Init(PyVarObject *v, PyTypeObject *tp, Py_ssize_t nitems, Context *c)
{
    assert(tp->tp_itemsize > 0);
    return (PyVarObject *)init_object(c, (PyObject *)v, tp, nitems);
}

__inline
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
    if (Py_PXCTX) {
        void *r;
        if (s & _SIG_PY)
            printf("\n_PxObject_Realloc(Py_PXCTX && p = _SIG_PY)\n");
        r = _PyHeap_Realloc(c, p, nbytes);
        return r;
    } else {
        if (s & _SIG_PX)
            printf("\n_PxObject_Realloc(!Py_PXCTX && p = _SIG_PX)\n");
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
    if (Py_PXCTX) {
        if (!(s & _SIG_PX))
            printf("\n_PxObject_Free(Py_PXCTX && p != _SIG_PX)\n");
        else
            _PyHeap_Free(c, p);
    } else {
        if (s & _SIG_PX)
            printf("\n_PxObject_Free(!Py_PXCTX && p = _SIG_PX)\n");
        else
            PyObject_Free(p);
    }
}
#else /* Py_DEBUG */
void *
_PxObject_Realloc(void *p, size_t nbytes)
{
    Px_GUARD
    return _PyHeap_Realloc(ctx, p, nbytes);
}

void
_PxObject_Free(void *p)
{
    Px_GUARD
    if (!p)
        return;
    _PyHeap_Free(ctx, p);
}
#endif

__inline
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

__inline
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


__inline
void
Px_INCCTX(Context *c)
{
    InterlockedIncrement(&(c->refcnt));
}

void
_PxState_ReleaseContext(PxState *px, Context *c)
{
    register Context *last;
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

__inline
long
Px_DECCTX(Context *c)
{
    register PxState *px = c->px;
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
    Context *c;
    PxState *px;

    px = (PxState *)malloc(sizeof(PxState));
    if (!px)
        return PyErr_NoMemory();

    memset((void *)px, 0, sizeof(PxState));

    px->errors = PxList_New();
    if (!px->errors)
        goto free_px;

    /*
    px->socket_errors = PxList_New();
    if (!px->socket_errors)
        goto free_errors;
    */

    px->completed_callbacks = PxList_New();
    if (!px->completed_callbacks)
        goto free_errors;

    px->completed_errbacks = PxList_New();
    if (!px->completed_errbacks)
        goto free_completed_callbacks;

    px->incoming = PxList_New();
    if (!px->incoming)
        goto free_completed_errbacks;

    px->finished = PxList_New();
    if (!px->finished)
        goto free_incoming;

    px->finished_sockets = PxList_New();
    if (!px->finished_sockets)
        goto free_finished;

    px->io_free  = PxList_New();
    if (!px->io_free)
        goto free_finished_sockets;

    px->io_free_wakeup = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!px->io_free_wakeup)
        goto free_io_free;

    px->wakeup = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!px->wakeup)
        goto free_io_wakeup;

#ifdef Py_DEBUG
    _PxState_InitPxPages(px);
#endif

    InitializeCriticalSectionAndSpinCount(&(px->cs), 12);

    tstate->px = px;

    tstate->is_parallel_thread = 0;
    px->ctx_ttl = 1;

    c = (Context *)malloc(sizeof(Context));
    if (!c)
        goto free_wakeup;
    memset((void *)c, 0, sizeof(Context));

    c->tstate = tstate;
    c->px = px;

    px->iob_ctx = c;

    if (!_PxState_AllocIOBufs(px, c, PyAsync_NUM_BUFS, PyAsync_IO_BUFSIZE))
        goto free_context;

    goto done;

free_context:
    free(c);

free_wakeup:
    CloseHandle(px->wakeup);

free_io_wakeup:
    CloseHandle(px->io_free_wakeup);

free_io_free:
    PxList_FreeListHead(px->io_free);

free_finished_sockets:
    PxList_FreeListHead(px->finished_sockets);

free_finished:
    PxList_FreeListHead(px->finished);

free_incoming:
    PxList_FreeListHead(px->incoming);

free_completed_errbacks:
    PxList_FreeListHead(px->completed_errbacks);

free_completed_callbacks:
    PxList_FreeListHead(px->completed_callbacks);

    /*
free_socket_errors:
    PxList_FreeListHead(px->socket_errors);
    */

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

int
_PyParallel_ExecutingCallbackFromMainThread(void)
{
    PyThreadState *tstate;
    PxState       *px;
    Py_GUARD

    tstate = PyThreadState_GET();

    assert(!tstate->is_parallel_thread);
    px = (PxState *)tstate->px;
    return (px->processing_callback == 1);
}

Py_TLS static int _PxNewThread = 1;

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


void
_PyParallel_EnteredCallback(Context *c, PTP_CALLBACK_INSTANCE instance)
{
    Stats *s;

    ctx = c;

    if (_PxNewThread) {
        /* xxx new thread init */
        _PxNewThread = 0;
    } else {
        /* xxx not a new thread */
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

    assert(c->instance);
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
    /*
    if (overlapped)
        assert(overlapped == &(c->overlapped));
    if (tp_io)
        assert(tp_io == c->tp_io);
        */
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
        switch (c->io_op) {
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

}

void
_PyParallel_ClearMainThreadId(void)
{
    _Py_sfence();
    Py_MainThreadId = 0;
    _Py_lfence();
    _Py_clflush(&Py_MainThreadId);
}

void
_PyParallel_CreatedGIL(void)
{
    _PyParallel_ClearMainThreadId();
}

void
_PyParallel_AboutToDropGIL(void)
{
    _PyParallel_ClearMainThreadId();
}

void
_PyParallel_DestroyedGIL(void)
{
    _PyParallel_ClearMainThreadId();
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
        Py_FatalError(buf);
    }

    if (Py_MainProcessId == -1)
        Py_FatalError("_PyParallel_JustAcquiredGIL: Py_MainProcessId == -1");

    _Py_sfence();
    Py_MainThreadId = _Py_get_current_thread_id();
    _Py_lfence();
    _Py_clflush(&Py_MainThreadId);
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

/* mod _async */
__inline
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

__inline
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

__inline
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

__inline
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

__inline
void
incref_args(Context *c)
{
    Py_INCREF(c->func);
    Py_XINCREF(c->args);
    Py_XINCREF(c->kwds);
    Py_XINCREF(c->callback);
    Py_XINCREF(c->errback);
}

__inline
void
incref_waitobj_args(Context *c)
{
    Py_INCREF(c->waitobj);
    Py_INCREF(c->waitobj_timeout);
    incref_args(c);
}


__inline
void
decref_args(Context *c)
{
    Py_XDECREF(c->func);
    Py_XDECREF(c->args);
    Py_XDECREF(c->kwds);
    Py_XDECREF(c->callback);
    Py_XDECREF(c->errback);
}

__inline
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

int
_Py_CheckCtrlC(void)
{
    Py_GUARD

    if (_Py_CtrlCPressed) {
        _Py_CtrlCPressed = 0;
        PyErr_SetNone(PyExc_KeyboardInterrupt);
        return 1;
    }

    return 0;
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
    Py_GUARD

    if (PyErr_CheckSignals() || _Py_CheckCtrlC())
        return NULL;

    if (!_Py_InstalledCtrlCHandler) {
        if (!SetConsoleCtrlHandler(_Py_HandleCtrlC, TRUE)) {
            PyErr_SetFromWindowsErr(0);
            return NULL;
        }
        _Py_InstalledCtrlCHandler = 1;
    }

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
    /* First error wins. */
    item = PxList_Pop(px->errors);
    if (item) {
        c = item->from;
        assert(PyExceptionClass_Check((PyObject *)item->p1));
        PyErr_Restore((PyObject *)item->p1,
                      (PyObject *)item->p2,
                      (PyObject *)item->p3);

        /* Ugh, so hacky.  If our originating context is an I/O object, don't
         * treat the context as 'finished'. */
        if (!c->io_obj) {
            PxList_Transfer(px->finished, item);
            InterlockedIncrement64(&(px->done));
        }
        return NULL;
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
            assert(!c->io_obj);
            ++processed_callbacks;
            item = PxList_Transfer(px->finished, item);
        } while (item);
    }

    item = PxList_Flush(px->completed_errbacks);
    if (item) {
        do {
            /* XXX TODO: update stats. */
            assert(!c->io_obj);
            ++processed_errbacks;
            item = PxList_Transfer(px->finished, item);
        } while (item);
    }

    if (px->active == 0 || purged)
        Py_RETURN_NONE;

    /* Return if we've done something useful... */
    if (processed_errors    ||
        processed_finished  ||
        processed_incoming  ||
        processed_errbacks  ||
        processed_callbacks)
            Py_RETURN_NONE;

    /* ...and wait for a second if we haven't. */
    err = WaitForSingleObject(px->wakeup, 1000);
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

/*
void
_do_work(Context *c)
{
    PTP_SIMPLE_CALLBACK cb = _PyParallel_WorkCallback;
    if (Py_PXCTX) {
        assert(c->instance);
        cb(c->instance, c);
    } else {
        if (!TrySubmitThreadpoolCallback(cb, c, NULL)) {
            PyErr_SetFromWindowsErr(0);
            PxContext_HandleError(c)

        }

    }
}
*/

__inline
int
submit_work(Context *c)
{
    int retval;
    PTP_SIMPLE_CALLBACK cb = _PyParallel_WorkCallback;
    if (Py_PXCTX) {
        assert(c->instance);
        cb(c->instance, c);
        return 1;
    } else {
        retval = TrySubmitThreadpoolCallback(cb, c, NULL);
        if (!retval)
            PyErr_SetFromWindowsErr(0);
        return retval;
    }
}

Context *
new_context(size_t heapsize)
{
    PxState  *px;
    Stats *s;
    PyThreadState *pstate;
    Context  *c = (Context *)malloc(sizeof(Context));

    if (!c)
        return (Context *)PyErr_NoMemory();

    memset((void *)c, 0, sizeof(Context));

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

    c->tbuf_next = c->tbuf_base = (void *)&(c->tbuf[0]);
    c->tbuf_last_alignment = Px_GET_ALIGNMENT(c->tbuf_next);
    c->tbuf_remaining = _PX_TMPBUF_SIZE;

    s = &(c->stats);
    s->startup_size = s->allocated;

    c->px->contexts_created++;
    InterlockedIncrement(&(c->px->contexts_active));

    return c;

free_heap:
    HeapDestroy(c->heap_handle);

free_context:
    free(c);

    return NULL;
}

Context *
new_context_for_socket(PxSocket *s)
{
    size_t heapsize = s->recvbuf_size + s->sendbuf_size + (2 * Px_PAGE_SIZE);
    Context *c;
    assert(!s->ctx);
    c = new_context(heapsize);
    if (!c)
        return NULL;

    /* Err... seems a bit odd that we'd need both of these. */
    c->io_type = Px_IOTYPE_SOCKET;
    c->context_type = Px_CTXTYPE_SOCK;
    c->io_obj = (PyObject *)s;
    s->ctx = c;
    return c;
}

PyObject *
_async_submit_work(PyObject *self, PyObject *args)
{
    PyObject *result = NULL;
    Context  *c;
    PxState  *px;

    c = new_context(0);
    if (!c)
        return NULL;

    px = c->px;

    if (!extract_args(args, c))
        goto free_context;

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
            PySys_FormatStdout("_async.run(%d) [%d/%d]\n", i,
                               active_contexts, persisted_contexts);
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
    c->tp_wait = CreateThreadpoolWait(cb, c, NULL);
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
    PxState *px = (Py_PXCTX ? ctx->px : PXSTATE());
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
    c->tp_io = CreateThreadpoolIo(f->h, callback, c, NULL);
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

    Px_GUARD

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
__inline
PyObject *
_wrap(PyTypeObject *tp, PyObject *args, PyObject *kwds)
{
    PyObject *self;
    Py_GUARD
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

#define _ASYNC_WRAP(name, type)                                                 \
PyDoc_STRVAR(_async_##name##_doc,                                               \
"Helper function for creating an async-protected instance of type '##name'\n"); \
PyObject *                                                                      \
_async_##name(PyObject *self, PyObject *args, PyObject *kwds)                   \
{                                                                               \
    return _wrap(&type, args, kwds);                                            \
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
#ifdef Py_DEBUG
    Px_GUARD
    assert(ctx->pstate);
    assert(ctx->pstate != ctx->tstate);
#endif
    return ctx->pstate;
}

void
_Px_NewReference(PyObject *op)
{
#ifdef Py_DEBUG
    Px_GUARD
    Px_GUARD_MEM(op);

    if (!_Px_TEST(op))
        printf("\n_Px_NewReference(op) -> failed _Px_TEST!\n");
    if (!Py_ASPX(op))
        printf("\n_Px_NewReference: no px object!\n");

    assert(op->is_px != _Py_NOT_PARALLEL);

    if (op->is_px != _Py_IS_PARALLEL)
        op->is_px = _Py_IS_PARALLEL;

#endif
    assert(Py_TYPE(op));

    op->ob_refcnt = 1;
    ctx->stats.newrefs++;
}

void
_Px_ForgetReference(PyObject *op)
{
#ifdef Py_DEBUG
    Px_GUARD_OBJ(op);
    Px_GUARD
#endif
    ctx->stats.forgetrefs++;
}

void
_Px_Dealloc(PyObject *op)
{
#ifdef Py_DEBUG
    Px_GUARD_OBJ(op);
    Px_GUARD
    assert(Py_ASPX(op)->ctx == ctx);
#endif
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
    Px_GUARD
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
    assert(!Py_PXCTX);
    assert(Px_PERSISTED(o));

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
        }

        assert(0);
    }
}

/* socket */
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

int
PxSocket_SetProtocolType(PxSocket *s, PyObject *p)
{
    PyObject *i; /* protocol instance */

    if (!p) {
        PyErr_SetString(PyExc_ValueError, "missing protocol value");
        return 0;
    }

    if (!PyType_CheckExact(p)) {
        PyErr_SetString(PyExc_ValueError, "protocol must be a class");
        return 0;
    }

    s->protocol_type = p;

    if (PyObject_GetAttrString(p, "long_lived") == Py_True) {
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_LONG_LIVED;
        i = PyObject_CallObject(p, NULL);
        if (!i) {
            PyErr_SetString(PyExc_ValueError,
                            "create protocol instance failed");
            return 0;
        }
        /* xxx should Py_CLEAR() the existing protocol here */
        WRITE_LOCK(s);
        s->protocol = i;
        WRITE_UNLOCK(s);

        if (!_protect(s->protocol))
            return 0;

    } else
        assert(!s->protocol);

    return 1;
}

void
PxSocket_CallbackComplete(Context *c)
{
    c->callback_completed->from = c;
    PxList_TimestampItem(c->callback_completed);
    PxList_Push(c->px->completed_callbacks, c->callback_completed);
}

void
PxSocket_ErrbackComplete(Context *c)
{
    c->errback_completed->from = c;
    PxList_TimestampItem(c->errback_completed);
    PxList_Push(c->px->completed_errbacks, c->errback_completed);
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

    if (fatal && s->sock_fd != -1) {
        closesocket(s->sock_fd);
        s->sock_fd = -1;
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

/*
 * Standard entry point for handling any socket errors.  There are two main
 * types of errors:
 *
 *  1. Actual underlying socket errors.  ``errcode`` will represent the value
 *     of WSAGetLastError().  We switch on this value and invoke the relevant
 *     callback (i.e. WSACONNRESET invokes the socket's ``connection_closed``
 *     callback, WSAETIMEDOUT invokes ``connection_timeout``, etc).
 *
 *     Error codes and the corresponding callbacks:
 *      - WSAECONNRESET     ->  connection_closed
 *      - WSAENETDOWN,
 *        WSAENETRESET,
 *        WSAECONNABORTED   ->  connection_lost
 *      - WSAETIMEDOUT      ->  connection_timeout
 *
 *      - everything else   ->  connection_error
 *
 *     The state of the socket at the point the socket's callback is invoked
 *     is guaranteed to be closed/disconnected for everything except the
 *     ``connection_error`` callback, where the state is undefined (or, more
 *     accurately, the state depends on the type of error that was
 *     encountered -- a WSAENOTCONN errcode implies a disconnected socket,
 *     whereas an unexpected WSAEINPROGRESS may not).
 *
 *     ``syscall`` will represent the name of the socket-method that was
 *     called that resulted in ``errcode`` being detected.
 *
 *     The socket's error handler (s->errorhandler()) won't have been called,
 *     nor should PyErr_Occurred() return anything at this point.
 *
 *     The relevant C functions that handle the invocation of the socket
 *     object's callback are as follows:
 *
 *          PxSocket_ConnectionClosed(s, op, ...)
 *          PxSocket_ConnectionLost(s, op, ...)
 *          PxSocket_ConnectionTimeout(s, op, ...)
 *          PxSocket_ConnectionError(s, op, syscall, errcode)
 *
 *     If the socket does not have a callback defined, the action is
 *     essentially a no-op.  No Python code is executed.  If there is a Python
 *     callback, it is executed in the standard parallel callback fashion.  If
 *     the callback executes without error, the functions return 1 to indicate
 *     success.
 *
 *     If the callback raises an error (as detected by a NULL return and a
 *     corresponding exception set (PyErr_Occurred()), 0 is returned to
 *     indicate failure.  It is our responsibility to invoke the socket's
 *     pre-defined exception handler via PxSocket_HandleException(s, exc).
 *     ``exc`` is the standard 3-part tuple of (exc_type, exc_value, exc_tb).
 *     This method also returns 1 on success, 0 on error.  If it's successful,
 *     we mark the context as done and push it to PxState's completed_errbacks
 *     list.
 *
 *     If there is no pre-defined exception handler, or the handler itself
 *     raises an exception, we need to propagate the error back to the main
 *     thread.  This is done in the usual fashion: mark the context as done,
 *     set the context's ``error`` list-item with the exception details, and
 *     push it to PxState's ``errors`` list.
 *
 *  This method is also responsible for cleaning up any I/O related resources.
 *  If c->tp_io is not NULL, CancelThreadpoolIo(c->tp_io) is called.  If
 *  c->overlapped.hEvent is not NULL, WSACloseEvent(c->overlapped.hEvent) is
 *  called.
 *
 */
void
PxSocket_HandleError(
    Context *c,
    int op,
    const char *syscall,
    int errcode)
{
    PxState *px = c->px;
    PxSocket *s = (PxSocket *)c->io_obj;
    PyThreadState *pstate = c->pstate;
    PyObject *func, *args, *result;
    char *name = NULL;

    assert(!PyErr_Occurred());

    switch (errcode) {
        case WSAEWOULDBLOCK:
            Py_FatalError("WSAEWOULDBLOCK!");
            break;

        case WSAECONNRESET:
        case WSAENETDOWN:
        case WSAENETRESET:
        case WSAECONNABORTED:
        case WSAENOTCONN:
        case WSAEDISCON:
            Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_CLOSE_SCHEDULED;
            break;

        case WSAETIMEDOUT:
            Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_CLOSE_SCHEDULED;
            Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_TIMEDOUT;
            break;

        case WSAESHUTDOWN:
            if (c->io_op == PxSocket_IO_SEND) {
                Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_SEND_SHUTDOWN;
                name = "send_shutdown";
            } else if (c->io_op == PxSocket_IO_RECV) {
                Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_RECV_SHUTDOWN;
                name = "recv_shutdown";
            }

            if ((Px_SOCKFLAGS(s) & Px_SOCKFLAGS_SEND_SHUTDOWN) &&
                (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_RECV_SHUTDOWN)) {
                Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_BOTH_SHUTDOWN;
                Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_CLOSE_SCHEDULED;
            }
            break;

        /* (we shouldn't see any of these in normal operating conditions) */
        case WSAEINTR:
        case WSAEINVAL:
        case WSAEFAULT:
        case WSAEMSGSIZE:
        case WSAEOPNOTSUPP:
        case WSAEINPROGRESS:
        case WSA_OPERATION_ABORTED:
        case WSAENOTSOCK:
        case WSANOTINITIALISED:
        default:
            PxSocket_WSAERROR(syscall);
            goto end;
    }

    MAYBE_DO_SEND_FAILED();

    if (!name)
        goto maybe_close;

    assert(name);
    READ_LOCK(s);
    func = PxSocket_GET_ATTR(name);
    READ_UNLOCK(s);

    if (!func || func == Py_None)
        goto maybe_close;

    args = Py_BuildValue("(O)", s);
    if (!args)
        PxSocket_FATAL();

    result = PyObject_CallObject(func, args);
    if (null_with_exc_or_non_none_return_type(result, c->pstate))
        PxSocket_EXCEPTION();

maybe_close:
    MAYBE_CLOSE();

end:
    return;
}

void
pxsocket_dealloc(PxSocket *s)
{
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

    Py_TYPE(s)->tp_free((PyObject *)s);
}

PyObject *
create_pxsocket(PyObject *args, PyObject *kwds, int flags)
{
    char *val;
    int len = sizeof(int);
    int nonblock = 1;
    PxSocket *s;
    SOCKET fd;
    char *host;
    Py_ssize_t hostlen;

    PyTypeObject *tp = &PxSocket_Type;

    if (Py_PXCTX && (flags != Px_SOCKFLAGS_SERVERCLIENT)) {
        int mismatch;
        Context *c = ctx;
        if (args || kwds) {
            PyErr_SetString(PyExc_ValueError,
                            "sockets cannot be created in async contexts");
            return NULL;
        }
        s = (PxSocket *)c->io_obj;
        if (!s || Py_TYPE(s) != tp) {
            PyErr_SetString(PyExc_ValueError,
                            "not in an async socket context");
            return NULL;
        }

        mismatch = (
            (flags == Px_SOCKFLAGS_CLIENT && !PxSocket_IS_CLIENT(s)) ||
            (flags == Px_SOCKFLAGS_SERVER && !PxSocket_IS_SERVER(s))
        );
        if (mismatch) {
            PyErr_SetString(PyExc_ValueError,
                            "client/server context mismatch");
            return NULL;
        }
        return (PyObject *)s;
    }

    s = (PxSocket *)(tp->tp_alloc(tp, 0));
    if (!s)
        return NULL;

    s->flags = flags;

    s->sock_fd = -1;
    s->sock_timeout = -1.0;
    s->errorhandler = PySocketModule.socket_errorhandler;
    s->sock_family = AF_INET;
    s->sock_type = SOCK_STREAM;

    if (PxSocket_IS_SERVERCLIENT(s)) {
        assert(!PxSocket_IS_SERVER(s));
        assert(!PxSocket_IS_CLIENT(s));
        goto serverclient;
    }

    assert(
        ( PxSocket_IS_CLIENT(s) && !PxSocket_IS_SERVER(s)) ||
        (!PxSocket_IS_CLIENT(s) &&  PxSocket_IS_SERVER(s))
    );


    if (!PyArg_ParseTupleAndKeywords(PxSocket_PARSE_ARGS))
        goto error;

    if (s->sock_family != AF_INET) {
        PyErr_SetString(PyExc_ValueError, "family must be AF_INET");
        goto error;
    }

    if (s->sock_type != SOCK_STREAM) {
        PyErr_SetString(PyExc_ValueError, "sock type must be SOCK_STREAM");
        goto error;
    }

    if (s->port < 0 || s->port > 0xffff) {
        PyErr_SetString(PyExc_OverflowError, "socket: port must be 0-65535");
        goto error;
    }

    if (host[0] >= '1' && host[0] <= '9') {
        int d1, d2, d3, d4;
        char ch;

        if (sscanf(host, "%d.%d.%d.%d%c", &d1, &d2, &d3, &d4, &ch) == 4 &&
            0 <= d1 && d1 <= 255 && 0 <= d2 && d2 <= 255 &&
            0 <= d3 && d3 <= 255 && 0 <= d4 && d4 <= 255)
        {
            struct sockaddr_in *sin;
            int *len;

            s->ip = (char *)malloc(16);
            memset(s->ip, 0, 16);
            strncpy(s->ip, host, 15);
            assert(s->ip[15] == '\0');
            s->host = &(s->ip[0]);

            if (PxSocket_IS_CLIENT(s)) {
                sin = &(s->remote_addr.in);
                len = &(s->remote_addr_len);
            } else if (PxSocket_IS_SERVER(s)) {
                sin = &(s->local_addr.in);
                len = &(s->local_addr_len);
            } else
                assert(0);

            sin->sin_addr.s_addr = htonl(
                ((long)d1 << 24) | ((long)d2 << 16) |
                ((long)d3 << 8)  | ((long)d4 << 0)
            );

            sin->sin_family = AF_INET;
            sin->sin_port = htons((short)s->port);
            *len = sizeof(*sin);
        } else {
            PyErr_SetString(PyExc_ValueError, "invalid IPv4 address");
            goto error;
        }
    } else {
        strncpy(s->host, host, hostlen);
        assert(!s->ip);
    }

serverclient:
    s->sock_fd = socket(s->sock_family, s->sock_type, s->sock_proto);
    if (s->sock_fd == INVALID_SOCKET) {
        s->errorhandler();
        goto error;
    }

    fd = s->sock_fd;
    if (ioctlsocket(fd, FIONBIO, &nonblock) == SOCKET_ERROR)
        goto free_sock;

    val = (char *)&(s->recvbuf_size);
    if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, val, &len) == SOCKET_ERROR)
        goto free_sock;

    assert(s->recvbuf_size >= 1024 && s->recvbuf_size <= 65536);

    val = (char *)&(s->sendbuf_size);
    if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, val, &len) == SOCKET_ERROR)
        goto free_sock;

    assert(s->sendbuf_size >= 1024 && s->sendbuf_size <= 65536);

    InitializeCriticalSectionAndSpinCount(&(s->cs), CS_SOCK_SPINCOUNT);

    if (!PxSocket_IS_SERVERCLIENT(s))
        _protect((PyObject *)s);

    if (PxSocket_IS_SERVER(s))
        s->preallocate = _PxSocketServer_PreallocatedSockets;

    return (PyObject *)s;

free_sock:
    s->errorhandler();
    (void)closesocket(s->sock_fd);
    s->sock_fd = -1;

error:
    assert(PyErr_Occurred());
    Py_DECREF(s);
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
PxSocket_SendCallback(Context *c)
{
    PxSocket *s = (PxSocket *)c->io_obj;
    PTP_WIN32_IO_CALLBACK cb = PxSocketClient_Callback;
    int op = PxSocket_IO_SEND;
    const char *syscall = "WSASendCallback";

    CHECK_SEND_RECV_CALLBACK_INVARIANTS();

    Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_SEND_SCHEDULED;

    if (c->io_result != NO_ERROR) {
        PxSocket_HandleError(c, op, syscall, c->io_result);
        goto maybe_close;
    }

    MAYBE_DO_SEND_COMPLETE();

    MAYBE_DO_CONNECTION_MADE();

    if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_SEND_SCHEDULED) {
        /* xxx todo */

    }

maybe_close:
    MAYBE_CLOSE();

    if (!PyErr_Occurred() && (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_CONNECTED))
        PxSocket_TryRecv(c);

end:
    return;
}

__inline
const char *
PxSocket_GetRecvCallback(Context *c)
{
    PxSocket *s = (PxSocket *)c->io_obj;
    int line_mode;
    READ_LOCK(s);
    line_mode = PyObject_IsTrue(PxSocket_GET_ATTR("line_mode"));
    READ_UNLOCK(s);
    return (line_mode ? "line_received" : "data_received");
}

/* 0 = failure, 1 = success */
int
PxSocket_TrySendResult(Context *c, PyObject *result)
{
    return 1;

    if (result == Py_None)
        return 1;

    /* xxx todo */
    return 0;
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
        goto callback_complete;

    va_start(va, format);
    args = Py_VaBuildValue(format, va);
    va_end(va);

    result = PyObject_CallObject(func, args);

    if (result)
        assert(!PyErr_Occurred());

    if (PyErr_Occurred())
        assert(!result);

    if (!result)
        PxSocket_EXCEPTION();

    if (result == Py_None)
        goto callback_complete;

    if (!PxSocket_TrySendResult(c, result))
        PxSocket_EXCEPTION();

    c->result = result;

    /* xxx todo: check for other async ops to apply */

callback_complete:
    /*
    if (PxSocket_IS_CONNECTED(s) && _pxsocket_recv(s, c))
        PxSocket_CallbackComplete(c);
    */

end:
    return;
}

void
PxSocket_ProcessReceivedData(Context *c)
{
    PxSocket *s = (PxSocket *)c->io_obj;

    assert(!PyErr_Occurred());

    /* xxx todo: recvbuf chunk checking */
    Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_RECV_MORE;

}

void
PxSocket_RecvCallback(Context *c)
{
    PxSocket *s = (PxSocket *)c->io_obj;
    int op = PxSocket_IO_RECV;
    const char *syscall = "WSARecvCallback";

    CHECK_SEND_RECV_CALLBACK_INVARIANTS();

    if (c->io_result != NO_ERROR) {
        PxSocket_HandleError(c, op, syscall, c->io_result);
        goto maybe_close;
    }

    PxSocket_ProcessReceivedData(c);
    if (PyErr_Occurred())
        goto maybe_close;

    if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_RECV_MORE) {
        PxSocket_TryRecv(c);
        goto end;
    }

    DO_DATA_RECEIVED();

    MAYBE_SEND();

maybe_close:
    MAYBE_CLOSE();

end:
    return;
}

void
PxSocket_TryRecv(Context *c)
{
    int err;
    PxSocket *s = (PxSocket *)c->io_obj;
    PxSocketBuf *b;
    Py_ssize_t nbytes;
    ULONG avail;
    OVERLAPPED *ol = NULL;
    DWORD flags = 0;
    DWORD _received;
    DWORD *rbytes = &_received;
    int op = PxSocket_IO_RECV;

    Px_GUARD

    if (ioctlsocket(s->sock_fd, FIONREAD, &avail) == SOCKET_ERROR)
        PxSocket_SOCKERROR("ioctlsocket");

    assert(!PxSocket_LONG_LIVED(s));

    /*
    if (PxSocket_LONG_LIVED(s))
        reuse_context = 0;

    if (PxSocket_RECV_MORE(s))
        reuse_context = 1;

    if (avail > 0 && ((avail + sizeof(PxSocketBuf) + 1) < c->h->remaining))
        reuse_context = 1;

    if (!c)
        reuse_context = 0;

    assert(reuse_context);

    if (!reuse_context)
        c = new_context_for_socket(s);

    if (!c)
        PxSocket_EXCEPTION();

    if (reuse_context)
        Px_CTXFLAGS(c) |= Px_CTXFLAGS_REUSED;
    */

    /*
    if (PxSocket_RECV_MORE(s)) {
        assert(c->rbuf_first);
        assert(c->rbuf_last);
        assert(c->rbuf_last->next == NULL);

        nbytes = Px_PAGE_ALIGN(Px_MAX(avail, s->recvbuf_size));

        b = new_pxsocketbuf(c, nbytes);
        if (!b)
            PxSocket_EXCEPTION();

        b->prev = c->rbuf_last;
        b->prev->next = b;
        c->rbuf_last = b;
    } else {
        c->rbuf_first = b;
        c->rbuf_last = b;
        b->prev = NULL;
        b->next = NULL;
    }
    */
    assert(!PxSocket_RECV_MORE(s));

    if (!c->rbuf_first) {
        nbytes = Px_PAGE_ALIGN(Px_MAX(avail, (ULONG)s->recvbuf_size));

        b = new_pxsocketbuf(c, nbytes);
        if (!b)
            PxSocket_EXCEPTION();

        c->rbuf_first = b;
        c->rbuf_last = b;
        b->prev = NULL;
        b->next = NULL;
    } else {
        b = c->rbuf_first;
    }

    c->io_op = op;
    c->io_result = NO_ERROR;

    if (!avail) {
        ol = &(c->overlapped);
        StartThreadpoolIo(c->tp_io);
        rbytes = NULL;
    }

    err = WSARecv(s->sock_fd, &(b->w), 1, rbytes, &flags, ol, NULL);
    if (err != SOCKET_ERROR) {
        if (!avail) {
            CancelThreadpoolIo(c->tp_io);
            rbytes = &_received;
            *rbytes = 0;
        }
        else
            assert(*rbytes == avail);
        PxSocketClient_Callback(NULL, c, ol, NO_ERROR, *rbytes, NULL);
    } else {
        err = WSAGetLastError();
        if (err != WSA_IO_PENDING)
            PxSocket_HandleError(c, op, "WSARecv", err);
    }

end:
    return;
}

void
PxSocket_TrySend(Context *c, PxSocketBuf *b)
{
    int err;
    PxSocket *s = (PxSocket *)c->io_obj;
    OVERLAPPED *ol = NULL;
    DWORD flags = 0;
    int op = PxSocket_IO_SEND;

    Px_GUARD

    c->io_op = op;
    c->io_result = NO_ERROR;

    if (!c->tp_io) {
        PTP_WIN32_IO_CALLBACK cb = PxSocketClient_Callback;
        assert(Px_SOCKFLAGS(s) & Px_SOCKFLAGS_SENDING_INITIAL_BYTES);
        c->tp_io = CreateThreadpoolIo((HANDLE)s->sock_fd, cb, c, NULL);
        if (!c->tp_io)
            PxSocket_SYSERROR("CreateThreadpoolIo");
        StartThreadpoolIo(c->tp_io);
    }

    ol = &(c->overlapped);

    err = WSASend(s->sock_fd, &(b->w), 1, NULL, flags, ol, NULL);
    if (err != SOCKET_ERROR) {
        CancelThreadpoolIo(c->tp_io);
        PxSocketClient_Callback(NULL, c, ol, NO_ERROR, b->w.len, NULL);
    } else {
        err = WSAGetLastError();
        if (err != WSA_IO_PENDING)
            PxSocket_HandleError(c, op, "WSARecv", err);
    }

end:
    return;
}

void
PxSocket_ConnectCallback(Context *c)
{
    int err;
    int op = PxSocket_IO_CONNECT;
    const char *syscall = "ConnectEx";
    PxSocket *s = (PxSocket *)c->io_obj;

    assert(PxSocket_IS_CONNECTED(s));
    if (c->io_result != NO_ERROR) {
        PxSocket_HandleError(c, op, syscall, c->io_result);
        goto end;
    }

    err = setsockopt(s->sock_fd, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0);
    if (err == SOCKET_ERROR)
        PxSocket_WSAERROR("setsockopt(SO_UPDATE_CONNECT_CONTEXT)");

    PxSocket_HandleCallback(c, "connection_made", "(O)", s);

    /* xxx todo: check for other async ops to apply */
end:
    return;
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

void
NTAPI
PxSocketClient_Callback(
    PTP_CALLBACK_INSTANCE instance,
    void *context,
    void *overlapped,
    ULONG io_result,
    ULONG_PTR nbytes,
    TP_IO *tp_io
)
{
    int try_recv = 0;
    Context *c = (Context *)context;
    PxSocket *s = (PxSocket *)c->io_obj;
    sockcb_t cb = NULL;

    EnterCriticalSection(&(s->cs));

    ENTERED_IO_CALLBACK();

    switch (c->io_op) {
        case PxSocket_IO_CONNECT:
            cb = PxSocket_ConnectCallback;
            break;
        case PxSocket_IO_RECV:
            cb = PxSocket_RecvCallback;
            break;
        case PxSocket_IO_SEND:
            cb = PxSocket_SendCallback;
            break;
        default:
            assert(0);
    }

    cb(c);

    if (PxSocket_IS_CONNECTED(s) && c->io_op != PxSocket_IO_SEND)
        PxSocket_TryRecv(c);

    LeaveCriticalSection(&(s->cs));
}

void
NTAPI
PxSocketServer_AcceptCallback(
    PTP_CALLBACK_INSTANCE instance,
    void *context,
    void *overlapped,
    ULONG io_result,
    ULONG_PTR nbytes,
    TP_IO *tp_io
)
{
    int err;
    int op = PxSocket_IO_ACCEPT;
    int bufsize;
    const char *syscall = "AcceptEx";
    void *p;
    LPSOCKADDR local;
    LPSOCKADDR remote;
    PxSocketBuf *b;
    int sz = sizeof(SOCKADDR);
    int llen = 0;
    int rlen = 0;

    Context *c = (Context *)context;
    PxSocket *s = (PxSocket *)c->io_obj;
    PxSocket *o;
    Context  *x = _Py_CAST_BACK(overlapped, Context *, Context, overlapped);

    o = (PxSocket *)x->io_obj;
    assert(o->parent == s);
    s = o;
    c = x;

    EnterCriticalSection(&(s->cs));

    ENTERED_IO_CALLBACK();

    if (c->io_result != NO_ERROR) {
        PyErr_SetFromWindowsErr(c->io_result);
        PxSocket_FATAL();
        goto end;
    }

    err = setsockopt(s->sock_fd,
                     SOL_SOCKET,
                     SO_UPDATE_ACCEPT_CONTEXT,
                     (char *)&(s->parent->sock_fd),
                     sizeof(SOCKET));
    if (err == SOCKET_ERROR)
        PxSocket_WSAERROR("setsockopt(SO_UPDATE_ACCEPT_CONTEXT)");

    if (PxSocket_HAS_INITIAL_BYTES(s))
        bufsize = 0;
    else
        bufsize = (s->recvbuf_size - (sz * 2));

    p = c->rbuf_first->w.buf;
    GetAcceptExSockaddrs(p, bufsize, sz, sz, &local, &llen, &remote, &rlen);

    memcpy(&(s->local_addr), local, llen);
    memcpy(&(s->remote_addr), remote, rlen);

    if (PxSocket_HAS_INITIAL_BYTES(s)) {
        memset(p, 0, sz * 2);
        b = PxSocket_GetInitialBytes(c);
        if (!b)
            PxSocket_EXCEPTION();
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_SENDING_INITIAL_BYTES;
        PxSocket_TrySend(c, b);
    }


    /* xxx todo:
     *  - send initial bytes
     *  - `connection_made`
     *  - create a new threadpool io
     */

    /*
    if (PxSocket_IS_CONNECTED(s) && c->io_op != PxSocket_IO_SEND)
        PxSocket_TryRecv(c);
    */

end:
    LeaveCriticalSection(&(s->cs));
}


/* 0 = failure, 1 = success */
int
PxSocket_InitProtocol(Context *c)
{
    PxSocket *s = (PxSocket *)c->io_obj;
    if (!s->protocol) {
        assert(!PxSocket_LONG_LIVED(s));
        assert(s->protocol_type);
        s->protocol = PyObject_CallObject(s->protocol_type, NULL);
        if (!s->protocol)
            PxSocket_FATAL();

        if (!_Px_TryPersist(s->protocol))
            PxSocket_FATAL();
    } else
        assert(!PxSocket_LONG_LIVED(s));

    assert(s->protocol);

    if (PxSocket_HasCallback(s, "connection_made"))
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_HAS_CONNECTION_MADE;

    if (PxSocket_HasCallback(s, "data_received") ||
        PxSocket_HasCallback(s, "line_received"))
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_HAS_DATA_RECEIVED;

    if (PxSocket_HasCallback(s, "send_failed"))
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_HAS_SEND_FAILED;

    if (PxSocket_HasCallback(s, "send_complete"))
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_HAS_SEND_COMPLETE;

    return 1;

end:
    return 0;
}

#define INVALID_INITIAL_BYTES                                           \
    "initial_bytes_to_send must be one of the following types: bytes, " \
    "unicode or callable"

/* 0 = failure (error will be set), 1 = no error occurred */
int
PxSocket_InitInitialBytes(Context *c)
{
    PxSocket *s = (PxSocket *)c->io_obj;
    PyObject *o, *t = s->protocol;
    assert(t);
    assert(!PyErr_Occurred());

    o = PyObject_GetAttrString(t, "initial_bytes_to_send");
    if (PyErr_ExceptionMatches(PyExc_AttributeError)) {
        PyErr_Clear();
        return 1;
    }

    if (PyErr_Occurred())
        return 0;

    if (!o || o == Py_None)
        return 1;

    if (PyBytes_Check(o))
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_INITIAL_BYTES_PYBYTES;

    else if (PyUnicode_Check(o))
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_INITIAL_BYTES_UNICODE;

    else if (PyCallable_Check(o))
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_INITIAL_BYTES_CALLABLE;

    else {
        PyErr_SetString(PyExc_ValueError, INVALID_INITIAL_BYTES);
        return 0;
    }

    /* XXX TODO: check that there are no 'expect_*' members, as these imply
     * the socket will wait for data from the peer after a connection has been
     * made, which conflicts with an 'initial_bytes_to_send' setting. */

    Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_INITIAL_BYTES;

    if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_INITIAL_BYTES_CALLABLE)
        s->initial_bytes_callable = o;
    else {
        PxSocketBuf *b;
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_INITIAL_BYTES_STATIC;
        if (!PxSocket_IS_SERVERCLIENT(s)) {
            assert(!s->initial_bytes);
            if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_INITIAL_BYTES_PYBYTES)
                b = new_pxsocketbuf_from_bytes(c, (PyBytesObject *)o);
            else if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_INITIAL_BYTES_UNICODE)
                b = new_pxsocketbuf_from_unicode(c, (PyUnicodeObject *)o);
            if (!b)
                return 0;
            s->initial_bytes = b;
        } else
            s->initial_bytes = s->parent->initial_bytes;

        assert(s->initial_bytes);
    }

    return 1;
}

PxSocketBuf *
PxSocket_GetInitialBytes(Context *c)
{
    PxSocket *s = (PxSocket *)c->io_obj;
    PyObject *o;

    if (Px_SOCKFLAGS(s) & Px_SOCKFLAGS_INITIAL_BYTES_STATIC)
        return s->initial_bytes;


    assert(Px_SOCKFLAGS(s) & Px_SOCKFLAGS_INITIAL_BYTES_CALLABLE);
    o = s->initial_bytes_callable;
    return _try_extract_something_sendable_from_object(c, o, 0);
}

void
PxSocket_InitExceptionHandler(Context *c)
{
    PxSocket *s = (PxSocket *)c->io_obj;
    PyObject *eh;
    assert(s->protocol);
    assert(!PyErr_Occurred());
    if (!s->exception_handler) {
        eh = PyObject_GetAttrString(s->protocol, "exception_handler");
        if (eh && eh != Py_None)
            s->exception_handler = eh;
        else
            PyErr_Clear();
    }
    assert(!PyErr_Occurred());
}

void
NTAPI
PxSocket_Connect(PTP_CALLBACK_INSTANCE instance, void *context)
{
    Context *c = (Context *)context;
    PxState *px;
    PTP_WIN32_IO_CALLBACK cb;
    BOOL success;
    SOCKET fd;
    struct sockaddr *sa;
    int len;
    PxSocketBuf *b;
    char *cbuf = NULL;
    size_t size = 0;
    WSAOVERLAPPED *ol;
    PyObject *result = NULL;
    PxSocket *s = (PxSocket *)c->io_obj;

    Px_GUARD

    ENTERED_CALLBACK();

    if (!PxSocket_InitProtocol(c))
        goto end;

    assert(s->protocol);
    assert(!PyErr_Occurred());

    b = _pxsocket_initial_bytes_to_send(c, s);
    if (PyErr_Occurred())
        PxSocket_EXCEPTION();

    if (b) {
        cbuf = b->w.buf;
        size = b->w.len;
    }

    px = c->px;

    if (!(PxSocket_IS_BOUND(s))) {
        struct sockaddr_in *sin = &(s->local_addr.in);
        sin->sin_family = AF_INET;
        sin->sin_addr.s_addr = INADDR_ANY;
        sin->sin_port = 0;
        if (bind(s->sock_fd, (struct sockaddr *)sin, sizeof(*sin)))
            PxSocket_WSAERROR("bind");
        Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_BOUND;
    }

    cb = PxSocketClient_Callback;
    c->tp_io = CreateThreadpoolIo((HANDLE)s->sock_fd, cb, c, NULL);
    if (!c->tp_io)
        PxSocket_SYSERROR("CreateThreadpoolIo");

    c->io_type = Px_IOTYPE_SOCKET;
    c->io_op = PxSocket_IO_CONNECT;

    ol = &(c->overlapped);
    RESET_OVERLAPPED(ol);
    /*
    ol->hEvent = WSACreateEvent();
    if (!ol->hEvent)
        PxSocket_WSAERROR("WSACreateEvent");
    */

    sa = (struct sockaddr *)&(s->remote_addr.sa);
    len = s->remote_addr_len;
    fd = s->sock_fd;

    StartThreadpoolIo(c->tp_io);
    PxSocket_InitExceptionHandler(c);
    Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_CONNECTED;
    success = ConnectEx(fd, sa, len, cbuf, (DWORD)size, NULL, ol);
    if (!success) {
        if (WSAGetLastError() != WSA_IO_PENDING) {
            Px_SOCKFLAGS(s) &= ~Px_SOCKFLAGS_CONNECTED;
            s->exception_handler = NULL;
            PxSocket_WSAERROR("ConnectEx");
        }
    } else {
        PTP_CALLBACK_INSTANCE i = c->instance;
        CancelThreadpoolIo(c->tp_io);
        cb(NULL, c, ol, NO_ERROR, 0, NULL);
    }

end:
    return;
}

__inline
PxSocket *
PxSocketServer_CreateClientSocket(Context *c)
{
    PxSocket *s = (PxSocket *)c->io_obj;
    PxSocket *o;
    Context  *x;
    PxSocketBuf *b;

    o = (PxSocket *)create_pxsocket(NULL, NULL, Px_SOCKFLAGS_SERVERCLIENT);
    if (!o)
        return NULL;

    assert(PxSocket_IS_SERVERCLIENT(o));
    assert(s->protocol_type);

    o->parent = s;
    o->protocol_type = s->protocol_type;
    o->protocol = PyObject_CallObject(o->protocol_type, NULL);
    if (!o->protocol)
        goto error;

    if (!new_context_for_socket(o))
        goto error;

    x = o->ctx;

    if (!PxSocket_InitInitialBytes(x))
        goto error;

    PxSocket_InitExceptionHandler(x);

    b = new_pxsocketbuf(x, o->recvbuf_size);
    if (!b)
        goto error;

    x->rbuf_first = b;
    x->rbuf_last = b;

    return o;

error:
    assert(PyErr_Occurred());
    if (closesocket(o->sock_fd) == SOCKET_ERROR) {
        /* xxx: which error is more important? */
        /* x->errorhandler(); */
    }
    return NULL;
}

#define I2S(i) (_Py_CAST_BACK(i, PxSocket *, PyObject, slist_entry))
#define PxList_PushSocket(h, s) (I2O(PxList_Push((PxListHead *)(h), O2I((o)))))
#define PxList_PopSocket(h) (I2S(PxList_Pop((PxListHead *)(h))))

PxSocket *
PxSocketServer_AllocClientSockets(Context *c, int n)
{
    int i;
    PxSocket *s = (PxSocket *)c->io_obj;
    PxSocket *first = NULL;
    PxSocket *last = NULL;
    PxSocket *x;

    assert(PxSocket_IS_SERVER(s));

    if (!n)
        n = s->preallocate;

    for (i = 0; i < n; i++) {
        PxSocket *x = PxSocketServer_CreateClientSocket(c);
        if (!x)
            goto error;

        if (!first)
            first = x;
        else
            last->next = x;

        last = x;

        assert(!x->next);
    }

    return first;

error:
    for (x = first; x; x = x->next)
        (void)closesocket(x->sock_fd);

    return NULL;
}

int PxSocketServer_WaitOn_FD_ACCEPT(Context *c, Context *x);

void
NTAPI
PxSocket_AcceptWait(
    PTP_CALLBACK_INSTANCE instance,
    void *context,
    PTP_WAIT wait, TP_WAIT_RESULT wait_result)
{
    PxSocket *s;
    Context *c, *x;

    x = (Context *)context;
    s = (PxSocket *)x->io_obj;
    c = s->ctx;

    ENTERED_CALLBACK();

    /* xxx todo: post more accepts; enumerate existing sockets, find those
     * with excessive SO_CONNECT_TIMES + no posted bytes and disconnect them.
     */

    /*PxSocketServer_WaitOn_FD_ACCEPT(c, x);*/

/*end:*/
    return;
}

/* 0 = failure, 1 = no error */
int
PxSocketServer_WaitOn_FD_ACCEPT(Context *c, Context *x)
{
    PTP_WAIT_CALLBACK cb = PxSocket_AcceptWait;
    PxSocket *s = (PxSocket *)c->io_obj;

    if (!x) {
        assert(!s->fd_accept);
        x = new_context(0);
        if (!x)
            return 0;
        x->io_obj = (PyObject *)s;
        s->fd_accept = WSACreateEvent();
        if (!s->fd_accept)
            goto wsa_error;
        x->tp_wait = CreateThreadpoolWait(cb, x, NULL);
        if (!x->tp_wait)
            goto sys_error;
    } else {
        assert(s->fd_accept);
        assert(x->io_obj == (PyObject *)s);
        assert(x->tp_wait);
    }

    if (WSAEventSelect(s->sock_fd, s->fd_accept, FD_ACCEPT) == SOCKET_ERROR)
        goto wsa_error;

    SetThreadpoolWait(x->tp_wait, s->fd_accept, NULL);
    return 1;

sys_error:
    PyErr_SetFromWindowsErr(0);
    goto error;

wsa_error:
    PyErr_SetFromWindowsErr(WSAGetLastError());

error:
    if (s->fd_accept)
        WSACloseEvent(s->fd_accept);
    return 0;
}

void
NTAPI
PxSocketServer_Start(PTP_CALLBACK_INSTANCE instance, void *context)
{
    Context *c = (Context *)context;
    PxState *px;
    PTP_WIN32_IO_CALLBACK cb;
    BOOL success;
    char failed = 0;
    struct sockaddr *sa;
    int len;
    DWORD bufsize;
    char *buf = NULL;
    DWORD size = 0;
    int accepts = 0;
    PyObject *result = NULL;
    PxSocket *s = (PxSocket *)c->io_obj;
    PxSocket *o;
    Context  *x;
    PyTypeObject *tp = &PxSocket_Type;

    Px_GUARD

    ENTERED_CALLBACK();

    if (!PxSocket_InitProtocol(c))
        goto end;

    PxSocket_InitExceptionHandler(c);

    assert(PxSocket_IS_SERVER(s));

    assert(s->protocol);
    assert(!PyErr_Occurred());

    if (!PxSocket_InitInitialBytes(c))
        PxSocket_FATAL();

    px = c->px;
    c->io_op = PxSocket_IO_ACCEPT;

    assert(!(PxSocket_IS_BOUND(s)));
    sa = (struct sockaddr *)&(s->local_addr.in);
    len = s->local_addr_len;
    if (bind(s->sock_fd, sa, len) == SOCKET_ERROR)
        PxSocket_WSAERROR("bind");
    Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_BOUND;

    if (listen(s->sock_fd, SOMAXCONN) == SOCKET_ERROR)
        PxSocket_WSAERROR("listen");
    Px_SOCKFLAGS(s) |= Px_SOCKFLAGS_LISTEN;

    if (!PxSocketServer_WaitOn_FD_ACCEPT(c, NULL))
        PxSocket_FATAL();

    s->first = PxSocketServer_AllocClientSockets(c, 0);
    if (!s->first)
        PxSocket_FATAL();

    cb = PxSocketServer_AcceptCallback;
    c->tp_io = CreateThreadpoolIo((HANDLE)s->sock_fd, cb, c, NULL);
    if (!c->tp_io)
        PxSocket_SYSERROR("CreateThreadpoolIo");

    size = sizeof(struct sockaddr_in) + 16;
    if (PxSocket_HAS_INITIAL_BYTES(s))
        bufsize = 0;
    else
        bufsize = (DWORD)(s->recvbuf_size - (size * 2));

    StartThreadpoolIo(c->tp_io);
    for (o = s->first; o; o = o->next) {
        x = o->ctx;
        assert(x->rbuf_first);
        assert(x->rbuf_first->w.len == s->recvbuf_size);

        success = AcceptEx(s->sock_fd,
                           o->sock_fd,
                           x->rbuf_first->w.buf,
                           bufsize,
                           size,
                           size,
                           &(o->rbytes),
                           &(x->overlapped));
        if (!success) {
            if (WSAGetLastError() != WSA_IO_PENDING) {
                failed = 1;
                break;
            }
            accepts++;
        } else {
            /* This really shouldn't ever happen. */
            assert(0);
        }
    }

    if (!accepts)
        CancelThreadpoolIo(c->tp_io);

    if (failed)
        PxSocket_WSAERROR("AcceptEx");

end:
    return;
}

PyObject *
pxsocket_close(PxSocket *s, PyObject *args)
{
    if (s->sock_fd != -1) {
        if (closesocket(s->sock_fd) == SOCKET_ERROR)
            return s->errorhandler();
        s->sock_fd = -1;
    }
    Py_RETURN_NONE;
}

PyObject *
pxsocket_disconnect(PxSocket *s, PyObject *args)
{
/*
    Context *c = ctx;
    PyObject *result;
    BOOL success;
    PTP_WIN32_IO_CALLBACK cb;
    WRITE_LOCK(s);
    if (s->sock_fd == -1) {
        WRITE_UNLOCK(s);
        PyErr_SetString(PyExc_ValueError, "sock_fd == -1");
        return NULL;
    }

    memset((void *)&(c->overlapped), 0, sizeof(WSAOVERLAPPED));

    c = ctx;
    assert(c->io_obj == (PyObject *)s);
    cb = PxSocket_DisconnectCallback;
    c->tp_io = CreateThreadpoolIo((HANDLE)s->sock_fd, cb, c, NULL);
    if (!c->tp_io) {
        WRITE_UNLOCK(s);
        PyErr_SetFromWindowsErr(0);
        return NULL;
    }

    StartThreadpoolIo(c->tp_io);

    success = DisconnectEx(s->sock_fd, &(c->overlapped), 0, 0);
    if (!success) {
        int last_error = WSAGetLastError();
        if (last_error != ERROR_IO_PENDING) {
            CancelThreadpoolIo(c->tp_io);
            PyErr_SetFromWindowsErr(last_error);
            PxSocket_HandleException(c, "DisconnectEx", 1);
            return NULL;
        }
    } else {
        CancelThreadpoolIo(c->tp_io);
        PySys_WriteStdout("DisconnectEx() completed synchronously\n");
        result = Py_True;
    }

    result = Py_None;

done:
    if (!result)
        assert(PyErr_Occurred());
    else {
        assert(result == Py_None || result == Py_True);
        Py_INCREF(result);
    }
    return result;
    */
    return NULL;
}

PyObject *
pxsocket_listen(PxSocket *s, PyObject *args)
{
    Py_RETURN_NONE;
}

PyObject *
pxsocket_write(PxSocket *s, PyObject *args)
{
    PyObject *result = NULL;

    Py_RETURN_NONE;
}


#define _PXSOCKET(n, a) _METHOD(pxsocket, n, a)
#define _PXSOCKET_N(n) _PXSOCKET(n, METH_NOARGS)
#define _PXSOCKET_O(n) _PXSOCKET(n, METH_O)
#define _PXSOCKET_V(n) _PXSOCKET(n, METH_VARARGS)

static PyMethodDef PxSocketMethods[] = {
    _PXSOCKET_N(close),
    { NULL, NULL }
};

#define _MEMBER(n, t, c, f, d) {#n, t, offsetof(c, n), f, d}
#define _PXSOCKETMEM(n, t, f, d)  _MEMBER(n, t, PxSocket, f, d)
#define _PXSOCKET_CB(n)        _PXSOCKETMEM(n, T_OBJECT,    0, #n " callback")
#define _PXSOCKET_ATTR_O(n)    _PXSOCKETMEM(n, T_OBJECT_EX, 0, #n " callback")
#define _PXSOCKET_ATTR_OR(n)   _PXSOCKETMEM(n, T_OBJECT_EX, 1, #n " callback")
#define _PXSOCKET_ATTR_I(n)    _PXSOCKETMEM(n, T_INT,       0, #n " attribute")
#define _PXSOCKET_ATTR_IR(n)   _PXSOCKETMEM(n, T_INT,       1, #n " attribute")
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
    _PXSOCKET_ATTR_IR(sock_backlog),

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
    //_PXSOCKET_ATTR_B(line_mode),
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
    return create_pxsocket(args, kwds, Px_SOCKFLAGS_CLIENT);
}

PyObject *
_async_server(PyObject *self, PyObject *args, PyObject *kwds)
{
    return create_pxsocket(args, kwds, Px_SOCKFLAGS_SERVER);
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
    PyObject *protocol;

    static const char *kwlist[] = { "transport", "protocol", NULL };
    static const char *fmt = "OO:register";

    Py_GUARD

    if (!PyArg_ParseTupleAndKeywords(args, kwds, fmt, (char **)kwlist,
                                     &transport, &protocol))
        return NULL;

    if (PxSocket_Check(transport)) {
        Context *c;
        PxSocket *s = (PxSocket *)transport;
        PTP_SIMPLE_CALLBACK cb;

        c = new_context_for_socket(s);
        if (!c)
            return NULL;

        if (!PxSocket_SetProtocolType(s, protocol))
            return NULL;

        if (PxSocket_IS_CLIENT(s))
            cb = PxSocket_Connect;
        else
            cb = PxSocketServer_Start;

        if (!TrySubmitThreadpoolCallback(cb, c, NULL)) {
            PyErr_SetFromWindowsErr(0);
            return NULL;
        }

        Py_INCREF(transport);
        Py_INCREF(protocol);

        Py_RETURN_NONE;

    } else {
        PyErr_SetString(PyExc_ValueError, "unsupported async object");
        return NULL;
    }
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
    //_ASYNC_V(open),
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
    _ASYNC_O(unprotect),
    _ASYNC_O(protected),
    _ASYNC_N(is_active),
    _ASYNC_V(submit_io),
    _ASYNC_O(read_lock),
    _ASYNC_V(_post_open),
    _ASYNC_V(fileopener),
    _ASYNC_V(filecloser),
    _ASYNC_O(write_lock),
    _ASYNC_V(submit_work),
    _ASYNC_V(submit_wait),
    _ASYNC_O(read_unlock),
    _ASYNC_O(write_unlock),
    _ASYNC_N(is_active_ex),
    _ASYNC_N(active_count),
    _ASYNC_O(_dbg_address),
    _ASYNC_V(submit_timer),
    _ASYNC_O(submit_class),
    _ASYNC_O(submit_client),
    _ASYNC_O(submit_server),
    _ASYNC_O(try_read_lock),
    _ASYNC_O(try_write_lock),
    _ASYNC_V(submit_write_io),
    _ASYNC_V(signal_and_wait),
    _ASYNC_N(active_contexts),
    _ASYNC_N(is_parallel_thread),
    _ASYNC_N(persisted_contexts),
    _ASYNC_V(call_from_main_thread),
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

    m = PyModule_Create(&_asyncmodule);
    if (m == NULL)
        return NULL;

    socket_api = PySocketModule_ImportModuleAndAPI();
    if (!socket_api)
        return NULL;
    PySocketModule = *socket_api;

    if (PyModule_AddObject(m, "socket", (PyObject *)&PxSocket_Type))
        return NULL;

    PyExc_AsyncError = PyErr_NewException("_async.AsyncError", NULL, NULL);
    if (!PyExc_AsyncError)
        return NULL;

    PyExc_ProtectionError = \
        PyErr_NewException("_async.ProtectionError", PyExc_AsyncError, NULL);
    if (!PyExc_ProtectionError)
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

    if (PyModule_AddObject(m, "AsyncError", PyExc_AsyncError))
        return NULL;

    if (PyModule_AddObject(m, "ProtectionError", PyExc_ProtectionError))
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

    Py_INCREF(PyExc_AsyncError);
    Py_INCREF(PyExc_ProtectionError);
    Py_INCREF(PyExc_PersistenceError);
    Py_INCREF(PyExc_NoWaitersError);
    Py_INCREF(PyExc_WaitError);
    Py_INCREF(PyExc_WaitTimeoutError);
    Py_INCREF(PyExc_AsyncIOBuffersExhaustedError);

    /* Uncomment the following (during development) as needed. */
    if (Py_VerboseFlag)
        printf("sizeof(PxSocket): %d\n", sizeof(PxSocket));

    return m;
}

    /*
    l = (PxSocketBufList *)malloc(sizeof(*l));
    if (!l) {
        PyErr_NoMemory();
        PxSocket_EXCEPTION();
    }
    memset(l, 0, sizeof(*l));
    l->wsabufs = (WSABUF **)malloc(sizeof(l->wsabufs));
    if (!l->wsabufs) {
        free(l);
        PyErr_NoMemory();
        PxSocket_EXCEPTION();
    }
    l->flags = NULL;
    l->nbufs = 1;

    w = (WSABUF *)malloc(sizeof(WSABUF));
    if (!w) {
        PyErr_NoMemory();
        PxSocket_EXCEPTION();
    }
    w->len = s->recvbuf_size;
    w->buf = (char *)_aligned_malloc(w->len, MEMORY_ALLOCATION_ALIGNMENT);
    if (!w->buf) {
        free(l->wsabufs);
        free(w);
        PyErr_NoMemory();
        PxSocket_EXCEPTION();
    }

    l->wsabufs[0] = w;*/

#ifdef __cpplus
}
#endif

/* vim:set ts=8 sw=4 sts=4 tw=78 et nospell: */

