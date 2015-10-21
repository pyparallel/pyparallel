from types cimport *

cdef extern from * namespace "Gdiplus":

    ctypedef enum DebugEventLevel:
        DebugEventLevelFatal
        DebugEventLevelWarning

    ctypedef enum Status:
        Ok                          = 0
        GenericError                = 1
        InvalidParameter            = 2
        OutOfMemory                 = 3
        ObjectBusy                  = 4
        InsufficientBuffer          = 5
        NotImplemented              = 6
        Win32Error                  = 7
        WrongState                  = 8
        Aborted                     = 9
        FileNotFound                = 10
        ValueOverflow               = 11
        AccessDenied                = 12
        UnknownImageFormat          = 13
        FontFamilyNotFound          = 14
        FontStyleNotFound           = 15
        NotTrueTypeFont             = 16
        UnsupportedGdiplusVersion   = 17
        GdiplusNotInitialized       = 18
        PropertyNotFound            = 19
        PropertyNotSupported        = 20
        ProfileNotFound             = 21

    ctypedef void (__stdcall *DebugEventProc)(DebugEventLevel level, CHAR *message)
    ctypedef Status (__stdcall *NotificationHookProc)(ULONG_PTR *token)
    ctypedef Status (__stdcall *NotificationUnhookProc)(ULONG_PTR *token)

    ctypedef struct GdiplusStartupInput:
        UINT32          GdiplusVersion
        DebugEventProc  DebugEventCallback
        BOOL            SuppressBackgroundThread
        BOOL            SuppressExternalCodecs

    ctypedef struct GdiplusStartupOutput:
        NotificationHookProc    NotificationHook
        NotificationUnhookProc  NotificationUnhook

    Status GdiplusStartup(ULONG_PTR *token, const GdiplusStartupInput *input, GdiplusStartupOutput *output)

    void GdiplusShutdown(ULONG_PTR token)

cdef extern from "_gdiplus.h":
    ULONG_PTR gdiplus_startup()
    void gdiplus_shutdown()

# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:
