#include <R.h>
#include <Rinternals.h>

SEXP _later_new_weakref(SEXP x){

  return R_MakeWeakRef(x, R_NilValue, R_NilValue, FALSE);

}

SEXP _later_wref_key(SEXP x){

  return x != R_NilValue ? R_WeakRefKey(x) : R_NilValue;

}
