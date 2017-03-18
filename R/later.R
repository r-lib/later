#' @useDynLib later
#' @import Rcpp

.onLoad <- function(...) {
  saveNframesCallback(quote(base::sys.nframe()))
}

#' Executes a function later
#' 
#' Executes a function or formula (see \code{\link[rlang]{as_function}}) some
#' time in the future, when no other R code is on the execution stack.
#' 
#' TODO: Talk about error handling
#' 
#' @param func A function or formula.
#'   
#' @export
later <- function(func) {
  f <- rlang::as_function(func)
  execLater(f)
}
