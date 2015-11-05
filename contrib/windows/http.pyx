
cdef extern from "<http.h>":
    pass

include "windows.pxi"

from types cimport *
from constants cimport *
from base cimport *
from http cimport *

#===============================================================================
# Helpers
#===============================================================================

#===============================================================================
# Classes
#===============================================================================
cdef class HttpRequestV1:
    cdef:
        HTTP_REQUEST _request

cdef class HttpRequest:
    cdef:
        HTTP_REQUEST_V2 _request


# vim: set ts=8 sw=4 sts=4 tw=80 et:
