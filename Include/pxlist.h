#ifndef PXLIST_H
#define PXLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WITH_PARALLEL

#include "pyerrors.h" /* for Py_FatalError */

#include <windows.h>

#define PxListHead  SLIST_HEADER
#define PxListEntry SLIST_ENTRY

#undef E2I
#undef I2E

#define E2I(p) ((PxListItem  *)p)
#define I2E(p) ((PxListEntry *)p)

/* Fill up 64-bytes. */
typedef struct _PxListItem32 {  /* sizeof   total   remaining   */
    PxListEntry   entry;        /*      8       8          56   */
    __declspec(align(8))
    void         *from;         /*      8      16          48   */
    __declspec(align(8))
    void         *p1;           /*      8      24          40   */
    __declspec(align(8))
    void         *p2;           /*      8      32          32   */
    __declspec(align(8))
    void         *p3;           /*      8      40          24   */
    __declspec(align(8))
    void         *p4;           /*      8      48          16   */
    __declspec(align(8))
    void         *p5;           /*      8      56           8   */
    __declspec(align(8))
    void         *p6;           /*      8      64           0   */
} PxListItem32;

typedef struct _PxListItem64 {  /* sizeof   total   remaining   */
    PxListEntry   entry;        /*      8       8          56   */
    void         *from;         /*      8      16          48   */
    void         *p1;           /*      8      24          40   */
    void         *p2;           /*      8      32          32   */
    void         *p3;           /*      8      40          24   */
    void         *p4;           /*      8      48          16   */
    void         *p5;           /*      8      56           8   */
    void         *p6;           /*      8      64           0   */
} PxListItem64;

#ifdef _WIN64
typedef struct _PxListItem64 PxListItem;
#else
typedef struct _PxListItem32 PxListItem;
#endif

static __inline
void *
PxList_Malloc(Py_ssize_t size)
{
    register void *p = _aligned_malloc(size, MEMORY_ALLOCATION_ALIGNMENT);
    if (!p)
        Py_FatalError("PxList_Malloc:_aligned_malloc");
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
    InitializeSListHead(l);
    return l;
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
PxList_Next(PxListItem *item)
{
    return E2I(item->entry.Next);
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

#if (Py_NTDDI >= 0x0602)
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
