from types cimport *

cdef extern from *:
    ctypedef Py_ssize_t HBITMAP

    HBITMAP __stdcall CreateCompatibleBitmap(HDC hdc, int cx, int cy)
    HDC __stdcall CreateCompatibleDC(HDC hdc)
    HGDIOBJ __stdcall SelectObject(HDC hdc, HGDIOBJ h)
    int GetObject(HGDIOBJ hgdiobj, int cbBuffer, LPVOID lpvObject)

    BOOL __stdcall BitBlt(HDC hdc, int x, int y, int cx, int cy, int hdcSrc, int x1, int y1, DWORD rop)
    int SRCCOPY

    BOOL __stdcall DeleteDC(HDC hdc)
    BOOL __stdcall DeleteObject(HGDIOBJ ho)

    int GetDIBits(HDC hdc, HBITMAP hbmp, UINT uStartScan, UINT cScanLines, LPVOID lpvBits, LPBITMAPINFO lpbi, UINT uUsage)
    UINT DIB_RGB_COLORS
    DWORD ERROR_INVALID_PARAMETER

# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:
