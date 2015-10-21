from types cimport *

cdef extern from *:
    int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar)
    UINT CP_UTF8


# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:
