#' @describeIn later Schedules a recurring task
#' @details
#'
#' In `later_recurring`, if `func` returns an explicit `FALSE` then
#' this is interpreted as self-cancelling the loop. Anything else
#' returned (including multiple `FALSE`) is ignored.
#' 
#' @param limit Number of times to repeat the function. If `Inf` (the
#'   default) then no limit.
#' @examples
#' # Limit number of executions to 3 times
#' later_recurring(~ message("Hello from the past"), 1, limit = 3)
#' 
#' # Stop recurring when the return value is `FALSE`
#' later_recurring(function() {
#'   message("Flipping a coin to see if we run again...")
#'   sample(c(TRUE, FALSE), size = 1L)
#' }, 0.25, limit = Inf)
#' @export
later_recurring <- function(func, delay, limit = Inf, loop = current_loop()) {
  func <- rlang::as_function(func)
  cancelled <- FALSE
  if (is.na(limit) || limit < 1)
    stop("'limit' must be a positive number")
  func2 <- function() {
    limit <<- limit - 1L
    ret <- func()
    if (is_false(ret)) cancelled <<- !ret[1]
    if (!cancelled && limit > 0)
      handle <<- later(func2, delay, loop)
  }
  handle <- later(func2, delay, loop)
  invisible(function() {
    cancelled <<- TRUE
    handle()
  })
}
