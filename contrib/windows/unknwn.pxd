from types cimport *

cdef extern from "<Unknwn.h>":

    cdef cppclass IUnknown:
        ULONG AddRef()
        HRESULT QueryInterface(REFIID riid, void **ppvObject)
        ULONG Release()

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
