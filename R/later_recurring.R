#' @describeIn later Schedules a recurring task
#' @param limit Number of times to repeat the function. If `NA` (the default)
#'   then no limit.
#' @examples
#' later_recurring(~cat("Hello from the past\n"), 3, limit = 2)
later_recurring <- function(func, delay, limit = NA, loop = current_loop()) {
  func <- rlang::as_function(func)
  cancelled <- FALSE
  if (!is.na(limit) && limit < 1)
    stop("'limit' must be 'NA' or a positive number")
  func2 <- function() {
    limit <<- limit - 1L
    func()
    if (!cancelled && (is.na(limit) || limit > 0))
      handle <<- later(func2, delay, loop)
  }
  handle <- later(func2, delay, loop)
  invisible(function() {
    cancelled <<- TRUE
    handle()
  })
}
