#ifndef PYPARALLEL_ODBC_H
#define PYPARALLEL_ODBC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sql.h>
#include <sqlext.h>

PyAPI_FUNC(HENV*) _PyParallel_GetDbEnvp(void);

#ifdef __cplusplus
}
#endif

#endif /* PYPARALLEL_ODBC_H */

/* vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                  */
