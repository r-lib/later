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

  # test that it works
  promises::promise(function(resolve, reject) {
    env$asyncFib(resolve, reject, 3)
  }) |>
    promises::then(\(x) {
      expect_identical(x, 2)
    })
  run_now(1)
  while (!loop_empty()) {
    run_now(0.1)
  }

})
