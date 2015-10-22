from types cimport *

cdef extern from *:
    ctypedef void* HBITMAP
    ctypedef void* HPALETTE

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
    ctypedef Status GpStatus

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

    cdef cppclass EncoderParameter:
        GUID    Guid
        ULONG   NumberOfValues
        ULONG   Type
        PVOID   Value

    cdef cppclass EncoderParameters:
        UINT Count
        EncoderParameter Parameter[1]

    GUID EncoderQuality

    ctypedef enum EncoderParameterValueType:
        EncoderParameterValueTypeByte           = 1
        EncoderParameterValueTypeASCII          = 2
        EncoderParameterValueTypeShort          = 3
        EncoderParameterValueTypeLong           = 4
        EncoderParameterValueTypeRational       = 5
        EncoderParameterValueTypeLongRange      = 6
        EncoderParameterValueTypeUndefined      = 7
        EncoderParameterValueTypeRationalRange  = 8
        EncoderParameterValueTypePointer        = 9

    cdef cppclass GpImage:
        pass

    cdef cppclass GpBitmap(GpImage):
        pass

    cdef cppclass IStream:
        pass


cdef extern from * namespace "Gdiplus::DllExports":
    GpStatus GdipCreateBitmapFromHBITMAP(HBITMAP hbm, HPALETTE hpal, GpBitmap** bitmap)
    GpStatus GdipSaveImageToFile(GpImage *image, const WCHAR* filename, CLSID* clsidEncoder, EncoderParameters* encoderParams)

cdef extern from "_gdiplus.h":
    ULONG_PTR gdiplus_startup()
    void gdiplus_shutdown()
    CLSID ImageEncoderBmpClsid
    CLSID ImageEncoderJpegClsid
    CLSID ImageEncoderGifClsid
    CLSID ImageEncoderTiffClsid
    CLSID ImageEncoderPngClsid

#cdef void save_bitmap_as(void *handle, CLSID* encoder, LPCWSTR filename, ULONG quality)

#cdef void save_bitmap_as_jpeg(void *handle, LPCWSTR filename, ULONG quality)
#cdef void save_bitmap_as_png(void *handle, LPCWSTR filename, ULONG quality)
cpdef void save_bitmap(Py_ssize_t handle, unicode filename, ULONG quality)

# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:
