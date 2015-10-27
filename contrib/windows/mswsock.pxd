# Mswsock.h
from types cimport *

cdef extern from *:

    RIO_BUFFERID RIORegisterBuffer(
        PCHAR DataBuffer,
        DWORD DataLength
    )

    void RIODeregisterBuffer(RIO_BUFFERID BufferId)

    RIO_RQ RIOCreateRequestQueue(
        SOCKET Socket,
        ULONG MaxOutstandingReceive,
        ULONG MaxReceiveDataBuffers,
        ULONG MaxOutstandingSend,
        ULONG MaxSendDataBuffers
        RIO_CQ ReceiveCQ,
        RIO_CQ SendCQ,
        PVOID SocketContext
    )

    BOOL RIOResizeRequestQueue(
        RIO_RQ RQ,
        DWORD MaxOutstandingReceive,
        DWORD MaxOutstandingSend
    )

    RIO_CQ RIOCreateCompletionQueue(
        DWORD QueueSize,
        RIO_NOTIFICATION_COMPLETION NotificationCompletion
    )

    BOOL RIOResizeCompletionQueue(
        RIO_CQ CQ,
        DWORD QueueSize
    )

    void RIOCloseCompletionQueue(RIO_CQ CQ)

    BOOL RIOSend(
        RIO_RQ SocketQueue,
        PRIO_BUF pData,
        ULONG DataBufferCount,
        DWORD Flags,
        PVOID RequestContext
    )

    BOOL RIOSendEx(
        RIO_RQ SocketQueue,
        PRIO_BUF pData,
        ULONG DataBufferCount,
        PRIO_BUF pLocalAddress,
        PRIO_BUF pRemoteAddress,
        PRIO_BUF pControlContext,
        PRIO_BUF pFlags,
        DWORD Flags,
        PVOID RequestContext
    )

    BOOL RIOReceive(
        RIO_RQ SocketQueue,
        PRIO_BUF pData,
        ULONG DataBufferCount,
        DWORD Flags,
        PVOID RequestContext
    )

    BOOL RIOReceiveEx(
        RIO_RQ SocketQueue,
        PRIO_BUF pData,
        ULONG DataBufferCount,
        PRIO_BUF pLocalAddress,
        PRIO_BUF pRemoteAddress,
        PRIO_BUF pControlContext,
        PRIO_BUF pFlags,
        DWORD Flags,
        PVOID RequestContext
    )

    INT RIONotify(RIO_CQ CQ)

    ULONG RIODequeueCompletion(
        RIO_CQ CQ,
        PRIORESULT Array,
        ULONG ArraySize
    )

    void RIOCloseCompletionQueue(RIO_CQ CQ)


# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
