#' @useDynLib later
#' @import Rcpp
#' @importFrom Rcpp evalCpp

.onLoad <- function(...) {
  ensureInitialized()
  .globals$next_id <- 0L
  .globals$global_loop <- create_loop()
  .globals$current_loop <- .globals$global_loop
}

.globals <- new.env(parent = emptyenv())

#' Private event loops
#'
#' Normally, later uses a global event loop for scheduling and running
#' functions. However, in some cases, it is useful to create a \emph{private}
#' event loop to schedule and execute tasks without disturbing the global event
#' loop. For example, you might have asynchronous code that queries a remote
#' data source, but want to wait for a full back-and-forth communication to
#' complete before continuing in your code -- from the caller's perspective, it
#' should behave like synchronous code, and not do anything with the global
#' event loop (which could run code unrelated to your operation). To do this,
#' you would run your asynchronous code using a private event loop.
#'
#' \code{create_loop} creates and returns a handle to a private event loop,
#' which is useful when for scheduling tasks when you do not want to interfere
#' with the global event loop.
#'
#' \code{destory_loop} destroys a private event loop.
#'
#' \code{exists_loop} reports whether an event loop exists -- that is, that it
#' has not been destroyed.
#'
#' \code{current_loop} returns the currently-active event loop. Any calls to
#' \code{\link{later}()} or \code{\link{run_now}()} will use the current loop by
#' default.
#'
#' \code{with_loop} evaluates an expression with a given event loop as the
#' currently-active loop.
#'
#' \code{with_temp_loop} creates an event loop, makes it the current loop, then
#' evaluates the given expression. Afterwards, the new event loop is destroyed.
#'
#' \code{global_loop} returns a handle to the global event loop.
#'
#'
#' @param loop A handle to an event loop.
#' @param expr An expression to evaluate.
#' @rdname create_loop
#'
#' @export
create_loop <- function() {
  id <- .globals$next_id
  .globals$next_id <- id + 1L
  createCallbackRegistry(id)

  # Create the handle for the loop
  loop <- new.env(parent = emptyenv())
  class(loop) <- "event_loop"
  loop$id <- id
  lockBinding("id", loop)
  # Automatically destroy the loop when the handle is GC'd
  reg.finalizer(loop, destroy_loop)

  loop
}

#' @rdname create_loop
#' @export
destroy_loop <- function(loop) {
  if (identical(loop, global_loop())) {
    stop("Can't destroy global loop.")
  }
  # Make sure we don't destroy a loop twice
  if (exists_loop(loop)) {
    deleteCallbackRegistry(loop$id)
  }
}

#' @rdname create_loop
#' @export
exists_loop <- function(loop) {
  existsCallbackRegistry(loop$id)
}

#' @rdname create_loop
#' @export
current_loop <- function() {
  .globals$current_loop
}

#' @rdname create_loop
#' @export
with_temp_loop <- function(expr) {
  loop <- create_loop()
  on.exit(destroy_loop(loop))

  with_loop(loop, expr)
}

#' @rdname create_loop
#' @export
with_loop <- function(loop, expr) {
  if (!identical(loop, current_loop())) {
    old_loop <- .globals$current_loop
    .globals$current_loop <- loop
    on.exit(.globals$current_loop <- old_loop, add = TRUE)
  }

  force(expr)
}

#' @rdname create_loop
#' @export
global_loop <- function() {
  .globals$global_loop
}


#' @export
format.event_loop <- function(x, ...) {
  paste0("<event loop>\n  id: ", x$id)
}

#' @export
print.event_loop <- function(x, ...) {
  cat(format(x, ...))
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
#' @param loop A handle to an event loop. Defaults to the currently-active loop.
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
later <- function(func, delay = 0, loop = current_loop()) {
  f <- rlang::as_function(func)
  execLater(f, delay, loop$id)
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
#' or control returns to the R prompt). You must use your own
#' [tryCatch][base::conditions] if you want to handle errors.
#'
#' @param timeoutSecs Wait (block) for up to this number of seconds waiting for
#'   an operation to be ready to run. If `0`, then return immediately if there
#'   are no operations that are ready to run. If `Inf` or negative, then wait as
#'   long as it takes (if none are scheduled, then this will block forever).
#' @param all If `FALSE`, `run_now()` will execute at most one scheduled
#'   operation (instead of all eligible operations). This can be useful in cases
#'   where you want to interleave scheduled operations with your own logic.
#' @param loop A handle to an event loop. Defaults to the currently-active loop.
#'
#' @return A logical indicating whether any callbacks were actually run.
#'
#' @export
run_now <- function(timeoutSecs = 0L, all = TRUE, loop = current_loop()) {
  if (timeoutSecs == Inf) {
    timeoutSecs <- -1
  }

  if (!is.numeric(timeoutSecs))
    stop("timeoutSecs must be numeric")

  with_loop(loop,
    invisible(execCallbacks(timeoutSecs, all, loop$id))
  )
}

#' Check if later loop is empty
#'
#' Returns true if there are currently no callbacks that are scheduled to
#' execute in the present or future.
#'
#' @inheritParams create_loop
#' @keywords internal
#' @export
loop_empty <- function(loop = current_loop()) {
  idle(loop$id)
}

#' Relative time to next scheduled operation
#'
#' Returns the duration between now and the earliest operation that is currently
#' scheduled, in seconds. If the operation is in the past, the value will be
#' negative. If no operation is currently scheduled, the value will be `Inf`.
#'
#' @inheritParams create_loop
#' @export
next_op_secs <- function(loop = current_loop()) {
  nextOpSecs(loop$id)
}


# Get the contents of an event loop, as a list. (For debugging only, so it's
# not exported.)
list_queue <- function(loop = current_loop()) {
  list_queue_(loop$id)
}
