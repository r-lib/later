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
