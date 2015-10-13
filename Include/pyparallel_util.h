#ifndef PYPARALLEL_UTIL_H
#define PYPARALLEL_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#define _MICROSECOND_ABSOLUTE           (10LL) /* 100 nanoseconds */
#define _MILLISECOND_ABSOLUTE           (_MICROSECOND_ABSOLUTE * 1000)
#define _SECOND_ABSOLUTE                (_MILLISECOND_ABSOLUTE * 1000)
#define _MINUTE_ABSOLUTE           (60 * _SECOND_ABSOLUTE)
#define _HOUR_ABSOLUTE             (60 * _MINUTE_ABSOLUTE)
#define _DAY_ABSOLUTE              (24 * _HOUR_ABSOLUTE)


#define _MICROSECOND_RELATIVE           (-10LL) /* 100 nanoseconds */
#define _MILLISECOND_RELATIVE           (_MICROSECOND_RELATIVE * 1000)
#define _SECOND_RELATIVE                (_MILLISECOND_RELATIVE * 1000)
#define _MINUTE_RELATIVE           (60 * _SECOND_RELATIVE)
#define _HOUR_RELATIVE             (60 * _MINUTE_RELATIVE)
#define _DAY_RELATIVE              (24 * _HOUR_RELATIVE)

static __inline
void
_PyParallel_MicrosecondsToRelativeThreadpoolTime(
    DWORD microseconds,
    PFILETIME filetime
)
{
    ULARGE_INTEGER sec, rel;
    sec.LowPart = microseconds;
    sec.HighPart = 0;
    rel.QuadPart = (_MICROSECOND_RELATIVE * sec.QuadPart);
    filetime->dwLowDateTime = rel.LowPart;
    filetime->dwHighDateTime = rel.HighPart;
    return;
}

static __inline
void
_PyParallel_MillisecondsToRelativeThreadpoolTime(
    DWORD milliseconds,
    PFILETIME filetime
)
{
    ULARGE_INTEGER sec, rel;
    sec.LowPart = milliseconds;
    sec.HighPart = 0;
    rel.QuadPart = (_MILLISECOND_RELATIVE * sec.QuadPart);
    filetime->dwLowDateTime = rel.LowPart;
    filetime->dwHighDateTime = rel.HighPart;
    return;
}

static __inline
void
_PyParallel_SecondsToRelativeThreadpoolTime(
    DWORD seconds,
    PFILETIME filetime
)
{
    ULARGE_INTEGER sec, rel;
    sec.LowPart = seconds;
    sec.HighPart = 0;
    rel.QuadPart = (_SECOND_RELATIVE * sec.QuadPart);
    filetime->dwLowDateTime = rel.LowPart;
    filetime->dwHighDateTime = rel.HighPart;
    return;
}

static __inline
void
_PyParallel_DaysSecondsMicrosecondsToRelativeThreadpoolTime(
    DWORD days,
    DWORD seconds,
    DWORD microseconds,
    PFILETIME filetime
)
{
    ULARGE_INTEGER micro, sec, day, rel;
    micro.LowPart = microseconds;
    micro.HighPart = 0;
    rel.QuadPart = (_MICROSECOND_ABSOLUTE * micro.QuadPart);

    sec.LowPart = seconds;
    sec.HighPart = 0;
    rel.QuadPart += (_SECOND_ABSOLUTE * sec.QuadPart);

    day.LowPart = days;
    day.HighPart = 0;
    rel.QuadPart += (_DAY_ABSOLUTE * day.QuadPart);

    rel.QuadPart *= -1ULL;

    filetime->dwLowDateTime = rel.LowPart;
    filetime->dwHighDateTime = rel.HighPart;
    return;
}

#ifdef __cplusplus
}
#endif

PyAPI_FUNC(PyObject *) _PyParallel_ProtectObject(PyObject *o);

#endif /* PYPARALLEL_UTIL_H */

/* vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                  */
