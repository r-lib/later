#' @useDynLib later, .registration=TRUE
#' @importFrom Rcpp evalCpp

.onLoad <- function(...) {
  ensureInitialized()
  .globals$next_id <- 0L
  # Store a ref to the global loop so it doesn't get GC'd.
  .globals$global_loop <- create_loop(parent = NULL)
}

.globals <- new.env(parent = emptyenv())
# A registry of weak refs to loop handle objects. Given an ID number, we can
# get the corresponding loop handle. We use weak refs because we don't want
# this registry to keep the loop objects alive.
.loops <- new.env(parent = emptyenv())

# Our own weakref functions are implemented (instead of using those from
# `rlang`) to avoid loading `rlang` automatically upon package load, as this
# causes additional overhead for packages which only link to `later`.
new_weakref <- function(loop) {
  .Call(`_later_new_weakref`, loop)
}

wref_key <- function(w) {
  .Call(`_later_wref_key`, w)
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
#' @param autorun This exists only for backward compatibility. If set to
#'   \code{FALSE}, it is equivalent to using \code{parent=NULL}.
#' @param parent The parent event loop for the one being created. Whenever the
#'   parent loop runs, this loop will also automatically run, without having to
#'   manually call \code{\link{run_now}()} on this loop. If \code{NULL}, then
#'   this loop will not have a parent event loop that automatically runs it; the
#'   only way to run this loop will be by calling \code{\link{run_now}()} on this
#'   loop.
#' @rdname create_loop
#'
#' @export
create_loop <- function(parent = current_loop(), autorun = NULL) {
  id <- .globals$next_id
  .globals$next_id <- id + 1L

  if (!is.null(autorun)) {
    # This is for backward compatibility, if `create_loop(autorun=FALSE)` is called.
    parent <- NULL
  }
  if (identical(parent, FALSE)) {
    # This is for backward compatibility, if `create_loop(FALSE)` is called.
    # (Previously the first and only parameter was `autorun`.)
    parent <- NULL
    warning("create_loop(FALSE) is deprecated. Please use create_loop(parent=NULL) from now on.")
  }
  if (!is.null(parent) && !inherits(parent, "event_loop")) {
    stop("`parent` must be NULL or an event_loop object.")
  }

  if (is.null(parent)) {
    parent_id <- -1L
  } else {
    parent_id <- parent$id
  }
  createCallbackRegistry(id, parent_id)

  # Create the handle for the loop
  loop <- new.env(parent = emptyenv())
  class(loop) <- "event_loop"
  loop$id <- id
  lockBinding("id", loop)

  # Add a weak reference to the loop object in our registry.
  .loops[[sprintf("%d", id)]] <- new_weakref(loop)

  if (id != 0L) {
    # Inform the C++ layer that there are no more R references when the handle
    # is GC'd (unless it's the global loop.) The global loop handle never gets
    # GC'd under normal circumstances because .globals$global_loop refers to it.
    # However, if the package is unloaded it can get GC'd, and we don't want the
    # destroy_loop() finalizer to give an error message about not being able to
    # destroy the global loop.
    reg.finalizer(loop, notify_r_ref_deleted)
  }

  loop
}

notify_r_ref_deleted <- function(loop) {
  if (identical(loop, global_loop())) {
    stop("Can't notify that reference to global loop is deleted.")
  }

  res <- notifyRRefDeleted(loop$id)
  if (res) {
    rm(list = sprintf("%d", loop$id), envir = .loops)
  }
  invisible(res)
}

#' @rdname create_loop
#' @export
destroy_loop <- function(loop) {
  if (identical(loop, global_loop())) {
    stop("Can't destroy global loop.")
  }

  res <- deleteCallbackRegistry(loop$id)
  if (res) {
    rm(list = sprintf("%d", loop$id), envir = .loops)
  }
  invisible(res)
}

#' @rdname create_loop
#' @export
exists_loop <- function(loop) {
  existsCallbackRegistry(loop$id)
}

#' @rdname create_loop
#' @export
current_loop <- function() {
  id <- getCurrentRegistryId()
  loop_weakref <- .loops[[sprintf("%d", id)]]
  if (is.null(loop_weakref)) {
    stop("Current loop with id ", id, " not found.")
  }

  loop <- wref_key(loop_weakref)
  if (is.null(loop)) {
    stop("Current loop with id ", id, " not found.")
  }

  loop
}

#' @rdname create_loop
#' @export
with_temp_loop <- function(expr) {
  loop <- create_loop(parent = NULL)
  on.exit(destroy_loop(loop))

  with_loop(loop, expr)
}

#' @rdname create_loop
#' @export
with_loop <- function(loop, expr) {
  if (!exists_loop(loop)) {
    stop("loop has been destroyed!")
  }
  old_loop <- current_loop()
  if (!identical(loop, old_loop)) {
    on.exit(setCurrentRegistryId(old_loop$id), add = TRUE)
    setCurrentRegistryId(loop$id)
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
  str <- paste0("<event loop> ID: ", x$id)
  if (!exists_loop(x)) {
    str <- paste(str, "(destroyed)")
  }
  str
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
  # `rlang::as_function` is used conditionally so that `rlang` is not loaded
  # until used, avoiding this overhead for packages only linking to `later`
  if (!is.function(func)) {
    func <- rlang::as_function(func)
  }
  id <- execLater(func, delay, loop$id)

  invisible(create_canceller(id, loop$id))
}

#' Executes a function when a file descriptor is ready
#'
#' Schedule an R function or formula to run after an indeterminate amount of
#' time when file descriptors are ready for reading or writing, subject to an
#' optional timeout.
#'
#' @param func A function that takes a single argument, a logical vector that
#'   indicates which file descriptors are ready (a concatenation of `readfds`,
#'   `writefds` and `exceptfds`). This may be all `FALSE` if the
#'   `timeout` argument is non-`Inf`. Invalid file descriptors (such as those
#'   already closed) are returned as `NA`, as are `readfds` and `writefds` with
#'   error conditions pending.
#' @param readfds Integer vector of file descriptors, or Windows `SOCKET`s to
#'   monitor for being ready to read.
#' @param writefds Integer vector of file descriptors, or Windows `SOCKET`s to
#'   monitor being ready to write.
#' @param exceptfds Integer vector of file descriptors, or Windows `SOCKET`s to
#'   monitor for error conditions pending.
#' @param timeout Number of seconds to wait before giving up, and calling `func`
#'   with all `FALSE`.
#' @param loop A handle to an event loop. Defaults to the currently-active loop.
#'
#' @inherit later return note
#'
#' @examplesIf requireNamespace("nanonext", quietly = TRUE)
#' # create nanonext sockets
#' s1 <- nanonext::socket(listen = "inproc://nano")
#' s2 <- nanonext::socket(dial = "inproc://nano")
#' fd1 <- nanonext::opt(s1, "recv-fd")
#' fd2 <- nanonext::opt(s2, "recv-fd")
#'
#' # 1. timeout: prints FALSE, FALSE
#' later_fd(print, c(fd1, fd2), timeout = 0.1)
#' Sys.sleep(0.2)
#' run_now()
#'
#' # 2. fd1 ready: prints TRUE, FALSE
#' later_fd(print, c(fd1, fd2), timeout = 1)
#' res <- nanonext::send(s2, "msg")
#' Sys.sleep(0.1)
#' run_now()
#'
#' # 3. both ready: prints TRUE, TRUE
#' res <- nanonext::send(s1, "msg")
#' later_fd(print, c(fd1, fd2), timeout = 1)
#' Sys.sleep(0.1)
#' run_now()
#'
#' # 4. fd2 ready: prints FALSE, TRUE
#' res <- nanonext::recv(s1)
#' later_fd(print, c(fd1, fd2), timeout = 1)
#' Sys.sleep(0.1)
#' run_now()
#'
#' # 5. fds invalid: prints NA, NA
#' close(s2)
#' close(s1)
#' later_fd(print, c(fd1, fd2), timeout = 1)
#' Sys.sleep(0.1)
#' run_now()
#'
#' @export
later_fd <- function(func, readfds = integer(), writefds = integer(), exceptfds = integer(),
                     timeout = Inf, loop = current_loop()) {
  if (!is.function(func)) {
    func <- rlang::as_function(func)
  }
  invisible(execLater_fd(func, readfds, writefds, exceptfds, timeout, loop$id))
}

# Returns a function that will cancel a callback with the given ID. If the
# callback has already been executed or canceled, then the function has no
# effect.
create_canceller <- function(id, loop_id) {
  force(id)
  force(loop_id)
  function() {
    invisible(cancel(id, loop_id))
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

  invisible(execCallbacks(timeoutSecs, all, loop$id))
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
