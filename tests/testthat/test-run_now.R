context("test-run_now.R")

jitter <- 0.017*2 # Compensate for imprecision in system timer

test_that("run_now waits and returns FALSE if no tasks", {
  x <- system.time({
    result <- later::run_now(0.5)
  })
  expect_gte(as.numeric(x[["elapsed"]]), 0.5 - jitter)
  expect_identical(result, FALSE)

  x <- system.time({
    result <- later::run_now(3)
  })
  expect_gte(as.numeric(x[["elapsed"]]), 3 - jitter)
  expect_identical(result, FALSE)
})

test_that("run_now returns immediately after executing a task", {
  x <- system.time({
    later::later(~{}, 0)
    result <- later::run_now(2)
  })
  expect_lt(as.numeric(x[["elapsed"]]), 0.25)
  expect_identical(result, TRUE)
})

test_that("run_now executes all scheduled tasks, not just one", {
  later::later(~{}, 0)
  later::later(~{}, 0)
  result1 <- later::run_now()
  result2 <- later::run_now()
  expect_identical(result1, TRUE)
  expect_identical(result2, FALSE)
})

test_that("run_now executes just one scheduled task, if requested", {
  result1 <- later::run_now()
  expect_identical(result1, FALSE)

  later::later(~{}, 0)
  later::later(~{}, 0)

  result2 <- later::run_now(all = FALSE)
  expect_identical(result2, TRUE)

  result3 <- later::run_now(all = FALSE)
  expect_identical(result3, TRUE)

  result4 <- later::run_now()
  expect_identical(result4, FALSE)
})

test_that("run_now doesn't go past a failed task", {
  later::later(~stop("boom"), 0)
  later::later(~{}, 0)
  expect_error(later::run_now())
  expect_true(later::run_now())
})

test_that("run_now wakes up when a background thread calls later()", {
  env <- new.env()
  Rcpp::sourceCpp(system.file("bgtest.cpp", package = "later"), env = env)
  # The background task sleeps
  env$launchBgTask(1)

  x <- system.time({
    result <- later::run_now(3)
  })
  expect_lt(as.numeric(x[["elapsed"]]), 1.25)
  expect_true(result)
})

test_that("When callbacks have tied timestamps, they respect order of creation", {
  expect_error(testCallbackOrdering(), NA)

  Rcpp::sourceCpp(code = '
    #include <Rcpp.h>
    #include <later_api.h>

    void* max_seen = 0;

    void callback(void* data) {
      if (data < max_seen) {
        Rf_error("Bad ordering detected");
      }
      max_seen = data;
    }

    // [[Rcpp::depends(later)]]
    // [[Rcpp::export]]
    void checkLaterOrdering() {
      max_seen = 0;
      for (size_t i = 0; i < 10000; i++) {
        later::later(callback, (void*)i, 0);
      }
    }
    ')
  checkLaterOrdering(); while (!later::loop_empty()) later::run_now()
})


test_that("Callbacks cannot affect the caller", {
  # This is based on a pattern used in the callCC function. Normally, simply
  # touching `throw` will cause the expression to be evaluated and f() to return
  # early. (This test does not involve later.)
  f <- function() {
    delayedAssign("throw", return(100))
    g <- function() { throw }
    g()
    return(200)
  }
  expect_equal(f(), 100)


  # When later runs callbacks, it wraps the call in R_ToplevelExec(), which
  # creates a boundary on the call stack that the early return can't cross.
  f <- function() {
    delayedAssign("throw", return(100))
    later(function() { throw })

    run_now(1)
    return(200)
  }
  expect_error(f())


  # In this case, f() should return normally, and then when g() causes later to
  # run the callback with `throw`, it should be an error -- there's no function
  # to return from because it (f()) already returned.
  f <- function() {
    delayedAssign("throw", return(100))
    later(function() { throw }, 0.5)
    return(200)
  }
  g <- function() {
    run_now(1)
  }
  expect_equal(f(), 200)
  expect_error(g())
})



test_that("interrupt and exception handling", {
  # =======================================================
  # Errors and interrupts in R callbacks
  # =======================================================

  # R error
  error_obj <- FALSE
  tryCatch(
    {
      later(function() { stop("oopsie") })
      run_now()
    },
    error = function(e) {
      error_obj <<- e
    }
  )
  expect_true(grepl("oopsie", error_obj$message))

  # interrupt
  interrupted <- FALSE
  tryCatch(
    {
      later(function() { tools::pskill(Sys.getpid(), tools::SIGINT) })
      run_now()
    },
    interrupt = function(e) {
      interrupted <<- TRUE
    }
  )
  expect_true(interrupted)




  # =======================================================
  # Exceptions in C++ callbacks
  # =======================================================

  # In these tests, in C++, later schedules a C++ callback in which an
  # exception is thrown or interrupt occurs.
  #
  # Some of these callbacks in turn call R functions.

  Rcpp::cppFunction(
    depends = "later",
    includes = '
      #include <later_api.h>
      #include <stdio.h>
      #include <sys/types.h>
      #include <unistd.h>
      #include <signal.h>

      void oof(void* data) {
        int* v = (int*)data;
        int value = *v;
        delete v;

        if (value == 1) {
          throw std::runtime_error("This is a C++ exception.");

        } else if (value == 2) {
          // Throw an arbitrary object
          throw std::string();

        } else if (value == 3) {
          // Send an interrupt to the process.
          kill(getpid(), SIGINT);
          sleep(3);

        } else if (value == 4) {
          // Calls R function via Rcpp, which sends interrupt signal and then
          // sleeps. Note: This gets converted to std::runtime_error.
          Function("r_sleep_interrupt")();

        } else if (value == 5) {
          // Calls R function via Rcpp which calls stop().
          Function("r_error")();

        } else if (value == 6) {
          // Calls the `r_error` function via R\'s C API instead of Rcpp.
          // Note: We don\'t actually use this for testing, because calling
          //       Rf_eval from an Rcpp function is inherently unsafe. If an
          //       R error occurs during the Rf_eval, a longjmp over the whole
          //       C++ stack occurs, and the subsequent code in the function
          //       never gets run. This function is just here to keep record of
          //       another way that exceptions can occur.
          SEXP e;
          PROTECT(e = Rf_lang1(Rf_install("r_error")));

          Rf_eval(e, R_GlobalEnv);

          UNPROTECT(1);
        }
      }
    ',
    code = '
      void cpp_error(int value) {
        int* v = new int(value);
        later::later(oof, v, 0);
      }
    '
  )

  r_sleep_interrupt <- function() {
    tools::pskill(Sys.getpid(), tools::SIGINT)
    Sys.sleep(3)
  }

  r_error <- function() {
    stop("oopsie")
  }

  interrupted <- FALSE
  errored     <- FALSE
  tryCatch(
    { cpp_error(1); run_now() },
    error = function(e) errored <<- TRUE
  )
  expect_true(errored)

  interrupted <- FALSE
  error_obj <- NULL
  tryCatch(
    { cpp_error(2); run_now() },
    error = function(e) errored <<- TRUE
  )
  expect_true(errored)

  interrupted <- FALSE
  tryCatch(
    { cpp_error(3); run_now() },
    interrupt = function(e) interrupted <<- TRUE
  )
  expect_true(interrupted)

  error_obj <- NULL
  tryCatch(
    { cpp_error(4); run_now() },
    error = function(e) errored <<- TRUE
  )
  expect_true(errored)

  error_obj <- NULL
  tryCatch(
    { cpp_error(5); run_now() },
    error = function(e) errored <<- TRUE
  )
  expect_true(errored)
})
