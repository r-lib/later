#include <R.h>
#include <Rinternals.h>
#include <stdlib.h> // for NULL
#include <R_ext/Rdynload.h>
#include <stdint.h> // for uint64_t
#include "fd.h" // for struct pollfd

/* FIXME:
Check these declarations against the C/Fortran source code.
*/

/* .Call calls */
SEXP _later_ensureInitialized(void);
SEXP _later_execCallbacks(SEXP, SEXP, SEXP);
SEXP _later_idle(SEXP);
SEXP _later_execLater(SEXP, SEXP, SEXP);
SEXP _later_cancel(SEXP, SEXP);
SEXP _later_execLater_fd(SEXP, SEXP, SEXP, SEXP, SEXP, SEXP);
SEXP _later_fd_cancel(SEXP);
SEXP _later_nextOpSecs(SEXP);
SEXP _later_testCallbackOrdering(void);
SEXP _later_createCallbackRegistry(SEXP, SEXP);
SEXP _later_deleteCallbackRegistry(SEXP);
SEXP _later_existsCallbackRegistry(SEXP);
SEXP _later_notifyRRefDeleted(SEXP);
SEXP _later_setCurrentRegistryId(SEXP);
SEXP _later_getCurrentRegistryId(void);
SEXP _later_list_queue_(SEXP);
SEXP _later_log_level(SEXP);
SEXP _later_using_ubsan(void);
SEXP _later_new_weakref(SEXP);
SEXP _later_wref_key(SEXP);

static const R_CallMethodDef CallEntries[] = {
  {"_later_ensureInitialized",      (DL_FUNC) &_later_ensureInitialized,      0},
  {"_later_execCallbacks",          (DL_FUNC) &_later_execCallbacks,          3},
  {"_later_idle",                   (DL_FUNC) &_later_idle,                   1},
  {"_later_execLater",              (DL_FUNC) &_later_execLater,              3},
  {"_later_cancel",                 (DL_FUNC) &_later_cancel,                 2},
  {"_later_execLater_fd",           (DL_FUNC) &_later_execLater_fd,           6},
  {"_later_fd_cancel",              (DL_FUNC) &_later_fd_cancel,              1},
  {"_later_nextOpSecs",             (DL_FUNC) &_later_nextOpSecs,             1},
  {"_later_testCallbackOrdering",   (DL_FUNC) &_later_testCallbackOrdering,   0},
  {"_later_createCallbackRegistry", (DL_FUNC) &_later_createCallbackRegistry, 2},
  {"_later_deleteCallbackRegistry", (DL_FUNC) &_later_deleteCallbackRegistry, 1},
  {"_later_existsCallbackRegistry", (DL_FUNC) &_later_existsCallbackRegistry, 1},
  {"_later_notifyRRefDeleted",      (DL_FUNC) &_later_notifyRRefDeleted,      1},
  {"_later_setCurrentRegistryId",   (DL_FUNC) &_later_setCurrentRegistryId,   1},
  {"_later_getCurrentRegistryId",   (DL_FUNC) &_later_getCurrentRegistryId,   0},
  {"_later_list_queue_",            (DL_FUNC) &_later_list_queue_,            1},
  {"_later_log_level",              (DL_FUNC) &_later_log_level,              1},
  {"_later_using_ubsan",            (DL_FUNC) &_later_using_ubsan,            0},
  {"_later_new_weakref",            (DL_FUNC) &_later_new_weakref,            1},
  {"_later_wref_key",               (DL_FUNC) &_later_wref_key,               1},
  {NULL, NULL, 0}
};

uint64_t execLaterNative(void (*func)(void*), void* data, double secs);
uint64_t execLaterNative2(void (*func)(void*), void* data, double secs, int loop);
int execLaterFdNative(void (*)(int *, void *), void *, int, struct pollfd *, double, int);
int apiVersion(void);

void R_init_later(DllInfo *dll) {
  R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);
  R_forceSymbols(dll, TRUE);
  // 2019-08-06
  // execLaterNative is registered here ONLY for backward compatibility; If
  // someone installed a package which had `#include <later_api.h>` (like
  // httpuv) then that package would have compiled the inline functions from
  // inst/include/later.h, which in turn used `R_GetCCallable("later",
  // "execLaterNative")`, then called that function with 3 arguments. For
  // anyone who upgrades this package but does not upgrade the downstream
  // dependency, that interface cannot change.
  //
  // So we register `execLaterNative` here, even though we don't actually call
  // it from inst/include/later.h anymore. This ensures that downstream deps
  // that were built with the previous version can still use
  // `R_GetCCallable("later", "execLaterNative")` and have it work properly.
  //
  // In a future version, after no one is running downstream packages that are
  // built against the previous version of later, we can remove this line.
  //
  // https://github.com/r-lib/later/issues/97
  R_RegisterCCallable("later", "execLaterNative",  (DL_FUNC)&execLaterNative);
  R_RegisterCCallable("later", "execLaterNative2", (DL_FUNC)&execLaterNative2);
  R_RegisterCCallable("later", "execLaterFdNative",(DL_FUNC)&execLaterFdNative);
  R_RegisterCCallable("later", "apiVersion",       (DL_FUNC)&apiVersion);
}
