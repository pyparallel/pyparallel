from types cimport *

cdef extern from "<windows.h>":

    HDC __stdcall GetWindowDC(HWND hWnd)
    int __stdcall ReleaseDC(HWND hWnd, HDC hDC)

    HWND __stdcall WindowFromDC(HDC hDC)
    HDC __stdcall GetDC(HWND hWnd)
    HDC __stdcall GetDCEx(HWND hWnd, HRGN hrgnClip, DWORD flags)

    BOOL PrintWindow(HWND hwnd, HDC hdcBlt, UINT nFlags)
    ctypedef UINT PW_CLIENTONLY = 1
