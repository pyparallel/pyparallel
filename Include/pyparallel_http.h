
#include "picohttpparser.h"

#define _HttpHeader_HEAD                    \
    PyObject_HEAD                           \
    char   *method;                         \
    size_t  method_len;                     \
    char   *path;                           \
    size_t  path_len;                       \
    int     minor_version;                  \
    struct phr_header *headers;             \
    size_t  num_headers_used;               \
    size_t  last_len;                       \
    size_t  num_headers_allocated;          \
    size_t  bytes_allocated;                \
    size_t  bytes_used;                     \
    size_t  header_len;                     \
    PyObject *dict;

typedef struct _HttpHeader {
    _HttpHeader_HEAD
} HttpHeader;

typedef struct _HttpRequest {
    _HttpHeader_HEAD
} HttpRequest;

typedef struct _HttpResponse {
    _HttpHeader_HEAD
    char   *message;
    size_t  message_len;
} HttpResponse;

PyAPI_DATA(PyTypeObject) HttpHeader_Type;
/*
PyAPI_DATA(PyTypeObject) HttpRequest_Type;
PyAPI_DATA(PyTypeObject) HttpResponse_Type;
*/

/* vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                  */
