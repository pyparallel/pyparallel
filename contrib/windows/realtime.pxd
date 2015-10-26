# realtimeapiset.h
from types cimport *

cdef extern from "<windows.h>":

    BOOL QueryInterruptTime(PULONGLONG lpInterruptTime)

    BOOL QueryInterruptTimePrecise(PULONGLONG lpInterruptTimePrecise)

    BOOL QueryUnbiasedInterruptTime(PULONGLONG lpUnbiasedInterruptTime)

    BOOL QueryUnbiasedInterruptTimePrecise(
        PULONGLONG lpUnbiasedInterruptTimePrecise
    )

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
