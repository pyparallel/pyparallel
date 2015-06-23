#ifndef PYPARALLEL_UTIL_H
#define PYPARALLEL_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#define _SECOND_ABSOLUTE (10000000LL)
#define _MINUTE_ABSOLUTE (60 * _SECOND_ABSOLUTE)
#define _HOUR_ABSOLUTE   (60 * _MINUTE_ABSOLUTE)
#define _DAY_ABSOLUTE    (24 * _HOUR_ABSOLUTE)

#define _SECOND_RELATIVE (-10000000LL)
#define _MINUTE_RELATIVE (60 * _SECOND_RELATIVE)
#define _HOUR_RELATIVE   (60 * _MINUTE_RELATIVE)
#define _DAY_RELATIVE    (24 * _HOUR_RELATIVE)

static __inline
void
_PyParallel_SecondsToRelativeThreadpoolWaitTime(
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

#define _PyParallel_SecondsToRelativeThreadpoolTimerTime \
        _PyParallel_SecondsToRelativeThreadpoolWaitTime

#ifdef __cplusplus
}
#endif

#endif /* PYPARALLEL_UTIL_H */

/* vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                  */
