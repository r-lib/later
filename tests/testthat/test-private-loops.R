context("test-private-loops.R")

describe("Private event loop", {
  it("changes current_loop()", {
    expect_identical(current_loop(), global_loop())

    with_temp_loop({
      expect_false(identical(current_loop(), global_loop()))
    })
  })

  it("runs only its own tasks", {
    x <- 0
    later(~{x <<- 1}, 0)
    with_temp_loop({
      expect_true(loop_empty())

      later(~{x <<- 2})
      run_now()

      expect_identical(x, 2)

      run_now(loop = global_loop())
      expect_identical(x, 1)
    })
  })
})



test_that("Private event loops", {
  l <- create_loop(autorun = FALSE)
  x <- 0

  expect_true(exists_loop(l))

  with_loop(l, {
    later(function() x <<- x + 1 )
    run_now()
  })
  expect_equal(x, 1)

  with_loop(l, {
    later(function() x <<- x + 1 )
    run_now()

    later(function() x <<- x + 1 )
    later(function() x <<- x + 1 )
  })
  expect_equal(x, 2)

  with_loop(l, run_now())
  expect_equal(x, 4)

  destroy_loop(l)
  expect_false(exists_loop(l))

  # Can't run later-y things with destroyed loop
  expect_error(with_loop(l, later(function() message("foo"))))
  expect_error(with_loop(l, run_now()))

  # GC with functions in destroyed loops, even if callback isn't executed.
  l <- create_loop(autorun = FALSE)
  x <- 0
  gc()
  with_loop(l, {
    later(
      local({
        reg.finalizer(environment(), function(e) x <<-x + 1)
        function() message("foo")
      })
    )
  })
  gc()
  expect_identical(x, 0)

  destroy_loop(l)
  gc()
  expect_identical(x, 1)


  # A GC'd loop object will cause its queue to be deleted, which will allow GC
  # of any resources
  l <- create_loop(autorun = FALSE)
  x <- 0
  gc()
  with_loop(l, {
    later(
      local({
        reg.finalizer(environment(), function(e) x <<-x + 1)
        function() message("foo")
      })
    )
  })
  gc()
  expect_identical(x, 0)

  # Delete the reference to the loop, and GC. This causes the queue to be
  # deleted, which removes references to items in the queue. However, the items
  # in the queue won't be GC'd yet. (At least not as of R 3.5.2.)
  rm(l)
  gc()
  expect_identical(x, 0)

  # A second GC triggers the finalizer for an item that was in the queue.
  gc()
  expect_identical(x, 1)


  # Can't destroy global loop
  expect_error(destroy_loop(global_loop()))
})


test_that("Temporary event loops", {
  l <- NULL
  x <- 0
  with_temp_loop({
    l <- current_loop()
    later(function() x <<- x + 1 )
    run_now()
  })

  expect_false(exists_loop(l))
  expect_error(with_loop(l, {
    later(function() x <<- x + 1 )
    run_now()
  }))

  # Test GC
  # Make sure that items captured in later callbacks are GC'd after the callback
  # is executed.
  x <- 0
  with_temp_loop({
    later(
      local({
        reg.finalizer(environment(), function(e) x <<-x + 1)
        function() 1
      })
    )
    gc()
    run_now()
  })
  expect_identical(x, 0)
  gc()
  expect_identical(x, 1)

  # Test that objects are GC'd after loop is destroyed, even if callback hasn't
  # been executed.
  x <- 0
  with_temp_loop({
    later(
      local({
        reg.finalizer(environment(), function(e) x <<-x + 1)
        function() 1
      })
    )
    run_now()

    later(
      local({
        e <- environment()
        reg.finalizer(environment(), function(e) x <<-x + 1)
        function() 1
      })
    )
    gc()
  })
  expect_identical(x, 1)
  gc()
  expect_identical(x, 2)
})

test_that("Destroying loop and loop ID", {
  l <- create_loop()
  expect_true(is.integer(loop_id(l)))
  destroy_loop(l)
  expect_null(loop_id(l))
})

test_that("Can't destroy current loop", {
  errored <- FALSE
  with_temp_loop({
    later(function() {
      # We can't do expect_error in a later() callback, so use a tryCatch
      # instead to check that an error occurs.
      tryCatch(
        destroy_loop(current_loop()),
        error = function(e) { errored <<- TRUE }
      )
    })
    run_now()
  })

  expect_true(errored)
})

test_that("Can't GC current loop", {
  collected <- FALSE
  l <- create_loop()
  reg.finalizer(l, function(x) { collected <<- TRUE })
  with_loop(l, {
    rm(l, inherits = TRUE)
    gc()
    gc()
  })
  expect_false(collected)
  gc()
  expect_true(collected)
})


test_that("When auto-running a child loop, it will be reported as current_loop()", {
  l <- create_loop(autorun = TRUE, parent = global_loop())
  x <- NULL
  later(function() { x <<- current_loop() }, loop = l)
  run_now(loop = global_loop())
  expect_identical(x, l)
})


test_that("Auto-running grandchildren loops", {
  l1_ran  <- FALSE
  l11_ran <- FALSE
  l12_ran <- FALSE
  l13_ran <- FALSE
  l2_ran  <- FALSE
  l21_ran <- FALSE
  l22_ran <- FALSE
  l23_ran <- FALSE

  l1 <- create_loop()
  l2 <- create_loop(parent = NULL)

  # l1 should auto-run, along with l11 and l12. l13 should not, because it has
  # no parent.
  with_loop(l1, {
    later(function() l1_ran <<- TRUE)
    l11 <- create_loop()
    l12 <- create_loop()
    l13 <- create_loop(parent = NULL)
    later(function() l11_ran <<- TRUE, loop = l11)
    later(function() l12_ran <<- TRUE, loop = l12)
    later(function() l13_ran <<- TRUE, loop = l13)
  })

  # None of these should auto-run, because l2 has no parent.
  with_loop(l2, {
    later(function() l2_ran <<- TRUE)
    l21 <- create_loop()
    l22 <- create_loop()
    l23 <- create_loop(parent = NULL)
    later(function() l21_ran <<- TRUE, loop = l21)
    later(function() l22_ran <<- TRUE, loop = l22)
    later(function() l23_ran <<- TRUE, loop = l23)
  })

  run_now()
  expect_true(l1_ran)
  expect_true(l11_ran)
  expect_true(l12_ran)
  expect_false(l13_ran)
  expect_false(l2_ran)
  expect_false(l21_ran)
  expect_false(l22_ran)
  expect_false(l23_ran)
})

test_that("Grandchildren loops whose parent is destroyed should not autorun", {
  l_ran  <- FALSE
  l1_ran <- FALSE
  l <- create_loop()

  with_loop(l, {
    later(function() l_ran <<- TRUE)
    l1 <- create_loop()
    later(function() l1_ran <<- TRUE, loop = l1)
  })

  destroy_loop(l)
  run_now()
  expect_false(l_ran)
  expect_false(l1_ran)


  # Similar to previous, but instead of destroy_loop(), we rm() the loop and
  # gc().
  l_ran  <- FALSE
  l1_ran <- FALSE
  l <- create_loop()

  with_loop(l, {
    later(function() l_ran <<- TRUE)
    l1 <- create_loop()
    later(function() l1_ran <<- TRUE, loop = l1)
  })
  # The rm() and gc() causes the loop to be destroyed.
  rm(l)
  gc()
  run_now()
  expect_false(l_ran)
  expect_false(l1_ran)
})

test_that("list_queue", {
  l <- create_loop(autorun = FALSE)
  q <- NULL
  f <- function() 1  # A dummy function

  with_loop(l, {
    later(f)
    q <- list_queue()
  })
  expect_equal(length(q), 1)
  expect_identical(q[[1]]$callback, f)

  with_loop(l, {
    run_now()
    q <- list_queue()
  })
  expect_equal(length(q), 0)

  with_loop(l, {
    later(f)
    later(f)
    later(sum)
    q <- list_queue()
  })
  expect_equal(length(q), 3)
  expect_identical(q[[1]]$callback, f)
  expect_identical(q[[2]]$callback, f)
  expect_identical(q[[3]]$callback, sum)

  # Empty the queue by calling run now. Also test calling list_queue by passing
  # in a loop handle.
  with_loop(l, run_now())
  q <- list_queue(l)
  expect_equal(length(q), 0)
})

# TODO:
# if interrupt occurs, make sure current_loop doesn't get stuck
# Make sure children of a deleted loop are abandoned
