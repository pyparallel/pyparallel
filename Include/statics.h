#ifndef PY_STATICS_H
#define PY_STATICS_H

typedef struct _statics {
    PyObject *sep;              /* ""                   */
    PyObject *comma;            /* ","                  */
    PyObject *space;            /* " "                  */
    PyObject *tab;              /* "\t"                 */
    PyObject *cr;               /* "\r"                 */
    PyObject *lf;               /* "\n"                 */

    PyObject *s_true;           /* "true"               */
    PyObject *s_false;          /* "false"              */
    PyObject *s_null;           /* "null"               */

    PyObject *nan;              /* "NaN"                */
    PyObject *infinity;         /* "Infinity"           */
    PyObject *neg_infinity;     /* "-Infinity"          */

    PyObject *open_dict;        /* "{"                  */
    PyObject *close_dict;       /* "}"                  */
    PyObject *empty_dict;       /* "{}"                 */

    PyObject *open_array;       /* "["                  */
    PyObject *close_array;      /* "]"                  */
    PyObject *empty_array;      /* "[]"                 */

} PyStatics;

PyAPI_DATA(PyStatics) statics;

int _init_statics();

#endif /* PY_STATICS_H */
