#include "Python.h"
#include "pyparallel_http.h"

void
HttpHeader_dealloc(PyObject *self, PyObject *args)
{
    Py_XDECREF(((HttpHeader *)self)->dict);
    PyObject_Del(self);
}

#define _MEMBER(n, t, c, f, d) {#n, t, offsetof(c, n), f, d}
#define _HHMEM(n, t, f, d)  _MEMBER(n, t, HttpHeader, f, d)
#define _HH_CB(n)        _HHMEM(n, T_OBJECT,    0, #n " callback")
#define _HH_ATTR_O(n)    _HHMEM(n, T_OBJECT_EX, 0, #n " callback")
#define _HH_ATTR_OR(n)   _HHMEM(n, T_OBJECT_EX, 1, #n " callback")
#define _HH_ATTR_I(n)    _HHMEM(n, T_INT,       0, #n " attribute")
#define _HH_ATTR_IR(n)   _HHMEM(n, T_INT,       1, #n " attribute")
#define _HH_ATTR_UI(n)   _HHMEM(n, T_UINT,      0, #n " attribute")
#define _HH_ATTR_UIR(n)  _HHMEM(n, T_UINT,      1, #n " attribute")
#define _HH_ATTR_LL(n)   _HHMEM(n, T_LONGLONG,  0, #n " attribute")
#define _HH_ATTR_LLR(n)  _HHMEM(n, T_LONGLONG,  1, #n " attribute")
#define _HH_ATTR_ULL(n)  _HHMEM(n, T_ULONGLONG, 0, #n " attribute")
#define _HH_ATTR_ULLR(n) _HHMEM(n, T_ULONGLONG, 1, #n " attribute")
#define _HH_ATTR_B(n)    _HHMEM(n, T_BOOL,      0, #n " attribute")
#define _HH_ATTR_BR(n)   _HHMEM(n, T_BOOL,      1, #n " attribute")
#define _HH_ATTR_D(n)    _HHMEM(n, T_DOUBLE,    0, #n " attribute")
#define _HH_ATTR_DR(n)   _HHMEM(n, T_DOUBLE,    1, #n " attribute")
#define _HH_ATTR_S(n)    _HHMEM(n, T_STRING,    0, #n " attribute")

/*
static PyMemberDef HttpHeaderMembers[] = {
    _HH_ATTR_S(method),
    _HH_ATTR_S(path),
    _HH_ATTR_UIR(minor_version),
    _HH_ATTR_LLR(header_len),
    { NULL }
};
*/

/*
static int
HttpHeader_init(HttpHeader *self, PyObject *args, PyObject **kwds)
{
    if (PyDict_Type.tp_init((PyObject *)self, args, kwds) < 0)
        return -1;

}
*/


static PyTypeObject HttpHeader_Type = {
    PyVarObject_HEAD_INIT(0, 0)
    "HttpHeader",                               /* tp_name */
    sizeof(HttpHeader),                         /* tp_basicsize */
    0,                                          /* tp_itemsize */
    0,                                          /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_reserved */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,   /* tp_flags */
    "HTTP Header",                              /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    0,                                          /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    0,                                          /* tp_init */
    0,                                          /* tp_alloc */
    0,                                          /* tp_new */
    0,                                          /* tp_free */
};


/* vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                  */
