context("test-run_now.R")

test_that("run_now waits and returns FALSE if no tasks", {
  x <- system.time({
    result <- later::run_now(0.5)
  })
  expect_gte(as.numeric(x[["elapsed"]]), 0.499)
  expect_identical(result, FALSE)

  x <- system.time({
    result <- later::run_now(3)
  })
  expect_gte(as.numeric(x[["elapsed"]]), 2.999)
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
