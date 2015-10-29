
from cpython.mem cimport PyMem_Malloc, PyMem_Free

cdef extern from *:
    void __debugbreak()
    object PyErr_NoMemory()

    ctypedef struct PyBytesObject:
        Py_ssize_t ob_size
        char ob_sval[1]

    PyBytesObject* PyBytes_New(Py_ssize_t size)

cdef extern from "../../Include/unicodeobject.h":
    char* PyUnicode_AsUTF8AndSize(object, Py_ssize_t *)

include "windows.pxi"
from constants cimport *
from base cimport *
from file cimport *
from user cimport *
from gdi cimport *
from string cimport *

DEF MAX_FILE_NAME = 1024

cdef class Screenshot:
    cdef readonly:
        BITMAPFILEHEADER header
        BITMAPINFOHEADER info
        PCHAR buf
        DWORD bitmap_size
        DWORD dib_size
        BITMAP bitmap
        HBITMAP handle
        HDC screen
        HDC memory

    def __cinit__(self):
        cdef:
            int width = GetSystemMetrics(SM_CXSCREEN)
            int height = GetSystemMetrics(SM_CYSCREEN)
            WORD bit_count = 32

        SecureZeroMemory(&self.info, sizeof(self.info))
        SecureZeroMemory(&self.header, sizeof(self.header))
        SecureZeroMemory(&self.bitmap, sizeof(self.bitmap))

        self.screen = 0
        self.memory = 0
        self.handle = 0

        self.screen = GetDC(<HWND>NULL)
        if not self.screen:
            raise RuntimeError("GetDC")

        self.memory = CreateCompatibleDC(self.screen)
        if not self.memory:
            raise RuntimeError("CreateCompatibleDC")

        self.handle = CreateCompatibleBitmap(self.screen, width, height)
        if not self.handle:
            raise RuntimeError("CreateCompatibleBitmap")

        self.info.biSize = sizeof(BITMAPINFOHEADER)
        self.info.biWidth = width
        self.info.biHeight = height
        self.info.biPlanes = 1
        self.info.biBitCount = bit_count
        self.info.biCompression = BI_RGB

        self.bitmap_size = (((width * bit_count + 31) // 32) * 4 * height)
        self.buf = <PCHAR>PyMem_Malloc(self.bitmap_size)

        self.dib_size = (
            self.bitmap_size +
            sizeof(BITMAPFILEHEADER) +
            sizeof(BITMAPINFOHEADER)
        )

        self.header.bfOffBits = (
            <DWORD>sizeof(BITMAPFILEHEADER) +
            <DWORD>sizeof(BITMAPINFOHEADER)
        )

        self.header.bfSize = self.dib_size
        self.header.bfType = 0x4d42 # BM
        self.header.bfReserved1 = 0
        self.header.bfReserved2 = 0

        self.refresh()

    property width:
        def __get__(self):
            return self.info.biWidth

    property height:
        def __get__(self):
            return self.info.biHeight

    cpdef void refresh(self):
        cdef:
            BOOL result
            DWORD retval

        SelectObject(self.memory, self.handle)

        result = BitBlt(self.memory,
                        0, 0,
                        self.width,
                        self.height,
                        self.screen,
                        0, 0,
                        SRCCOPY)
        if not result:
            raise RuntimeError("BitBlt")

        GetObject(self.handle, sizeof(BITMAP), <LPVOID>&self.bitmap)

        retval = GetDIBits(self.memory,
                           self.handle,
                           0,
                           <UINT>self.height,
                           <LPVOID>self.buf,
                           <LPBITMAPINFO>&self.info,
                           DIB_RGB_COLORS)
        if not retval:
            raise RuntimeError("GetDIBits")
        if retval == ERROR_INVALID_PARAMETER:
            raise RuntimeError("GetDIBits: Invalid Parameter")

    def __len__(self):
        return self.dib_size

    def __dealloc__(self):
        if self.screen:
            ReleaseDC(<HWND>NULL, self.screen)

        if self.memory:
            DeleteObject(self.memory)

        if self.handle:
            DeleteObject(self.handle)

        if self.buf:
            PyMem_Free(self.buf)

    cpdef void copy_to_clipboard(self):
        OpenClipboard(<HWND>NULL)
        EmptyClipboard()
        SetClipboardData(CF_BITMAP, self.handle)
        CloseClipboard()

    # If buf is null or buf_size is less than the required size, the required
    # size is written into bytes_written and 0 is returned.  Otherwise, 1 is
    # returned and the number of bytes is written into bytes_written.
    cdef BOOLEAN copy(self, PCHAR buf, DWORD buf_size, PDWORD bytes_written):
        cdef:
            DWORD header_size = sizeof(self.header)
            DWORD info_size = sizeof(self.info)
            DWORD bitmap_offset = header_size + info_size

        if not buf or buf_size < self.dib_size:
            bytes_written[0] = self.dib_size
            return 0

        CopyMemory(<PVOID>&buf[0],
                   <const void *>&self.header,
                   header_size)

        CopyMemory(<PVOID>&buf[header_size],
                   <const void *>&self.info,
                   info_size)

        CopyMemory(<PVOID>&buf[bitmap_offset],
                   <const void *>self.buf,
                   self.bitmap_size)

        assert(bitmap_offset + self.bitmap_size == self.dib_size)
        bytes_written[0] = self.dib_size
        return 1

    cpdef object __bytes__(self):
        cdef:
            DWORD bytes_written
            PyBytesObject *obj

        obj = PyBytes_New(self.dib_size)
        if not obj:
            return PyErr_NoMemory()

        self.copy(<PCHAR>&obj.ob_sval, self.dib_size, &bytes_written)
        return <object>obj

    cpdef DWORD save(self, unicode filename):
        cdef:
            HANDLE h
            DWORD total_bytes_written = 0
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

        h = CreateFileW(<LPCTSTR>&uname[0],
                       GENERIC_WRITE,
                       0,
                       <LPSECURITY_ATTRIBUTES>NULL,
                       CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL,
                       <HANDLE>NULL)
        if not h:
            raise OSError("CreateFile")

        try:
            total_bytes_written = self.write(h)
        finally:
            CloseHandle(h)

        return total_bytes_written

    cpdef DWORD write(self, HANDLE handle):
        cdef:
            DWORD bytes_written = 0
            DWORD total_bytes_written = 0
            DWORD expected_bytes_written = 0

        WriteFile(handle,
                  <LPSTR>&self.header,
                  sizeof(self.header),
                  &bytes_written,
                  <LPOVERLAPPED>NULL)

        total_bytes_written = bytes_written

        WriteFile(handle,
                  <LPSTR>&self.info,
                  sizeof(self.info),
                  &bytes_written,
                  <LPOVERLAPPED>NULL)

        total_bytes_written += bytes_written

        WriteFile(handle,
                  <LPSTR>self.buf,
                  self.bitmap_size,
                  &bytes_written,
                  <LPOVERLAPPED>NULL)

        total_bytes_written += bytes_written
        expected_bytes_written = self.header.bfOffBits + self.bitmap_size
        if total_bytes_written != expected_bytes_written:
            msg = "Save failed: expected: %d, actual: %d" % (
                expected_bytes_written,
                total_bytes_written,
            )
            raise RuntimeError(msg)

        return total_bytes_written


# vim: set ts=8 sw=4 sts=4 tw=80 et:
