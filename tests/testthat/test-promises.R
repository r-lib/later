test_that("later C++ interface works with promises", {

  # This test was originally in the promises package but moved here
  # to consolidate everything that requires compilation
  skip_if_not_installed("promises")

  # test helper
  create_counting_domain <- function() {
    counts <- list2env(
      parent = emptyenv(),
      list(
        onFulfilledBound = 0L,
        onFulfilledCalled = 0L,
        onFulfilledActive = 0L,
        onRejectedBound = 0L,
        onRejectedCalled = 0L,
        onRejectedActive = 0L
      )
    )

    incr <- function(field) {
      field <- as.character(substitute(field))
      counts[[field]] <- counts[[field]] + 1L
    }

    decr <- function(field) {
      field <- as.character(substitute(field))
      counts[[field]] <- counts[[field]] - 1L
    }

    promises::new_promise_domain(
      wrapOnFulfilled = function(onFulfilled) {
        incr(onFulfilledBound)
        function(...) {
          incr(onFulfilledCalled)
          incr(onFulfilledActive)
          on.exit(decr(onFulfilledActive))

          onFulfilled(...)
        }
      },
      wrapOnRejected = function(onRejected) {
        incr(onRejectedBound)
        function(...) {
          incr(onRejectedCalled)
          incr(onRejectedActive)
          on.exit(decr(onRejectedActive))

          onRejected(...)
        }
      },
      counts = counts
    )
  }

  # grab promises function
  ns <- getNamespace("promises")
  current_promise_domain <- ns$current_promise_domain

  # compile test
  env <- new.env()
  Rcpp::sourceCpp(
    system.file("promise_task.cpp", package = "later"),
    env = env
  )

  # test that it works
  promises::promise(function(resolve, reject) {
    env$asyncFib(resolve, reject, 3)
  }) |>
    promises::then(\(x) {
      expect_identical(x, 2)
    })
  while (!loop_empty()) {
    run_now(1)
  }

  # test works with promise domains
  cd <- create_counting_domain()

  expect_true(is.null(current_promise_domain()))
  promises::with_promise_domain(cd, {
    promises::promise(function(resolve, reject) {
      env$asyncFib(resolve, reject, 3)
    }) |>
      promises::then(\(x) {
        expect_identical(x, 2)
        expect_identical(cd$counts$onFulfilledCalled, 1L)
        promises::promise_resolve(TRUE) |>
          promises::then(\(y) {
            expect_true(!is.null(current_promise_domain()))
            expect_identical(cd$counts$onFulfilledCalled, 2L)
          })
      })
    while (!loop_empty()) {
      run_now(1)
    }
  })
})
