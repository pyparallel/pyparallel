#ifndef __PYX_HAVE__windows__processthreads
#define __PYX_HAVE__windows__processthreads

struct THREAD_TIMES;
typedef struct THREAD_TIMES THREAD_TIMES;

/* "windows\processthreads.pxd":24
 * 
 * # Non-standard types
 * ctypedef public struct THREAD_TIMES:             # <<<<<<<<<<<<<<
 *     HANDLE ThreadHandle
 *     ULONGLONG CreationTime
 */
struct THREAD_TIMES {
  HANDLE ThreadHandle;
  ULONGLONG CreationTime;
  ULONGLONG ExitTime;
  ULONGLONG KernelTime;
  ULONGLONG UserTime;
};

/* "windows\processthreads.pxd":30
 *     ULONGLONG KernelTime
 *     ULONGLONG UserTime
 * ctypedef public THREAD_TIMES* PTHREAD_TIMES             # <<<<<<<<<<<<<<
 * 
 * 
 */
typedef THREAD_TIMES *PTHREAD_TIMES;

#ifndef __PYX_HAVE_API__windows__processthreads

#ifndef __PYX_EXTERN_C
  #ifdef __cplusplus
    #define __PYX_EXTERN_C extern "C"
  #else
    #define __PYX_EXTERN_C extern
  #endif
#endif

#ifndef DL_IMPORT
  #define DL_IMPORT(_T) _T
#endif

#endif /* !__PYX_HAVE_API__windows__processthreads */

#if PY_MAJOR_VERSION < 3
PyMODINIT_FUNC initprocessthreads(void);
#else
PyMODINIT_FUNC PyInit_processthreads(void);
#endif

#endif /* !__PYX_HAVE__windows__processthreads */
