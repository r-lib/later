#' @useDynLib later
#' @import Rcpp

.onLoad <- function(...) {
  saveNframesCallback(quote(base::sys.nframe()))
}

#' @export
later <- function(func) {
  f <- rlang::as_function(func)
  execLater(f)
}
