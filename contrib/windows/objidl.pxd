from types cimport *
from objidl cimport *

cdef extern from "<Objidl.h>":

    ctypedef struct STATSTG:
        LPOLESTR       pwcsName
        DWORD          type
        ULARGE_INTEGER cbSize
        FILETIME       mtime
        FILETIME       ctime
        FILETIME       atime
        DWORD          grfMode
        DWORD          grfLocksSupported
        CLSID          clsid
        DWORD          grfStateBits
        DWORD          reserved

    ctypedef struct LOCKTYPE:
        LOCK_WRITE      = 1
        LOCK_EXCLUSIVE  = 2
        LOCK_ONLYONCE   = 4

    ctypedef enum STATFLAG:
        STATFLAG_DEFAULT    = 0
        STATFLAG_NONAME     = 1
        STATFLAG_NOOPEN     = 2

    cdef cppclass ISequentialStream(IUnknown):
        HRESULT Read(void *pv, ULONG cb, ULONG *pcbRead)
        HRESULT Write(void const *pv, ULONG cb, ULONG *pcbWritten)

    cdef cppclass IStream:
        pass

    cdef cppclass IStream(ISequentialStream):
        HRESULT Clone(IStream **ppstm)
        HRESULT Commit(DWORD grfCommitFlags)
        HRESULT CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten)
        HRESULT LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
        HRESULT Read(void const *pv, ULONG cb, ULONG *pcbRead)
        HRESULT Revert()
        HRESULT Seek(ULARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
        HRESULT SetSize(ULARGE_INTEGER libNewSize)
        HRESULT Stat(STATSTG *pstatstg)
        HRESULT UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
        HRESULT Write(void const *pv, ULONG cb, ULONG *pcbWritten)

# vim:set ts=8 sw=4 sts=4 tw=0 et nospell:
