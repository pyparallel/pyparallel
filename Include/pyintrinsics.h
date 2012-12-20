#ifndef Py_INTRINSICS_H
#define Py_INTRINSICS_H
#ifdef __cplusplus
extern "C" {
#endif

#ifdef WITH_INTRINSICS
#   ifdef MS_WINDOWS
#       include <intrin.h>
#       if defined(MS_WIN64)
#           pragma intrinsic(__readgsdword)
#           define _Py_get_current_process_id() __readgsdword(0x40)
#           define _Py_get_current_thread_id()  __readgsdword(0x48)
#       elif defined(MS_WIN32)
#           pragma intrinsic(__readfsdword)
#           define _Py_get_current_process_id() __readfsdword(0x20)
#           define _Py_get_current_thread_id()  __readfsdword(0x24)
#       else
#           error "Unsupported architecture."
#       endif
#       define _Py_clflush(p)    _mm_clflush(p)
#       define _Py_lfence()      _mm_lfence()
#       define _Py_mfence()      _mm_mfence()
#       define _Py_sfence()      _mm_sfence()
#   else
#       error "Intrinsics not available for this platform yet."
#   endif
#else /* WITH_INTRINSICS */
#   ifdef MS_WINDOWS
#       define _Py_get_current_process_id GetCurrentProcessId()
#       define _Py_get_current_thread_id GetCurrentThreadId()
#       define _Py_clflush()    XXX_UNKNOWN
#       define _Py_lfence()     MemoryBarrier()
#       define _Py_mfence()     MemoryBarrier()
#       define _Py_sfence()     MemoryBarrier()
#   else /* MS_WINDOWS */
#       error "No intrinsics stubs available for this platform."
#   endif
#endif

#ifdef __cplusplus
}
#endif
#endif
/* vim:set ts=8 sw=4 sts=4 tw=78 et: */
