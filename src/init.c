#include <R.h>
#include <Rinternals.h>
#include <stdlib.h> // for NULL
#include <R_ext/Rdynload.h>

/* FIXME: 
Check these declarations against the C/Fortran source code.
*/

/* .Call calls */
extern SEXP later_execCallbacks();
extern SEXP later_execLater(SEXP, SEXP);
extern SEXP later_saveNframesCallback(SEXP);

static const R_CallMethodDef CallEntries[] = {
  {"later_execCallbacks",       (DL_FUNC) &later_execCallbacks,       0},
  {"later_execLater",           (DL_FUNC) &later_execLater,           2},
  {"later_saveNframesCallback", (DL_FUNC) &later_saveNframesCallback, 1},
  {NULL, NULL, 0}
};

void R_init_later(DllInfo *dll)
{
  R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);
}
