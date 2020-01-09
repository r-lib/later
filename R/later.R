#' @useDynLib later
#' @import Rcpp
#' @importFrom Rcpp evalCpp

.onLoad <- function(...) {
  ensureInitialized()
  .globals$next_id <- 0L
  .globals$global_loop <- create_loop(autorun = FALSE, parent = NULL)
  .globals$current_loop <- .globals$global_loop
}

.globals <- new.env(parent = emptyenv())

# Parent-child mappings
# .globals$parents is an environment of integer vectors
.globals$relations <- new.env(parent = emptyenv())
.globals$relations[["0"]] <- integer(0)

find_children <- function(id) {
  res <- .globals$relations[[as.character(id)]]
  if (is.null(res)) {
    res <- integer(0)
  }
  res
}

register_relationship <- function(parent_id, child_id) {
  relations <- .globals$relations
  parent_id_str <- as.character(parent_id)

  if (is.null(relations[[parent_id_str]])) {
    relations[[parent_id_str]] <- integer(0)
  }

  relations[[parent_id_str]][length(relations[[parent_id_str]]) + 1L] <- child_id
}

deregister_parent <- function(id) {
  if (exists(as.character(id), envir = .globals$relations)) {
    rm(list = as.character(id), envir = .globals$relations)
  }
}

deregister_child <- function(id) {
  # Super crude: iterate over all parents until child is found; then remove it.
  parent_ids <- ls(.globals$relations, all.names = TRUE, sorted = FALSE)
  for (parent_id in parent_ids) {
    idx <- match(id, .globals$relations[[parent_id]])
    if (!is.na(idx)) {
      .globals$relations[[parent_id]] <- .globals$relations[[parent_id]][-idx]
      return()
    }
  }

  message("deregister_child: id ", id, " not found.")
}


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
#' \code{destroy_loop} destroys a private event loop.
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
#' @param autorun Should this event loop automatically be run when its parent
#'   loop runs? Currently, only FALSE is allowed, but in the future TRUE will be
#'   implemented and the default. Because in the future the default will change,
#'   for now any code that calls \code{create_loop} must explicitly pass in
#'   \code{autorun=FALSE}.
#' @param parent The parent event loop for the one being created. If
#'   \code{autorun} is \code{TRUE}, then whenever the parent loop runs, this
#'   loop will also automatically run, without having to manually call
#'   \code{\link{run_now}()}. TODO: Maybe we don't need the autorun param at
#'   all?
#' @rdname create_loop
#'
#' @export
create_loop <- function(autorun = TRUE, parent = current_loop()) {
  # if (!identical(autorun, FALSE)) {
  #   stop("autorun must be set to FALSE (until TRUE is implemented).")
  # }

  id <- .globals$next_id
  .globals$next_id <- id + 1L
  createCallbackRegistry(id)

  # Create the handle for the loop
  loop <- new.env(parent = emptyenv())
  class(loop) <- "event_loop"
  loop$id <- id
  lockBinding("id", loop)
  if (id != 0L) {
    # Automatically destroy the loop when the handle is GC'd (unless it's the
    # global loop.) The global loop handle never gets GC'd under normal
    # circumstances because .globals$global_loop refers to it. However, if the
    # package is unloaded it can get GC'd, and we don't want the
    # destroy_loop() finalizer to give an error message about not being able
    # to destroy the global loop.
    reg.finalizer(loop, destroy_loop)
  }

  if (autorun && !is.null(parent)) {
    register_relationship(parent$id, id)
  }

  loop
}

#' @rdname create_loop
#' @export
destroy_loop <- function(loop) {
  if (identical(loop, global_loop())) {
    stop("Can't destroy global loop.")
  }

  deleteCallbackRegistry(loop$id)
  deregister_parent(loop$id)
  deregister_child(loop$id)
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
  loop <- create_loop(autorun = FALSE)
  on.exit(destroy_loop(loop))

  with_loop(loop, expr)
}

#' @rdname create_loop
#' @export
with_loop <- function(loop, expr) {
  if (!identical(loop, current_loop())) {
    old_loop <- .globals$current_loop
    on.exit(.globals$current_loop <- old_loop, add = TRUE)
    .globals$current_loop <- loop
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
#' @return A function, which, if invoked, will cancel the callback. The
#'   function will return \code{TRUE} if the callback was successfully
#'   cancelled and \code{FALSE} if not (this occurs if the callback has
#'   executed or has been cancelled already).
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
  id <- execLater(f, delay, loop$id)

  invisible(create_canceller(id, loop))
}

# Returns a function that will cancel a callback with the given ID. If the
# callback has already been executed or canceled, then the function has no
# effect.
create_canceller <- function(id, loop) {
  function() {
    invisible(cancel(id, loop$id))
  }
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


#' Get the contents of an event loop, as a list
#'
#' This function is for debugging only.
#'
#' @keywords internal
list_queue <- function(loop = current_loop()) {
  list_queue_(loop$id)
}
