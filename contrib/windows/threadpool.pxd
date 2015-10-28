# WinBase.h
from types cimport *

cdef extern from *:

    PTP_POOL __stdcall CreateThreadpool(PVOID reserved)
    ctypedef PTP_POOL (__stdcall *LPFN_CreateThreadpool)(PVOID reserved)

    void SetThreadpoolCallbackPool(
        PTP_CALLBACK_ENVIRON pcbe,
        PTP_POOL             ptpp
    )
    ctypedef void (*LPFN_SetThreadpoolCallbackPool)(
        PTP_CALLBACK_ENVIRON pcbe,
        PTP_POOL             ptpp
    )

    void SetThreadpoolCallbackPriority(
        PTP_CALLBACK_ENVIRON pcbe,
        TP_CALLBACK_PRIORITY Priority
    )
    ctypedef void (*LPFN_SetThreadpoolCallbackPriority)(
        PTP_CALLBACK_ENVIRON pcbe,
        TP_CALLBACK_PRIORITY Priority
    )

    void InitializeThreadpoolEnvironment(
        PTP_CALLBACK_ENVIRON pcbe
    )
    ctypedef void (*LPFN_InitializeThreadpoolEnvironment)(
        PTP_CALLBACK_ENVIRON pcbe
    )

    void DestroyThreadpoolEnvironment(
        PTP_CALLBACK_ENVIRON pcbe
    )
    ctypedef void (*LPFN_DestroyThreadpoolEnvironment)(
        PTP_CALLBACK_ENVIRON pcbe
    )

    PTP_CLEANUP_GROUP __stdcall CreateThreadpoolCleanupGroup()
    ctypedef PTP_CLEANUP_GROUP (__stdcall *LPFN_CreateThreadpoolCleanupGroup())

    void SetThreadpoolCallbackCleanupGroup(
        PTP_CALLBACK_ENVIRON              pcbe,
        PTP_CLEANUP_GROUP                 ptpcg,
        PTP_CLEANUP_GROUP_CANCEL_CALLBACK pfng
    )
    ctypedef void (*LPFN_SetThreadpoolCallbackCleanupGroup)(
        PTP_CALLBACK_ENVIRON              pcbe,
        PTP_CLEANUP_GROUP                 ptpcg,
        PTP_CLEANUP_GROUP_CANCEL_CALLBACK pfng
    )

    void __stdcall CloseThreadpoolCleanupGroup(
        PTP_CLEANUP_GROUP ptpcg
    )
    ctypedef void (__stdcall *LPFN_CloseThreadpoolCleanupGroup)(
        PTP_CLEANUP_GROUP ptpcg
    )

    void __stdcall CloseThreadpoolCleanupGroupMembers(
        PTP_CLEANUP_GROUP ptpcg,
        BOOL              fCancelPendingCallbacks,
        PVOID             pvCleanupContext
    )
    ctypedef void (__stdcall *LPFN_CloseThreadpoolCleanupGroupMembers)(
        PTP_CLEANUP_GROUP ptpcg,
        BOOL              fCancelPendingCallbacks,
        PVOID             pvCleanupContext
    )

    void __stdcall SetThreadpoolThreadMaximum(
        PTP_POOL ptpp,
        DWORD    cthrdMost
    )
    ctypedef void (__stdcall *LPFN_SetThreadpoolThreadMaximum)(
        PTP_POOL ptpp,
        DWORD    cthrdMost
    )

    BOOL __stdcall SetThreadpoolThreadMinimum(
        PTP_POOL ptpp,
        DWORD    cthrdMic
    )
    ctypedef BOOL (__stdcall *LPFN_SetThreadpoolThreadMinimum)(
        PTP_POOL ptpp,
        DWORD    cthrdMic
    )

    void __stdcall CloseThreadpool(PTP_POOL ptpp)
    ctypedef void (__stdcall *LPFN_CloseThreadpool)(PTP_POOL ptpp)

    PTP_IO __stdcall CreateThreadpoolIo(
        HANDLE                fl,
        PTP_WIN32_IO_CALLBACK pfnio,
        PVOID                 pv,
        PTP_CALLBACK_ENVIRON  pcbe
    )
    ctypedef PTP_IO (__stdcall *LPFN_CreateThreadpoolIo)(
        HANDLE                fl,
        PTP_WIN32_IO_CALLBACK pfnio,
        PVOID                 pv,
        PTP_CALLBACK_ENVIRON  pcbe
    )

    void __stdcall StartThreadpoolIo(PTP_IO pio)
    ctypedef void (__stdcall *LPFN_StartThreadpoolIo)(PTP_IO pio)

    void __stdcall CancelThreadpoolIo(PTP_IO pio)
    ctypedef void (__stdcall *LPFN_CancelThreadpoolIo)(PTP_IO pio)

    void __stdcall CloseThreadpoolIo(PTP_IO pio)
    ctypedef void (__stdcall *LPFN_CloseThreadpoolIo)(PTP_IO pio)
    void __stdcall WaitForThreadpoolIoCallbacks(
        PTP_IO pio,
        BOOL   fCancelPendingCallbacks
    )
    ctypedef void (__stdcall *LPFN_WaitForThreadpoolIoCallbacks)(
        PTP_IO pio,
        BOOL   fCancelPendingCallbacks
    )

    PTP_TIMER __stdcall CreateThreadpoolTimer(
        PTP_TIMER_CALLBACK   pfnti,
        PVOID                pv,
        PTP_CALLBACK_ENVIRON pcbe
    )
    ctypedef PTP_TIMER (__stdcall *LPFN_CreateThreadpoolTimer)(
        PTP_TIMER_CALLBACK   pfnti,
        PVOID                pv,
        PTP_CALLBACK_ENVIRON pcbe
    )

    void __stdcall SetThreadpoolTimer(
        PTP_TIMER pti,
        PFILETIME pftDueTime,
        DWORD     msPeriod,
        DWORD     msWindowLength
    )
    ctypedef void (__stdcall *LPFN_SetThreadpoolTimer)(
        PTP_TIMER pti,
        PFILETIME pftDueTime,
        DWORD     msPeriod,
        DWORD     msWindowLength
    )

    BOOL __stdcall IsThreadpoolTimerSet(PTP_TIMER pti)
    ctypedef BOOL (__stdcall *LPFN_IsThreadpoolTimerSet)(PTP_TIMER pti)

    void __stdcall WaitForThreadpoolTimerCallbacks(
        PTP_TIMER pti,
        BOOL      fCancelPendingCallbacks
    )
    ctypedef void (__stdcall *LPFN_WaitForThreadpoolTimerCallbacks)(
        PTP_TIMER pti,
        BOOL      fCancelPendingCallbacks
    )

    void __stdcall CloseThreadpoolTimer(PTP_TIMER pti)
    ctypedef void (__stdcall *LPFN_CloseThreadpoolTimer)(PTP_TIMER pti)

    PTP_WAIT __stdcall CreateThreadpoolWait(
        PTP_WAIT_CALLBACK    pfnwa,
        PVOID                pv,
        PTP_CALLBACK_ENVIRON pcbe
    )
    ctypedef PTP_WAIT (__stdcall *LPFN_CreateThreadpoolWait)(
        PTP_WAIT_CALLBACK    pfnwa,
        PVOID                pv,
        PTP_CALLBACK_ENVIRON pcbe
    )

    void __stdcall SetThreadpoolWait(
        PTP_WAIT  pwa,
        HANDLE    h,
        PFILETIME pftTimeout
    )
    ctypedef void (__stdcall *LPFN_SetThreadpoolWait)(
        PTP_WAIT  pwa,
        HANDLE    h,
        PFILETIME pftTimeout
    )

    BOOL __stdcall SetThreadpoolWaitEx(
        PTP_WAIT  pwa,
        HANDLE    h,
        PFILETIME pftTimeout,
        PVOID     Reserved
    )
    ctypedef BOOL (__stdcall *LPFN_SetThreadpoolWaitEx)(
        PTP_WAIT  pwa,
        HANDLE    h,
        PFILETIME pftTimeout,
        PVOID     Reserved
    )

    void __stdcall CloseThreadpoolWait(PTP_WAIT pwa)
    ctypedef void (__stdcall *LPFN_CloseThreadpoolWait)(PTP_WAIT pwa)

    void __stdcall WaitForThreadpoolWaitCallbacks(
        PTP_WAIT pwa,
        BOOL     fCancelPendingCallbacks
    )
    ctypedef void (__stdcall *LPFN_WaitForThreadpoolWaitCallbacks)(
        PTP_WAIT pwa,
        BOOL     fCancelPendingCallbacks
    )

    PTP_WORK __stdcall CreateThreadpoolWork(
        PTP_WORK_CALLBACK    pfnwk,
        PVOID                pv,
        PTP_CALLBACK_ENVIRON pcbe
    )
    ctypedef PTP_WORK (__stdcall *LPFN_CreateThreadpoolWork)(
        PTP_WORK_CALLBACK    pfnwk,
        PVOID                pv,
        PTP_CALLBACK_ENVIRON pcbe
    )

    void __stdcall SubmitThreadpoolWork(
        PTP_WORK pwk
    )
    ctypedef void (__stdcall *LPFN_SubmitThreadpoolWork)(
        PTP_WORK pwk
    )

    void __stdcall WaitForThreadpoolWorkCallbacks(
        PTP_WORK pwk,
        BOOL     fCancelPendingCallbacks
    )
    ctypedef void (__stdcall *LPFN_WaitForThreadpoolWorkCallbacks)(
        PTP_WORK pwk,
        BOOL     fCancelPendingCallbacks
    )

    void __stdcall CloseThreadpoolWork(
        PTP_WORK pwk
    )
    ctypedef void (__stdcall *LPFN_CloseThreadpoolWork)(
        PTP_WORK pwk
    )

    void SetThreadpoolCallbackLibrary(
        PTP_CALLBACK_ENVIRON pcbe,
        PVOID                mod
    )
    ctypedef void (*LPFN_SetThreadpoolCallbackLibrary)(
        PTP_CALLBACK_ENVIRON pcbe,
        PVOID                mod
    )

    void SetThreadpoolCallbackRunsLong(
        PTP_CALLBACK_ENVIRON pcbe
    )
    ctypedef void (*LPFN_SetThreadpoolCallbackRunsLong)(
        PTP_CALLBACK_ENVIRON pcbe
    )

    BOOL __stdcall CallbackMayRunLong(
        PTP_CALLBACK_INSTANCE pci
    )
    ctypedef BOOL (__stdcall *LPFN_CallbackMayRunLong)(
        PTP_CALLBACK_INSTANCE pci
    )

    void __stdcall ReleaseSemaphoreWhenCallbackReturns(
        PTP_CALLBACK_INSTANCE pci,
        HANDLE                sem,
        DWORD                 crel
    )
    ctypedef void (__stdcall *LPFN_ReleaseSemaphoreWhenCallbackReturns)(
        PTP_CALLBACK_INSTANCE pci,
        HANDLE                sem,
        DWORD                 crel
    )

    void __stdcall SetEventWhenCallbackReturns(
        PTP_CALLBACK_INSTANCE pci,
        HANDLE                evt
    )
    ctypedef void (__stdcall *LPFN_SetEventWhenCallbackReturns)(
        PTP_CALLBACK_INSTANCE pci,
        HANDLE                evt
    )

    void __stdcall LeaveCriticalSectionWhenCallbackReturns(
        PTP_CALLBACK_INSTANCE pci,
        PCRITICAL_SECTION     pcs
    )
    ctypedef void (__stdcall *LPFN_LeaveCriticalSectionWhenCallbackReturns)(
        PTP_CALLBACK_INSTANCE pci,
        PCRITICAL_SECTION     pcs
    )

cdef class Threadpool:
    cdef:
        PTP_CALLBACK_ENVIRON pcbe

    cdef PTP_IO create_threadpool_io(
        HANDLE fl,
        PTP_WIN32_IO_CALLBACK pfnio,
        PVOID pv
    )

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
