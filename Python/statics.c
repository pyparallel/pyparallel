#include "Python.h"
#include "statics.h"

static PyStatics statics;

/* 0 = failure, 1 = success */
int
_init_statics(void)
{
    Py_GUARD();

    statics.empty =              PyUnicode_New(0, 0);
    statics.nul =                PyUnicode_FromStringAndSize("\0", 1);

    statics.comma =              PyUnicode_InternFromString(",");
    statics.space =              PyUnicode_InternFromString(" ");
    statics.tab =                PyUnicode_InternFromString("\t");
    statics.cr =                 PyUnicode_InternFromString("\r");
    statics.lf =                 PyUnicode_InternFromString("\n");

    statics.comma_space =        PyUnicode_InternFromString(", ");

    statics.s_true =             PyUnicode_InternFromString("true");
    statics.s_false =            PyUnicode_InternFromString("false");
    statics.s_null =             PyUnicode_InternFromString("null");

    statics.infinity =           PyUnicode_InternFromString("Infinity");
    statics.neg_infinity =       PyUnicode_InternFromString("-Infinity");
    statics.nan =                PyUnicode_InternFromString("NaN");

    statics.open_dict =          PyUnicode_InternFromString("{");
    statics.close_dict =         PyUnicode_InternFromString("}");
    statics.empty_dict =         PyUnicode_InternFromString("{}");
    statics.ellipsis_dict =      PyUnicode_InternFromString("{...}");

    statics.open_array =         PyUnicode_InternFromString("[");
    statics.close_array =        PyUnicode_InternFromString("]");
    statics.empty_array =        PyUnicode_InternFromString("[]");
    statics.ellipsis_array =     PyUnicode_InternFromString("[...]");

    statics.open_tuple =         PyUnicode_InternFromString("(");
    statics.close_tuple =        PyUnicode_InternFromString(")");
    statics.empty_tuple =        PyUnicode_InternFromString("()");
    statics.ellipsis_tuple =     PyUnicode_InternFromString("(...)");
    statics.comma_close_tuple =  PyUnicode_InternFromString(",)");

    if (!(
        statics.empty            &&
        statics.nul              &&
        statics.comma            &&
        statics.space            &&
        statics.tab              &&
        statics.cr               &&
        statics.lf               &&

        statics.comma_space      &&

        statics.s_true           &&
        statics.s_false          &&
        statics.s_null           &&

        statics.infinity         &&
        statics.neg_infinity     &&
        statics.nan              &&

        statics.open_dict        &&
        statics.close_dict       &&
        statics.empty_dict       &&
        statics.ellipsis_dict    &&

        statics.open_array       &&
        statics.close_array      &&
        statics.empty_array      &&
        statics.ellipsis_array   &&

        statics.open_tuple       &&
        statics.close_tuple      &&
        statics.empty_tuple      &&
        statics.ellipsis_tuple   &&
        statics.comma_close_tuple
    ))
        return 0;

    Py_INCREF(statics.empty);
    Py_INCREF(statics.nul);
    Py_INCREF(statics.comma);
    Py_INCREF(statics.space);
    Py_INCREF(statics.tab);
    Py_INCREF(statics.cr);
    Py_INCREF(statics.lf);

    Py_INCREF(statics.comma_space);

    Py_INCREF(statics.s_true);
    Py_INCREF(statics.s_false);
    Py_INCREF(statics.s_null);

    Py_INCREF(statics.infinity);
    Py_INCREF(statics.neg_infinity);
    Py_INCREF(statics.nan);

    Py_INCREF(statics.open_dict);
    Py_INCREF(statics.close_dict);
    Py_INCREF(statics.empty_dict);
    Py_INCREF(statics.ellipsis_dict);

    Py_INCREF(statics.open_array);
    Py_INCREF(statics.close_array);
    Py_INCREF(statics.empty_array);
    Py_INCREF(statics.ellipsis_array);

    Py_INCREF(statics.open_tuple);
    Py_INCREF(statics.close_tuple);
    Py_INCREF(statics.empty_tuple);
    Py_INCREF(statics.ellipsis_tuple);
    Py_INCREF(statics.comma_close_tuple);

    return 1;
}

static
PyObject *
Py_Static(PyObject *)
{
    return _Py_Static(o);
}

