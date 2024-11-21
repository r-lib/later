#include "debug.h"
#include "utils.h"
#include <R.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

// For debug.h
#if defined(DEBUG_THREAD)
tct_thrd_t __main_thread__;
tct_thrd_t __background_thread__;
#endif


// It's not safe to call REprintf from the background thread but we need some
// way to output error messages. R CMD check does not it if the code uses the
// symbols stdout, stderr, and printf, so this function is a way to avoid
// those. It's to calling `fprintf(stderr, ...)`.
void err_printf(const char *fmt, ...) {
  const size_t max_size = 4096;
  char buf[max_size];

  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(buf, max_size, fmt, args);
  va_end(args);

  if (n == -1)
    return;

  if (write(STDERR_FILENO, buf, n)) {}
  // This is here simply to avoid a warning about "ignoring return value" of
  // the write(), on some compilers. (Seen with gcc 4.4.7 on RHEL 6)
}

// Set the default log level
LogLevel log_level_ = LOG_ERROR;


// Sets the current log level and returns previous value.
// [[Rcpp::export]]
std::string log_level(std::string level) {
  LogLevel old_level = log_level_;

  if (level == "") {
    // Do nothing
  } else if (level == "OFF") {
    log_level_ = LOG_OFF;
  } else if (level == "ERROR") {
    log_level_ = LOG_ERROR;
  } else if (level == "WARN") {
    log_level_ = LOG_WARN;
  } else if (level == "INFO") {
    log_level_ = LOG_INFO;
  } else if (level == "DEBUG") {
    log_level_ = LOG_DEBUG;
  } else {
    Rf_error("Unknown value for `level`");
  }

  switch(old_level) {
    case LOG_OFF:   return "OFF";
    case LOG_ERROR: return "ERROR";
    case LOG_WARN:  return "WARN";
    case LOG_INFO:  return "INFO";
    case LOG_DEBUG: return "DEBUG";
    default:        return "";
  }
}

// Reports whether package was compiled with UBSAN
// [[Rcpp::export]]
bool using_ubsan() {
#ifdef USING_UBSAN
  return true;
#else
  return false;
#endif
}
