Build notes
===========


## tinycthread

Later uses threads via the tinycthread library, which has an API that is based on `<threads.h>` from the C11 standard. Our version of tinycthread has a modified API. This is because the API of the standard version of tinycthread is very similar to `<threads.h>`, but this causes problems when linking: if the system's C library implements the functions from threads.h, then during the linking phase, the resulting program will call the C library's functions instead of the functions from tinycthread, which is unsafe and can cause errors.

The tinycthread library is from https://github.com/tinycthread/tinycthread, and we used commit 6957fc8383d6c7db25b60b8c849b29caab1caaee, which is says it is version 1.2, but it is not officially released or tagged.

To work around the problem of linking to (incorrect) system functions with the same name, our version of tinycthread has modified names for all externally-visible functions and values: they all begin with `tct_`.

We also added a dummy header file called `badthreads.h`. For all of the names from C11 threads.h, it `#define`s them to a value that will cause an error at compile time. This is to make sure that we don't accidentally use anything from threads.h. Note: `thread_local` is no longer redefined as it has become a keyword in C23 - it is not used in our code base and we should take care not to use it.

There is another change that we have made to tinycthread is in `tinycthread.h`. It is a workaround for building on CRAN's Solaris build machine which was needed at some point in the past. Note that when we tested on a Solaris VM, it didn't seem to be necessary, but we kept it there just to be safe, because we can't actually test on the CRAN Solaris build machine.

```
// jcheng 2017-11-03: _XOPEN_SOURCE 600 is necessary to prevent Solaris headers
// from complaining about the combination of C99 and _XOPEN_SOURCE <= 500. The
// error message starts with:
// "Compiler or options invalid for pre-UNIX 03 X/Open applications"
#if defined(sun) && (__STDC_VERSION__ - 0 >= 199901L) && (!defined(_XOPEN_SOURCE) || ((_XOPEN_SOURCE - 0) < 600))
  #undef _XOPEN_SOURCE
  #define _XOPEN_SOURCE 600
#endif
```
