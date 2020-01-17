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


test_that("When running a child loop, it will be reported as current_loop()", {
  l <- create_loop(autorun = TRUE, parent = global_loop())
  x <- NULL
  later(function() { x <<- current_loop() }, loop = l)
  run_now(loop = global_loop())
  expect_identical(x, l)
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
