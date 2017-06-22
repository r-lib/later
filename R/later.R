#' @useDynLib later
#' @import Rcpp
#' @importFrom Rcpp evalCpp

.onLoad <- function(...) {
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
#' Executes a function or formula (see \code{\link[rlang]{as_function}}) some 
#' time in the future, when no other R code is on the execution stack.
#' 
#' Error handling is not particularly well-defined and may change in the future.
#' options(error=browser) should work and errors in `func` should generally not
#' crash the R process, but not much else can be said about it at this point.
#' If you must have specific behavior occur in the face of errors, put error
#' handling logic inside of `func`.
#' 
#' @param func A function or formula.
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
#' Normally, operations scheduled with \code{\link{later}} will not execute 
#' unless/until no other R code is on the stack (i.e. at the top-level). If you 
#' need to run blocking R code for a long time and want to allow scheduled
#' operations to run at well-defined points of your own operation, you can call
#' \code{run_now} at those points and any operations that are due to run will do
#' so.
#' 
#' @return A logical indicating whether any callbacks were actually run.
#' 
#' @export
run_now <- function() {
  invisible(execCallbacks())
}