from types cimport *

cdef extern from "<windows.h>":
    HBITMAP __stdcall CreateCompatibleBitmap(HDC hdc, int cx, int cy)
    HDC __stdcall CreateCompatibleDC(HDC hdc)
    HDIOBJ __stdcall SelectObject(HDC hdc, HGDIOBJ h)

    BOOL __stdcall BitBlt(HDC hdc, int x, int y, int cx, int cy, int hdcSrc, int x1, int y1, DWORD rop)

    BOOL __stdcall DeleteDC(HDC hdc)
    BOOL __stdcall DeleteObject(HGDIOBJ ho)
