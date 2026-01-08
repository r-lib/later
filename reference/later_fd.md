# Executes a function when a file descriptor is ready

Schedule an R function or formula to run after an indeterminate amount
of time when file descriptors are ready for reading or writing, subject
to an optional timeout.

## Usage

``` r
later_fd(
  func,
  readfds = integer(),
  writefds = integer(),
  exceptfds = integer(),
  timeout = Inf,
  loop = current_loop()
)
```

## Arguments

- func:

  A function that takes a single argument, a logical vector that
  indicates which file descriptors are ready (a concatenation of
  `readfds`, `writefds` and `exceptfds`). This may be all `FALSE` if the
  `timeout` argument is non-`Inf`. File descriptors with error
  conditions pending are represented as `NA`, as are invalid file
  descriptors such as those already closed.

- readfds:

  Integer vector of file descriptors, or Windows SOCKETs, to monitor for
  being ready to read.

- writefds:

  Integer vector of file descriptors, or Windows SOCKETs, to monitor
  being ready to write.

- exceptfds:

  Integer vector of file descriptors, or Windows SOCKETs, to monitor for
  error conditions pending.

- timeout:

  Number of seconds to wait before giving up, and calling `func` with
  all `FALSE`. The default `Inf` implies waiting indefinitely.
  Specifying `0` will check once without blocking, and supplying a
  negative value defaults to a timeout of 1s.

- loop:

  A handle to an event loop. Defaults to the currently-active loop.

## Value

A function, which, if invoked, will cancel the callback. The function
will return `TRUE` if the callback was successfully cancelled and
`FALSE` if not (this occurs if the callback has executed or has been
cancelled already).

## Details

On the occasion the system-level `poll` (on Windows `WSAPoll`) returns
an error, the callback will be made on a vector of all `NA`s. This is
indistinguishable from a case where the `poll` succeeds but there are
error conditions pending against each file descriptor.

If no file descriptors are supplied, the callback is scheduled for
immediate execution and made on the empty logical vector `logical(0)`.

## Note

To avoid bugs due to reentrancy, by default, scheduled operations only
run when there is no other R code present on the execution stack; i.e.,
when R is sitting at the top-level prompt. You can force past-due
operations to run at a time of your choosing by calling
[`run_now()`](https://later.r-lib.org/reference/run_now.md).

Error handling is not particularly well-defined and may change in the
future. options(error=browser) should work and errors in `func` should
generally not crash the R process, but not much else can be said about
it at this point. If you must have specific behavior occur in the face
of errors, put error handling logic inside of `func`.

## Examples

``` r
# create nanonext sockets
s1 <- nanonext::socket(listen = "inproc://nano")
s2 <- nanonext::socket(dial = "inproc://nano")
fd1 <- nanonext::opt(s1, "recv-fd")
fd2 <- nanonext::opt(s2, "recv-fd")

# 1. timeout: prints FALSE, FALSE
later_fd(print, c(fd1, fd2), timeout = 0.1)
Sys.sleep(0.2)
run_now()
#> [1] FALSE FALSE

# 2. fd1 ready: prints TRUE, FALSE
later_fd(print, c(fd1, fd2), timeout = 1)
res <- nanonext::send(s2, "msg")
Sys.sleep(0.1)
run_now()
#> [1]  TRUE FALSE

# 3. both ready: prints TRUE, TRUE
res <- nanonext::send(s1, "msg")
later_fd(print, c(fd1, fd2), timeout = 1)
Sys.sleep(0.1)
run_now()
#> [1] TRUE TRUE

# 4. fd2 ready: prints FALSE, TRUE
res <- nanonext::recv(s1)
later_fd(print, c(fd1, fd2), timeout = 1)
Sys.sleep(0.1)
run_now()
#> [1] FALSE  TRUE

# 5. fds invalid: prints NA, NA
close(s2)
close(s1)
later_fd(print, c(fd1, fd2), timeout = 0)
Sys.sleep(0.1)
run_now()
#> [1] NA NA
```
