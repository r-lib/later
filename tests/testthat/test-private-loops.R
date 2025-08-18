test_that("Private event loop changes current_loop()", {
  expect_identical(current_loop(), global_loop())

  with_temp_loop({
    expect_false(identical(current_loop(), global_loop()))
  })
})

test_that("Private event loop runs only its own tasks", {
  x <- 0
  later(
    ~ {
      x <<- 1
    },
    0
  )
  with_temp_loop({
    expect_true(loop_empty())

    later(
      ~ {
        x <<- 2
      }
    )
    run_now()

    expect_identical(x, 2)

    run_now(loop = global_loop())
    expect_identical(x, 1)
  })
})

test_that("Private event loops", {
  l <- create_loop(parent = NULL)
  x <- 0

  expect_true(exists_loop(l))

  with_loop(l, {
    later(function() x <<- x + 1)
    run_now()
  })
  expect_equal(x, 1)

  with_loop(l, {
    later(function() x <<- x + 1)
    run_now()

    later(function() x <<- x + 1)
    later(function() x <<- x + 1)
  })
  expect_equal(x, 2)

  with_loop(l, run_now())
  expect_equal(x, 4)

  destroy_loop(l)
  expect_false(exists_loop(l))

  # Can't run later-y things with destroyed loop
  expect_snapshot(error = TRUE, with_loop(l, later(function() message("foo"))))
  expect_snapshot(error = TRUE, with_loop(l, run_now()))

  # GC with functions in destroyed loops, even if callback isn't executed.
  l <- create_loop(parent = NULL)
  x <- 0
  gc()
  with_loop(l, {
    later(
      local({
        reg.finalizer(environment(), function(e) x <<- x + 1)
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
  l <- create_loop(parent = NULL)
  x <- 0
  gc()
  with_loop(l, {
    later(
      local({
        reg.finalizer(environment(), function(e) x <<- x + 1)
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
  expect_snapshot(error = TRUE, destroy_loop(global_loop()))
})


test_that("Temporary event loops", {
  l <- NULL
  x <- 0
  with_temp_loop({
    l <- current_loop()
    later(function() x <<- x + 1)
    run_now()
  })

  expect_false(exists_loop(l))
  expect_snapshot(
    error = TRUE,
    with_loop(l, {
      later(function() x <<- x + 1)
      run_now()
    })
  )

  # Test GC
  # Make sure that items captured in later callbacks are GC'd after the callback
  # is executed.
  x <- 0
  with_temp_loop({
    later(
      local({
        reg.finalizer(environment(), function(e) x <<- x + 1)
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
        reg.finalizer(environment(), function(e) x <<- x + 1)
        function() 1
      })
    )
    run_now()

    later(
      local({
        e <- environment()
        reg.finalizer(environment(), function(e) x <<- x + 1)
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
  expect_true(is.integer(l$id))
  expect_true(destroy_loop(l))
  expect_false(exists_loop(l))

  # Should return false on subsequent calls to destroy_loop()
  expect_false(destroy_loop(l))
  # Destroying a second time shouldn't cause warnings.
  expect_silent(destroy_loop(l))
})

test_that("Can't destroy current loop", {
  errored <- FALSE
  with_temp_loop({
    later(function() {
      # We can't do expect_error in a later() callback, so use a tryCatch
      # instead to check that an error occurs.
      tryCatch(
        destroy_loop(current_loop()),
        error = function(e) {
          errored <<- TRUE
        }
      )
    })
    run_now()
  })

  expect_true(errored)
})

test_that("Can't GC current loop", {
  collected <- FALSE
  l <- create_loop()
  reg.finalizer(l, function(x) {
    collected <<- TRUE
  })
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
  l <- create_loop(parent = global_loop())
  x <- NULL
  later(
    function() {
      x <<- current_loop()
    },
    loop = l
  )
  run_now(loop = global_loop())
  expect_identical(x, l)
})


test_that("CallbackRegistry exists until its callbacks are run", {
  # If the R loop handle object is GC'd, it doesn't necessarily destroy the
  # underlying CallbackRegistry (in C++). The CallbackRegistry is only destroyed
  # when the R loop handle is GC'd AND the CallbackRegistry contains no more
  # callbacks.
  x <- 0
  callback <- function() {
    x <<- x + 1
  }
  local({
    l <- create_loop()
    later(callback, loop = l)
  })
  gc()
  run_now()
  expect_identical(x, 1)
})

test_that("Auto-running grandchildren loops", {
  l1_ran <- FALSE
  l11_ran <- FALSE
  l12_ran <- FALSE
  l13_ran <- FALSE
  l2_ran <- FALSE
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
  l_ran <- 0
  l1_ran <- 0
  l <- create_loop()

  with_loop(l, {
    later(function() l_ran <<- l_ran + 1)
    l1 <- create_loop()
    later(function() l1_ran <<- l1_ran + 1, loop = l1)
  })

  notify_r_ref_deleted(l)
  run_now()
  # l will run, because the underlying registry exists until empty. It also
  # causes l1 to run.
  expect_identical(l_ran, 1)
  expect_identical(l1_ran, 1)
  expect_false(exists_loop(l))
  # l1 should still exist because we still have a reference to it.
  expect_true(exists_loop(l1))

  # Schedule another function that we don't expect to actually run.
  # Use finalizer to keep
  l1_finalized <- FALSE
  later(
    local({
      reg.finalizer(environment(), function(e) l1_finalized <<- TRUE)
      function() l1_ran <<- l1_ran + 1
    }),
    loop = l1
  )
  run_now()
  # l1 won't run again
  expect_identical(l1_ran, 1)
  expect_true(exists_loop(l1))
  expect_false(l1_finalized)
  # If the reference is lost (like when the loop handle is GC'd) l1 will take
  # effect immediately.
  expect_true(notify_r_ref_deleted(l1))
  expect_false(exists_loop(l1))
  gc() # Make the finalizer run
  expect_true(l1_finalized)
})


test_that("Removing parent loop allows loop to be deleted", {
  # Create parent loop, then create a child loop, then add a finalizer to a
  # callback (actually, the env for the callback) in the child loop.
  l <- create_loop()
  l1 <- create_loop(parent = l)

  x <- 0
  with_loop(l1, {
    later(
      local({
        reg.finalizer(environment(), function(e) x <<- x + 1)
        function() NULL
      })
    )
  })

  # Removing the ref to the child should NOT cause the finalizer to run -- the
  # loop won't actually be destroyed because it (A) has a parent AND (B) has a
  # callback. notify_r_ref_deleted(l1)
  rm(l1)
  gc()
  gc()
  expect_identical(x, 0)

  # If we destroy the parent loop, then the finalizer will be called, because
  # even though the child loop has a callback, it no longer has a parent.
  # Because both the handle and its parent have been GC'd, there's no way to run
  # callbacks in the child, so the internal representation of the child loop can
  # be deleted, along with all the callbacks it contains.
  rm(l)
  # Use 2 GC's because the first causes the loops to be GC'd; the second causes
  # the function that was queued in a loop to be GC'd.
  gc()
  gc()
  expect_identical(x, 1)
})

test_that("Interrupt while running in private loop won't result in stuck loop", {
  l <- create_loop()
  later(
    function() {
      rlang::interrupt()
    },
    loop = l
  )
  tryCatch(
    {
      run_now(loop = l)
    },
    interrupt = function(e) NULL
  )
  expect_identical(current_loop(), global_loop())

  tryCatch(
    {
      with_loop(l, {
        rlang::interrupt()
      })
    },
    interrupt = function(e) NULL
  )
  expect_identical(current_loop(), global_loop())
})

test_that("list_queue", {
  l <- create_loop(parent = NULL)
  q <- NULL
  f <- function() 1 # A dummy function

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

  destroy_loop(l)
  expect_snapshot(error = TRUE, list_queue(l))
})

test_that("next_op_secs works", {
  loop <- with_temp_loop({
    expect_identical(next_op_secs(), Inf)

    later(function() {}, 0)
    expect_true(next_op_secs() <= 0)
    run_now()

    later(function() {}, 0.1)
    expect_true(next_op_secs() <= 0.1)
    run_now(0.15)

    current_loop()
  })
  expect_snapshot(error = TRUE, next_op_secs(loop))
})

test_that("parameter validation works", {
  loop <- create_loop(parent = NULL)
  expect_true(destroy_loop(loop))
  expect_false(destroy_loop(loop))
  expect_snapshot(error = TRUE, with_loop(loop, {}))
  expect_snapshot(error = TRUE, loop_empty(loop))
  expect_snapshot(error = TRUE, create_loop(parent = "invalid"))
  expect_snapshot(error = TRUE, destroy_loop(global_loop()))
})

test_that("print.event_loop works correctly", {
  loop <- create_loop(parent = NULL)

  output <- capture.output(print(loop))
  expect_match(output, "<event loop> ID: [0-9]+")

  destroy_loop(loop)
  output_destroyed <- capture.output(print(loop))
  expect_match(output_destroyed, "\\(destroyed\\)")
})

test_that("esoteric error handlers", {
  loop <- create_loop(parent = NULL)
  expect_snapshot(error = TRUE, {
    with_loop(loop, deleteCallbackRegistry(current_loop()$id))
  })
  expect_snapshot(error = TRUE, with_loop(loop, notify_r_ref_deleted(loop)))
  expect_snapshot(error = TRUE, {
    with_loop(loop, {
      .loops[[as.character(loop$id)]] <- NULL
      current_loop()
    })
  })
  expect_snapshot(error = TRUE, notify_r_ref_deleted(global_loop()))
  expect_snapshot(error = TRUE, deleteCallbackRegistry(global_loop()$id))
})
