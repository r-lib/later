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
  l <- create_loop(autorun = FALSE)
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

  # Canceling after an event loop has been destroyed should return FALSE
  cancel <- NULL
  x <- 0
  with_temp_loop({
    cancel <- later(function() { x <<- x + 1 })
  })
  expect_false(cancel())
  expect_identical(x, 0)
})


test_that("Cancelling callbacks on persistent private loops", {
  l1 <- create_loop(autorun = FALSE)
  l2 <- create_loop(autorun = FALSE)

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
  l3 <- create_loop(autorun = FALSE)
  cancel <- NULL
  x <- 0
  with_loop(l3, {
    cancel <- later(function() { x <<- x + 1 })
  })
  destroy_loop(l3)
  expect_false(cancel())
  expect_identical(x, 0)
})

test_that("Canceling a callback from another a callback", {
  # Canceling a callback from another callback should work. Additionally, the
  # altered ordering of callbacks shouldn't prevent other callbacks from
  # running. In this test, #1 cancels 2 and 3, but we still expect 4 to run. If
  # we used the wrong algorithm for traversing the queue and canceling
  # callbacks, it would be possible for the cancellation of 2 and 3 to cause 4
  # to not run. This test ensures that we do it the right way.
  ran_2 <- FALSE
  ran_3 <- FALSE
  ran_4 <- FALSE
  with_temp_loop({
    cancel_1 <- later(function() { cancel_2(); cancel_3() })
    cancel_2 <- later(function() { ran_2 <<- TRUE })
    cancel_3 <- later(function() { ran_3 <<- TRUE })
    later(function() { ran_4 <<- TRUE })
    run_now()
  })

  expect_false(ran_2)
  expect_false(ran_3)
  expect_true(ran_4)
})
