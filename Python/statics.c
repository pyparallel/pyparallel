#include "Python.h"
#include "statics.h"

static PyStatics statics;

/* xxx todo: simplify this with some macro magic. */

/* 0 = failure, 1 = success */
int
_init_statics(void)
{
    Py_GUARD();

    statics.empty =              PyUnicode_New(0, 0);
    statics.nul =                PyUnicode_FromStringAndSize("\0", 1);

    statics.comma =              PyUnicode_FromString(",");
    statics.space =              PyUnicode_FromString(" ");
    statics.tab =                PyUnicode_FromString("\t");
    statics.cr =                 PyUnicode_FromString("\r");
    statics.lf =                 PyUnicode_FromString("\n");

    statics.comma_space =        PyUnicode_FromString(", ");

    statics.s_true =             PyUnicode_FromString("true");
    statics.s_false =            PyUnicode_FromString("false");
    statics.s_null =             PyUnicode_FromString("null");

    statics.infinity =           PyUnicode_FromString("Infinity");
    statics.neg_infinity =       PyUnicode_FromString("-Infinity");
    statics.nan =                PyUnicode_FromString("NaN");

    statics.open_dict =          PyUnicode_FromString("{");
    statics.close_dict =         PyUnicode_FromString("}");
    statics.empty_dict =         PyUnicode_FromString("{}");
    statics.ellipsis_dict =      PyUnicode_FromString("{...}");

    statics.open_array =         PyUnicode_FromString("[");
    statics.close_array =        PyUnicode_FromString("]");
    statics.empty_array =        PyUnicode_FromString("[]");
    statics.ellipsis_array =     PyUnicode_FromString("[...]");

    statics.open_tuple =         PyUnicode_FromString("(");
    statics.close_tuple =        PyUnicode_FromString(")");
    statics.empty_tuple =        PyUnicode_FromString("()");
    statics.ellipsis_tuple =     PyUnicode_FromString("(...)");
    statics.comma_close_tuple =  PyUnicode_FromString(",)");

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
Py_Static(PyObject *o)
{
    Py_INCREF(o);
    return o;
}

int
_Py_IsStatic(PyObject *o, int *index)
{
    PyObject **p = (PyObject **)&statics;
    int num_statics = sizeof(PyStatics) / sizeof(PyObject *);
    int found = 0;
    int i;

    for (i = 0; i < num_statics; i++, p++) {
        if (*p == o) {
            found = 1;
            break;
        }
    }

    if (found && index)
        *index = i;

    return found;
}
