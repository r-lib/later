#' Get and set logging level
#'
#' The logging level for later can be set to report differing levels of
#' information. Possible logging levels (from least to most information
#' reported) are: \code{"OFF"}, \code{"ERROR"}, \code{"WARN"}, \code{"INFO"}, or
#' \code{"DEBUG"}. The default level is \code{ERROR}.
#'
#' @param level The logging level. Must be one of \code{NULL}, \code{"OFF"},
#'   \code{"ERROR"}, \code{"WARN"}, \code{"INFO"}, or \code{"DEBUG"}. If
#'   \code{NULL} (the default), then this function simply returns the current
#'   logging level.
#'
#' @return If \code{level=NULL}, then this returns the current logging level. If
#'   \code{level} is any other value, then this returns the previous logging
#'   level, from before it is set to the new value.
#'
#' @keywords internal
logLevel <- function(level = NULL) {
  if (is.null(level)) {
    level <- ""
    log_level("")
  } else {
    level <- match.arg(level, c("OFF", "ERROR", "WARN", "INFO", "DEBUG"))
    invisible(log_level(level))
  }
}
