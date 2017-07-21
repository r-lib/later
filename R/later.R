#' @useDynLib later
#' @import Rcpp
#' @importFrom Rcpp evalCpp

.onLoad <- function(...) {
  ensureInitialized()
  saveNframesCallback(parse(text="later:::nframe()")[[1]])
}

# I don't know why, but it's necessary to wrap sys.nframe (an .Internal) inside 
# a regular function call. Otherwise, you get the wrong results in the following
# situation:
#
# a <- function() {
#   later::later(~print("Got here 2"))
#   Sys.sleep(1)
#   print("Got here 1")
# }
# a()
#
# (To repro you must execute this in the R console, not source a script that 
# contains it, as the source() call itself will ruin the repro)
#
# The desired behavior is to sleep 1 second, then print "Got here 1", the print 
# "Got here 2". But if sys.nframe() is passed directly to saveNframesCallback,
# you get "Got here 2", then sleep 1 second, then "Got here 1".
#
# I spent half a day with a debug version of R and gdb trying to get to the root
# cause, without success.
nframe <- function() {
  sys.nframe()-1
}

#' Executes a function later
#' 
#' Schedule an R function or formula to run after a specified period of time.
#' Similar to JavaScript's `setTimeout` function. Like JavaScript, R is
#' single-threaded so there's no guarantee that the operation will run exactly
#' at the requested time, only that at least that much time will elapse.
#' 
#' The mechanism used by this package is inspired by Simon Urbanek's
#' [background](https://github.com/s-u/background) package and similar code in
#' Rhttpd.
#' 
#' @note
#' To avoid bugs due to reentrancy, by default, scheduled operations only run
#' when there is no other R code present on the execution stack; i.e., when R is
#' sitting at the top-level prompt. You can force past-due operations to run at
#' a time of your choosing by calling [run_now()].
#' 
#' Error handling is not particularly well-defined and may change in the future.
#' options(error=browser) should work and errors in `func` should generally not
#' crash the R process, but not much else can be said about it at this point.
#' If you must have specific behavior occur in the face of errors, put error
#' handling logic inside of `func`.
#' 
#' @param func A function or formula (see [rlang::as_function()]).
#' @param delay Number of seconds in the future to delay execution. There is no 
#'   guarantee that the function will be executed at the desired time, but it 
#'   should not execute earlier.
#'
#' @examples
#' # Example of formula style
#' later(~cat("Hello from the past\n"), 3)
#' 
#' # Example of function style
#' later(function() {
#'   print(summary(cars))
#' }, 2)
#'   
#' @export
later <- function(func, delay = 0) {
  f <- rlang::as_function(func)
  execLater(f, delay)
}

#' Execute scheduled operations
#' 
#' Normally, operations scheduled with [later()] will not execute unless/until
#' no other R code is on the stack (i.e. at the top-level). If you need to run
#' blocking R code for a long time and want to allow scheduled operations to run
#' at well-defined points of your own operation, you can call `run_now()` at
#' those points and any operations that are due to run will do so.
#' 
#' If one of the callbacks throws an error, the error will _not_ be caught, and
#' subsequent callbacks will not be executed (until `run_now()` is called again,
#' or control returns to the R prompt). You must use your own [base::tryCatch()]
#' if you want to handle errors.
#' 
#' @return A logical indicating whether any callbacks were actually run.
#' 
#' @export
run_now <- function() {
  invisible(execCallbacks())
}

#' Check if later loop is empty
#' 
#' Returns true if there are currently no callbacks that are scheduled to
#' execute in the present or future.
#' 
#' @keywords internal
#' @export
loop_empty <- function() {
  idle()
}
