
#include <pxlist.h>

#ifdef _WIN32

#include <Windows.h>
#pragma comment(lib, "ws2_32.lib")

#endif /* _WIN32 */

#ifdef WITH_PARALLEL

#undef E2I
#undef I2E

#define E2I(p) ((PxListItem  *)p)
#define I2E(p) ((PxListEntry *)p)
#define O2E(o) ((PxListEntry *)(&(((PyObject *)(o))->slist_entry)))
#define I2O(i) (_Py_CAST_BACK(i, PyObject *, PyObject, slist_entry))
#define I2C(i) (_Py_CAST_BACK(i, Context *, Context, slist_entry))
#define C2E(i) (_Py_CAST_FWD(i, PxListEntry *, Context, slist_entry))
#define C2I(i) (_Py_CAST_FWD(i, PxListItem *, Context, slist_entry))

#define PxListHead  SLIST_HEADER
#define PxListEntry SLIST_ENTRY

#define PxListItem_SIZE 64

/* Fill up 64-bytes. */
#ifndef _WIN64
typedef struct _PxListItem32 {
    __declspec(align(16)) PxListEntry slist_entry;
    __declspec(align(8))  __int64  when;
    __declspec(align(8))  void    *from;
    __declspec(align(8))  void    *p1;
    __declspec(align(8))  void    *p2;
    __declspec(align(8))  void    *p3;
    __declspec(align(8))  void    *p4;
} PxListItem32;
typedef struct _PxListItem32 PxListItem;

#else
typedef struct _PxListItem64 {
    PxListEntry   slist_entry; /* aligned to 16-bytes */
    unsigned __int64 when;
    void         *from;
    void         *p1;
    void         *p2;
    void         *p3;
    void         *p4;
} PxListItem64;
typedef struct _PxListItem64 PxListItem;
#endif

C_ASSERT(sizeof(PxListItem) == PxListItem_SIZE);

static __inline
PxListItem *
PxList_Next(PxListItem *item)
{
    return E2I(item->slist_entry.Next);
}

static __inline
void *
PxList_Malloc(Py_ssize_t size)
{
    register void *p;
    p = _aligned_malloc(size, MEMORY_ALLOCATION_ALIGNMENT);
    if (p)
        memset(p, 0, size);
    return p;
}

static __inline
void *
PxList_MallocFromHeap(HANDLE heap_handle, Py_ssize_t size)
{
    register void *p;
    Py_ssize_t aligned = _Py_SIZE_ROUND_UP(size, MEMORY_ALLOCATION_ALIGNMENT);
    p = HeapAlloc(heap_handle, HEAP_ZERO_MEMORY, aligned);
    if (!p)
        PyErr_SetFromWindowsErr(0);
    return p;
}

static __inline
void
PxList_Free(void *p)
{
    if (!p)
        return;
    _aligned_free(p);
}

static __inline
PxListHead *
PxList_New(void)
{
    PxListHead *l = (PxListHead *)PxList_Malloc(sizeof(PxListHead));
    if (!l)
        return NULL;

    InitializeSListHead(l);
    return l;
}

static __inline
PxListHead *
PxList_NewFromHeap(HANDLE heap_handle)
{
    PxListHead *l = (PxListHead *)PxList_MallocFromHeap(heap_handle,
                                                        sizeof(PxListHead));
    if (!l)
        return NULL;

    InitializeSListHead(l);
    return l;
}

static __inline
void
PxList_TimestampItem(PxListItem *item)
{
    item->when = _Py_rdtsc();
}

static __inline
PxListItem *
PxList_NewItem(void)
{
    return E2I(PxList_Malloc(sizeof(PxListItem)));
}

static __inline
void
PxList_FreeListHead(PxListHead *head)
{
    /* xxx todo: manage a list of free item lists */
    PxList_Free(head);
}

static __inline
void
PxList_FreeListItem(PxListItem *item)
{
    /* xxx todo: manage a list of free item lists */
    assert(PxList_Next(item) == NULL);
    PxList_Free(item);
}

static __inline
PxListItem *
PxList_FreeListItemAfterNext(PxListItem *item)
{
    PxListItem *next = PxList_Next(item);
    PxList_FreeListItem(item);
    return next;
}

static __inline
PxListItem *
PxList_SeverFromNext(PxListItem *item)
{
    register PxListItem *next = E2I(item->slist_entry.Next);
    item->slist_entry.Next = NULL;
    return next;
}

static __inline
unsigned short
PxList_QueryDepth(PxListHead *head)
{
    return QueryDepthSList(head);
}

static __inline
PxListItem *
PxList_FlushWithDepthHint(PxListHead *head,
                          unsigned short *depth_hint)
{
    if (depth_hint)
        *depth_hint = QueryDepthSList(head);
    return E2I(InterlockedFlushSList(head));
}

static __inline
PxListItem *
PxList_Flush(PxListHead *head)
{
    return E2I(InterlockedFlushSList(head));
}

static __inline
void
PxList_Clear(PxListHead *head)
{
    register PxListItem *item;

    if (!head)
        return;

    if (QueryDepthSList(head) == 0)
        return;

    item = E2I(InterlockedFlushSList(head));

    do {
        PxList_FreeListItem(item);
    } while (item = PxList_Next(item));

    return;
}

static __inline
void
PxList_FreeAllListItems(PxListItem *start)
{
    register PxListItem *item = start;
    do {
        PxList_FreeListItem(item);
    } while (item = PxList_Next(item));
}

__inline
size_t
PxList_CountItems(PxListItem *start)
{
    register PxListItem *item = start;
    size_t i = 0;

    if (!item)
        return 0;

    do {
        ++i;
    } while (item = PxList_Next(item));

    return i;
}

static __inline
void
PxList_Delete(PxListHead *head)
{
    PxList_Free(head);
}

static __inline
void
PxList_FreeList(PxListHead *head)
{
    PxList_Free(head);
}

static __inline
PxListItem *
PxList_Push(PxListHead *head, PxListItem *item)
{
    return E2I(InterlockedPushEntrySList(head, I2E(&item->slist_entry)));
}

static __inline
void
PxList_PushObject(PxListHead *head, PyObject *op)
{
    InterlockedPushEntrySList(head, O2E(op));
}

/*
#define PxList_PushObject(h, o) (I2O(PxList_Push((PxListHead *)(h), O2E((o)))))
*/
#define PxList_PushContext(h, c) (PxList_Push((PxListHead *)(h), C2I((c))))

static __inline
PxListItem *
PxList_Transfer(PxListHead *head, PxListItem *item)
{
    register PxListItem *next = E2I(item->slist_entry.Next);
    item->slist_entry.Next = NULL;
    PxList_Push(head, item);
    return next;
}
#define PxList_TransferObject(h, o) (I2O(PxList_Transfer(h, O2I(o))))

#if (Py_NTDDI >= 0x06020000)
static __inline
PxListItem *
PxList_PushList(PxListHead *head,
                PxListItem *start,
                PxListItem *end,
                unsigned long count)
{
    return E2I(InterlockedPushListSList(head, I2E(start), I2E(end), count));
}
#endif

static __inline
PxListItem *
PxList_Pop(PxListHead *head)
{
    return E2I(InterlockedPopEntrySList(head));
}
#define PxList_PopObject(h) (I2O(PxList_Pop(h)))

#else

#endif /* WITH_PARALLEL */
#ifdef __cplusplus
}
#endif
#endif

/* vim:set ts=8 sw=4 sts=4 tw=78 et: */
