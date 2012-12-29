#ifndef PXLIST_H
#define PXLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WITH_PARALLEL

#include "pyerrors.h" /* for Py_FatalError */

//#include <windows.h>

#undef E2I
#undef I2E

#define E2I(p) ((PxListItem  *)p)
#define I2E(p) ((PxListEntry *)p)

#define PxListHead  SLIST_HEADER
#define PxListEntry SLIST_ENTRY

/* Fill up 64-bytes. */
#ifndef _WIN64
typedef struct _PxListItem32 {
    __declspec(align(16)) PxListEntry entry;
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
    PxListEntry   entry; /* aligned to 16-bytes */
    unsigned __int64 when;
    void         *from;
    void         *p1;
    void         *p2;
    void         *p3;
    void         *p4;
} PxListItem64;
typedef struct _PxListItem64 PxListItem;
#endif

C_ASSERT(sizeof(PxListItem) == 64);

static __inline
void *
PxList_Malloc(Py_ssize_t size)
{
    return _aligned_malloc(size, MEMORY_ALLOCATION_ALIGNMENT);
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
PxList_Next(PxListItem *item)
{
    return E2I(item->entry.Next);
}

static __inline
PxListItem *
PxList_SeverNext(PxListItem *item)
{
    register PxListItem *next = E2I(item->entry.Next);
    item->entry.Next = NULL;
    return next;
}

static __inline
PxListItem *
PxList_Transfer(PxListHead *head, PxListItem *item)
{
    register PxListItem *next = E2I(item->entry.Next);
    item->entry.Next = NULL;
    PxList_Push(head, item);
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
PxList_Flush(PxListHead *head, unsigned short *depth_hint)
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
    return E2I(InterlockedPushEntrySList(head, I2E(&item->entry)));
}

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
PxList_Pop(PxListHead *head, PxListItem *item)
{
    return E2I(InterlockedPopEntrySList(head));
}

#else

#endif /* WITH_PARALLEL */
#ifdef __cplusplus
}
#endif
#endif

/* vim:set ts=8 sw=4 sts=4 tw=78 et: */
