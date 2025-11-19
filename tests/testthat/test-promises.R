test_that("later C++ BackgroundTask class works with promises", {

  # This test was originally in the promises package but moved here
  # to consolidate everything that requires compilation
  skip_if_not_installed("promises")

  # compile test
  env <- new.env()
  Rcpp::sourceCpp(
    system.file("promise_task.cpp", package = "later"),
    env = env
  )

  # test that resolve works
  result <- 0
  promises::promise(function(resolve, reject) {
    env$asyncFib(resolve, reject, 3)
  }) |>
    promises::then(\(x) {
      result <<- x
    })

  expect_identical(result, 0)
  run_now(1)
  while (!loop_empty()) {
    run_now(0.1)
  }
  expect_identical(result, 2)
  # test that reject works (swap resolve/reject)
  err_result <- 0
  promises::promise(function(resolve, reject) {
    env$asyncFib(reject, resolve, 6)
  }) |>
    promises::catch(\(x) {
      err_result <<- x
    })

  expect_identical(err_result, 0)
  run_now(1)
  while (!loop_empty()) {
    run_now(0.1)
  }
  expect_identical(err_result, 8)
})
