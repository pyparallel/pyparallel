#include "Python.h"
#include "statics.h"

static PyStatics statics;

/* 0 = failure, 1 = success */
int
_init_statics(void)
{
    /* This is super hacky. */
    Py_GUARD();

    statics.sep =                PyUnicode_FromStringAndSize("", 0);
    statics.comma =              PyUnicode_InternFromString(",");
    statics.space =              PyUnicode_InternFromString(" ");
    statics.tab =                PyUnicode_InternFromString("\t");
    statics.cr =                 PyUnicode_InternFromString("\r");
    statics.lf =                 PyUnicode_InternFromString("\n");

    statics.s_true =             PyUnicode_InternFromString("true");
    statics.s_false =            PyUnicode_InternFromString("false");
    statics.s_null =             PyUnicode_InternFromString("null");

    statics.infinity =           PyUnicode_InternFromString("Infinity");
    statics.neg_infinity =       PyUnicode_InternFromString("-Infinity");
    statics.nan =                PyUnicode_InternFromString("NaN");

    statics.open_dict =          PyUnicode_InternFromString("{");
    statics.close_dict =         PyUnicode_InternFromString("}");
    statics.empty_dict =         PyUnicode_InternFromString("{}");

    statics.open_array =         PyUnicode_InternFromString("[");
    statics.close_array =        PyUnicode_InternFromString("]");
    statics.empty_array =        PyUnicode_InternFromString("[]");

    if (!(
        statics.sep              &&
        statics.comma            &&
        statics.space            &&
        statics.tab              &&
        statics.cr               &&
        statics.lf               &&

        statics.s_true           &&
        statics.s_false          &&
        statics.s_null           &&

        statics.infinity         &&
        statics.neg_infinity     &&
        statics.nan              &&

        statics.open_dict        &&
        statics.close_dict       &&
        statics.empty_dict       &&

        statics.open_array       &&
        statics.close_array      &&
        statics.empty_array
    ))
        return 0;

    Py_INCREF(statics.sep);
    Py_INCREF(statics.comma);
    Py_INCREF(statics.space);
    Py_INCREF(statics.tab);
    Py_INCREF(statics.cr);
    Py_INCREF(statics.lf);

    Py_INCREF(statics.s_true);
    Py_INCREF(statics.s_false);
    Py_INCREF(statics.s_null);

    Py_INCREF(statics.infinity);
    Py_INCREF(statics.neg_infinity);
    Py_INCREF(statics.nan);

    Py_INCREF(statics.open_dict);
    Py_INCREF(statics.close_dict);
    Py_INCREF(statics.empty_dict);

    Py_INCREF(statics.open_array);
    Py_INCREF(statics.close_array);
    Py_INCREF(statics.empty_array);

    return 1;
}
