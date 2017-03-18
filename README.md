# later

Run an R function or formula sometime in the future, when no other R code is on the execution stack. Similar to JavaScript's `setTimeout(func, 0);` or `process.nextTick(func)`.

The mechanism used by this package is inspired by Simon Urbanek's [background](https://github.com/s-u/background) package and similar code in Rhttpd.
