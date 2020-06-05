The following messages occur with UBSAN. We believe that it is a false positive, due to a bug (or perhaps limitation) in UBSAN. Here's what we found from looking at it in the past:

```
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior /data/gannet/ripley/R/packages/tests-clang-SAN/later.Rcheck/later/include/later.h:96:3 in
/data/gannet/ripley/R/packages/tests-clang-SAN/later.Rcheck/later/include/later.h:96:3: runtime error: call to function execLaterNative through pointer to incorrect function type 'void (*)(void (*)(void *), void *, double)'


later.Rcheck/tests/testthat.Rout:/data/gannet/ripley/R/packages/incoming/later.Rcheck/later/include/later.h:72:3:
runtime error: call to function execLaterNative2 through pointer to
incorrect function type 'void (*)(void (*)(void *), void *, double, int)'
```

> The problem has to do with crossing dynamically linked libraries. It appears that when you have a pointer to a function in .so A, and pass it to an .so B to be invoked, clang-UBSAN emits this error. It's as if the function types can't match up if they're not from the same library.
> In our case, we're using R_GetCCallable to pass a callback to later.so. If the callback function lives in later.so, there's no error; but a callback function from a different .so does trigger the error.

For more information:
https://github.com/r-lib/later/pull/46#issuecomment-370973284

Here's the issue filed on the sanitizer:
https://github.com/google/sanitizers/issues/911
