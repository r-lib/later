context("test-recurring")

test_that("Limited recurrence", {
  # Repeat until the limit reached, stopped by limit
  x <- 0
  later_recurring(function() { x <<- x + 1 }, delay = 0.5, limit = 2)
  run_now(0.5)
  expect_identical(x, 1)
  run_now(0.5)
  expect_identical(x, 2)
  run_now(1)
  expect_identical(x, 2)
})

test_that("Self-cancelling recurrence", {
  # Repeat until the function returns FALSE, self-cancelling
  x <- 0
  cancel <- later_recurring(function() { x <<- x + 1; (x < 2) }, delay = 0.5, limit = 4)
  expect_identical(length(list_queue()), 1L)
  run_now(0.5)
  expect_identical(x, 1)
  run_now(0.5)
  expect_identical(x, 2)
  run_now(1)
  expect_identical(x, 2)
  expect_identical(length(list_queue()), 0L)
})
