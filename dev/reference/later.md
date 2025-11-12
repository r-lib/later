# Executes a function later

Schedule an R function or formula to run after a specified period of
time. Similar to JavaScript's `setTimeout` function. Like JavaScript, R
is single-threaded so there's no guarantee that the operation will run
exactly at the requested time, only that at least that much time will
elapse.

## Usage

``` r
later(func, delay = 0, loop = current_loop())
```

## Arguments

- func:

  A function or formula (see
  [`rlang::as_function()`](https://rlang.r-lib.org/reference/as_function.html)).

- delay:

  Number of seconds in the future to delay execution. There is no
  guarantee that the function will be executed at the desired time, but
  it should not execute earlier.

- loop:

  A handle to an event loop. Defaults to the currently-active loop.

## Value

A function, which, if invoked, will cancel the callback. The function
will return `TRUE` if the callback was successfully cancelled and
`FALSE` if not (this occurs if the callback has executed or has been
cancelled already).

## Details

The mechanism used by this package is inspired by Simon Urbanek's
[background](https://github.com/s-u/background) package and similar code
in Rhttpd.

## Note

To avoid bugs due to reentrancy, by default, scheduled operations only
run when there is no other R code present on the execution stack; i.e.,
when R is sitting at the top-level prompt. You can force past-due
operations to run at a time of your choosing by calling
[`run_now()`](https://later.r-lib.org/dev/reference/run_now.md).

Error handling is not particularly well-defined and may change in the
future. options(error=browser) should work and errors in `func` should
generally not crash the R process, but not much else can be said about
it at this point. If you must have specific behavior occur in the face
of errors, put error handling logic inside of `func`.

## Examples

``` r
# Example of formula style
later(~cat("Hello from the past\n"), 3)

# Example of function style
later(function() {
  print(summary(cars))
}, 2)
```
