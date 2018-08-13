Build notes
===========


## threads.h/tinycthread

Later uses threads via an API that is compatible with `<threads.h>` from the C11 standard. If `threads.h` is not available, it will use the tinycthread library instead, which is in src/tinycthreads. The tinycthread library provides an API that is compatible with threads.h.

threads.h is available in glibc 2.28 and above, and in musl (as of mid-2014). The threads.h detection is done by the /configure script.

Note that `threads.h` is not strictly tied to C11. C11 implementations are not required to have `threads.h`, and some older C implementations do have threads.h. For example, on Arch Linux (which uses glibc 2.28 as of 2018-08), if a C program uses `threads.h` and is compiled with `gcc -std=c99 -pthread`, it will compile and link correctly.

The tinycthread library is from https://github.com/tinycthread/tinycthread, and we used commit 6957fc8383d6c7db25b60b8c849b29caab1caaee, which is says it is version 1.2, but it is not officially released or tagged.

The one change that we have made to tinycthread is in `tinycthread.h`. It is a workaround for building on CRAN's Solaris build machine which was needed at some point in the past. Note that when we tested on a Solaris VM, it didn't seem to be necessary, but we kept it there just to be safe, because we can't actually test on the CRAN Solaris build machine.

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
