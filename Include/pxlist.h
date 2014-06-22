#ifndef PXLIST_H
#define PXLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WITH_PARALLEL

#ifdef _WIN32
	typedef HANDLE			        PxHeapHandle;
#else
	typedef struct PxHeapHandle     PxHeapHandle;
#endif

typedef struct PxListHead       PxListHead;
typedef struct PxListEntry      PxListEntry;
typedef struct PxListItem       PxListItem;
typedef struct PxHeapHandle     PxHeapHandle;

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

/*
#define PxList_PushObject(h, o) (I2O(PxList_Push((PxListHead *)(h), O2E((o)))))
*/
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
