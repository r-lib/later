#' @useDynLib later
#' @import Rcpp

.onLoad <- function(...) {
  saveNframesCallback(quote(later:::nframe()))
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
#' TODO: Talk about error handling
#' 
#' @param func A function or formula.
#' @param delay Number of seconds in the future to delay execution. There is no 
#'   guarantee that the function will be executed at the desired time, but it 
#'   should not execute earlier.
#'   
#' @export
later <- function(func, delay = 0) {
  f <- rlang::as_function(func)
  execLater(f, delay)
}

#' @export
run_now <- function() {
  invisible(execCallbacks())
}