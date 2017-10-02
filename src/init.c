#include <R.h>
#include <Rinternals.h>
#include <stdlib.h> // for NULL
#include <R_ext/Rdynload.h>

/* FIXME: 
Check these declarations against the C/Fortran source code.
*/

/* .Call calls */
extern SEXP _later_ensureInitialized();
extern SEXP _later_execCallbacks();
extern SEXP _later_idle();
extern SEXP _later_execLater(SEXP, SEXP);
extern SEXP _later_saveNframesCallback(SEXP);
extern SEXP _later_secsToNext();

static const R_CallMethodDef CallEntries[] = {
  {"_later_ensureInitialized",   (DL_FUNC) &_later_ensureInitialized,   0},
  {"_later_execCallbacks",       (DL_FUNC) &_later_execCallbacks,       0},
  {"_later_idle",                (DL_FUNC) &_later_idle,                0},
  {"_later_execLater",           (DL_FUNC) &_later_execLater,           2},
  {"_later_saveNframesCallback", (DL_FUNC) &_later_saveNframesCallback, 1},
  {"_later_secsToNext",          (DL_FUNC) &_later_secsToNext,          0},
  {NULL, NULL, 0}
};

void execLaterNative(void (*func)(void*), void* data, double secs);

void R_init_later(DllInfo *dll)
{
  R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);
  R_RegisterCCallable("later", "execLaterNative", (DL_FUNC)&execLaterNative);
}
