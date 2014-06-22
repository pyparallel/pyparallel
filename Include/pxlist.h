#ifndef PXLIST_H
#define PXLIST_H

#include "pyport.h"
#include "pyparallel.h"
#include "object.h"

#ifdef _WIN32

#include <Windows.h>

#endif

#if defined(__APPLE__) && defined(__MACH__)

#define USE_MACOSX_SLIST

#include <libkern/OSAtomic.h>

#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WITH_PARALLEL

#undef E2I

#define E2I(p) ((PxListItem  *)p)
#define I2O(i) (_Py_CAST_BACK(i, PyObject *, PyObject, slist_entry))
#define I2C(i) (_Py_CAST_BACK(i, Context *, Context, slist_entry))
#define C2E(i) (_Py_CAST_FWD(i, PxListEntry *, Context, slist_entry))
#define C2I(i) (_Py_CAST_FWD(i, PxListItem *, Context, slist_entry))

#ifdef _WIN32
        typedef SLIST_HEADER            PxListHead;
        typedef SLIST_ENTRY             PxListEntry;
        typedef HANDLE                  PxHeapHandle;
#elif defined(USE_MACOSX_SLIST)
        typedef OSFifoQueueHead         PxListHead;
        typedef void *                  PxListEntry;
        typedef struct PxHeapHandle     PxHeapHandle;
#endif

#define PxListItem_SIZE 64

/* Fill up 64-bytes. */
typedef struct _PxListItem {
    __declspec(align(16)) PxListEntry slist_entry;
    __declspec(align(8))  __int64  when;
    __declspec(align(8))  void    *from;
    __declspec(align(8))  void    *p1;
    __declspec(align(8))  void    *p2;
    __declspec(align(8))  void    *p3;
    __declspec(align(8))  void    *p4;
} _PxListItem;

typedef struct _PxListItem PxListItem;

PxListItem *    PxList_Next(PxListItem *item);

void *          PxList_Malloc(Py_ssize_t size);

void *          PxList_MallocFromHeap(PxHeapHandle heap_handle,
                                      Py_ssize_t   size);

void            PxList_Free(void *p);

PxListHead *    PxList_New(void);

PxListHead *    PxList_NewFromHeap(PxHeapHandle heap_handle);

void            PxList_TimestampItem(PxListItem *item);

PxListItem *    PxList_NewItem(void);

void            PxList_FreeListHead(PxListHead *head);

void            PxList_FreeListItem(PxListItem *item);

PxListItem *    PxList_FreeListItemAfterNext(PxListItem *item);

PxListItem *    PxList_SeverFromNext(PxListItem *item);

unsigned short  PxList_QueryDepth(PxListHead *head);

PxListItem *    PxList_FlushWithDepthHint(PxListHead *head,
                                          unsigned short *depth_hint);

PxListItem *    PxList_Flush(PxListHead *head);

void            PxList_Clear(PxListHead *head);

void            PxList_FreeAllListItems(PxListItem *start);

size_t          PxList_CountItems(PxListItem *start);

void            PxList_Delete(PxListHead *head);

void            PxList_FreeList(PxListHead *head);

PxListItem *    PxList_Push(PxListHead *head, PxListItem *item);

void            PxList_PushObject(PxListHead *head, PyObject *op);

#define PxList_PushContext(h, c) (PxList_Push((PxListHead *)(h), C2I((c))))

PxListItem *    PxList_Transfer(PxListHead *head, PxListItem *item);

#define PxList_TransferObject(h, o) (I2O(PxList_Transfer(h, O2I(o))))

#if (Py_NTDDI >= 0x06020000)
PxListItem *    PxList_PushList(PxListHead *head,
                                PxListItem *start,
                                PxListItem *end,
                                unsigned long count);
#endif /* Py_NTDDI */

PxListItem *    PxList_Pop(PxListHead *head);

#define PxList_PopObject(h) (I2O(PxList_Pop(h)))

#else  /* WITH_PARALLEL */

#endif /* WITH_PARALLEL */
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* PXLIST_H */

/* vim:set ts=8 sw=4 sts=4 tw=78 et: */
