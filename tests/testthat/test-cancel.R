context("test-cancel.R")

test_that("Cancelling callbacks", {
  # Cancel with zero delay
  x <- 0
  cancel <- later(function() { x <<- x + 1 })
  later(function() { x <<- x + 2 })
  expect_true(cancel())
  run_now()
  expect_identical(x, 2)

  # Cancel with zero delay
  x <- 0
  cancel <- later(function() { x <<- x + 1 }, 1)
  run_now(0.25)
  expect_true(cancel())
  run_now(1)
  expect_identical(x, 0)

  # Make sure a cancelled callback doesn't interfere with others
  x <- 0
  later(function() { x <<- x + 1 }, 1)
  cancel <- later(function() { x <<- x + 2 }, 0.5)
  run_now()
  expect_true(cancel())
  run_now(2)
  expect_identical(x, 1)
})


test_that("Cancelled functions will be GC'd", {
  x <- 0
  cancel <- later(
    local({
      reg.finalizer(environment(), function(e) x <<- x + 1)
      function() message("foo")
    })
  )
  expect_true(cancel())
  gc()
  expect_identical(x, 1)
})


test_that("Cancelling executed or cancelled callbacks has no effect", {
  # Cancelling an executed callback
  x <- 0
  cancel <- later(function() { x <<- x + 1 })
  run_now()
  expect_false(cancel())
  run_now()
  expect_identical(x, 1)

  # Cancelling twice
  x <- 0
  cancel <- later(function() { x <<- x + 1 })
  expect_true(cancel())
  expect_false(cancel())
  run_now()
  expect_identical(x, 0)
})


test_that("Cancelling callbacks on temporary event loops", {
  with_temp_loop({
    # Cancelling an executed callback
    x <- 0
    cancel <- later(function() { x <<- x + 1 })
    run_now()
    expect_false(cancel())
    run_now()
    expect_identical(x, 1)
  })

  with_temp_loop({
    # Cancelling twice
    x <- 0
    cancel <- later(function() { x <<- x + 1 })
    expect_true(cancel())
    expect_false(cancel())
    run_now()
    expect_identical(x, 0)
  })

  with_temp_loop({
    # Make sure a cancelled callback doesn't interfere with others
    x <- 0
    later(function() { x <<- x + 1 }, 1)
    cancel <- later(function() { x <<- x + 2 }, 0.5)
    run_now()
    expect_true(cancel())
    run_now(2)
    expect_identical(x, 1)
  })

  # Canceling after an event loop handle has been destroyed: the underlying
  # data structure (in C++) will be deleted, along with the callbacks. This is
  # true because the loop does not have a parent.
  cancel <- NULL
  x <- 0
  with_temp_loop({
    cancel <- later(function() { x <<- x + 1 })
  })
  expect_false(cancel())
  expect_identical(x, 0)
})


test_that("Cancelling callbacks on persistent private loops without parent", {
  l1 <- create_loop(parent = NULL)
  l2 <- create_loop(parent = NULL)

  # Cancel from outside with_loop
  cancel <- NULL
  x <- 0
  with_loop(l1, {
    cancel <- later(function() { x <<- x + 1 })
  })
  expect_true(cancel())
  expect_false(cancel())
  with_loop(l1, run_now())
  expect_false(cancel())
  expect_identical(x, 0)


  # Make sure it doesn't interfere with other event loops
  with_loop(l1, {
    cancel <- later(function() { x <<- x + 1 })
  })
  with_loop(l2, {
    later(function() { x <<- x + 2 })
  })
  later(function() { x <<- x + 4 })
  expect_true(cancel())
  with_loop(l1, run_now())
  with_loop(l2, run_now())
  run_now()
  expect_identical(x, 6)


  # Cancelling on an explicitly destroyed loop returns FALSE
  l3 <- create_loop(parent = NULL)
  cancel <- NULL
  x <- 0
  with_loop(l3, {
    cancel <- later(function() { x <<- x + 1 })
  })
  destroy_loop(l3)
  expect_false(cancel())
  expect_identical(x, 0)
})

test_that("Cancelling callbacks on persistent private loops with parent", {
  # If the loop handle is GC'd but the loop _does have_ a parent, then the
  # underlying objects will not be destroyed right away, so the cancel() will
  # work.
  cancel <- NULL
  x <- 0
  local({
    l1 <- create_loop(parent = current_loop())
    cancel <<- later(function() { x <<- x + 1 }, loop = l1)
  })
  expect_true(cancel())
  expect_false(cancel())
  expect_identical(x, 0)
})

test_that("A canceler will not keep loop alive", {
  l <- create_loop(parent = NULL)
  finalized <- FALSE

  reg.finalizer(l, function(x) finalized <<- TRUE)
  cancel <- later(function() 1, loop = l)
  rm(l)
  gc()
  expect_true(finalized)
})
