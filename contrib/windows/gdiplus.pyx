
include "windows.pxi"

cdef extern from "../../Include/unicodeobject.h":
    char* PyUnicode_AsUTF8AndSize(object, Py_ssize_t *)

from gdiplus cimport *
from string cimport *

DEF MAX_FILE_NAME = 1024

cpdef ULONG_PTR startup():
    return gdiplus_startup()

cpdef void shutdown():
    gdiplus_shutdown()

cdef void save_bitmap_as(HBITMAP handle, CLSID* encoder,
                         LPCWSTR filename, ULONG quality):
    cdef:
        GpStatus status
        GpBitmap* bitmap
        EncoderParameters params

    status = GdipCreateBitmapFromHBITMAP(<HBITMAP>handle,
                                         <HPALETTE>NULL,
                                         &bitmap)
    if status:
        raise RuntimeError("GdipCreateBitmapFromHBITMAP(%d)" % status)

    params.Count = 1
    params.Parameter[0].NumberOfValues = 1
    params.Parameter[0].Guid  = EncoderQuality
    params.Parameter[0].Type  = EncoderParameterValueTypeLong
    params.Parameter[0].Value = <PVOID>&quality

    status = GdipSaveImageToFile(bitmap, filename, encoder, &params)
    if status:
        raise RuntimeError("GdipSaveImageToFile(%d)" % status)

cdef void save_bitmap_as_jpeg(HBITMAP handle,
                              LPCWSTR filename,
                              ULONG quality):
    cdef CLSID* encoder = &ImageEncoderJpegClsid
    save_bitmap_as(handle, encoder, filename, quality)

cdef void save_bitmap_as_png(HBITMAP handle,
                             LPCWSTR filename,
                             ULONG quality):
    cdef CLSID* encoder = &ImageEncoderPngClsid
    save_bitmap_as(handle, encoder, filename, quality)

cpdef void save_bitmap(Py_ssize_t handle, unicode filename, ULONG quality):
    cdef:
        WCHAR uname[MAX_FILE_NAME+1]
        int uname_len
        char *name
        Py_ssize_t name_len

    name = PyUnicode_AsUTF8AndSize(filename, &name_len)

    uname_len = MultiByteToWideChar(CP_UTF8,
                                    0,
                                    <LPCSTR>name,
                                    name_len,
                                    &uname[0],
                                    MAX_FILE_NAME)
    if uname_len == 0:
        raise ValueError("save(): MultiByteToWideChar failed")

    uname[uname_len] = 0

    if filename.endswith(".png"):
        save_bitmap_as_png(<HBITMAP>handle, &uname[0], quality)
    elif filename.endswith(".jpg"):
        save_bitmap_as_jpeg(<HBITMAP>handle, &uname[0], quality)
    else:
        raise ValueError("Unknown file extension")

# vim: set ts=8 sw=4 sts=4 tw=80 et:
