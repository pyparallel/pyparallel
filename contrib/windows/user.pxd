from types cimport *

cdef extern from *:

    HDC __stdcall GetWindowDC(HWND hWnd)
    int __stdcall ReleaseDC(HWND hWnd, HDC hDC)

    HWND __stdcall WindowFromDC(HDC hDC)
    HDC __stdcall GetDC(HWND hWnd)
    HDC __stdcall GetDCEx(HWND hWnd, HRGN hrgnClip, DWORD flags)

    BOOL PrintWindow(HWND hwnd, HDC hdcBlt, UINT nFlags)
    UINT PW_CLIENTONLY

    BOOL __stdcall GetClientRect(HWND hWnd, LPRECT lpRect)
    BOOL __stdcall GetWindowRect(HWND hWnd, LPRECT lpRect)

    BOOL __stdcall OpenClipboard(HWND hWndNewOwner)
    BOOL __stdcall EmptyClipboard()
    HANDLE __stdcall SetClipboardData(UINT uFormat, HANDLE hMem)
    UINT CF_BITMAP
    BOOL __stdcall CloseClipboard()

    HWND __stdcall GetDesktopWindow()

    int __stdcall GetSystemMetrics(int nIndex)

    int SM_CXSCREEN
    int SM_CYSCREEN

    int BI_RGB
    int BI_RLE8
    int BI_RLE4
    int BI_BITFIELDS
    int BI_JPEG
    int BI_PNG

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
