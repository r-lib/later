context("test-later-fd.R")

test_that("later_fd", {
  skip_if_not_installed("nanonext")

  result <- NULL
  callback <- function(x) result <<- x
  s1 <- nanonext::socket(listen = "inproc://nano")
  s2 <- nanonext::socket(dial = "inproc://nano")
  fd1 <- nanonext::opt(s1, "recv-fd")
  fd2 <- nanonext::opt(s2, "recv-fd")

  # 1. timeout
  later_fd(callback, c(fd1, fd2), timeout = 0)
  Sys.sleep(0.2)
  run_now()
  expect_equal(result, c(FALSE, FALSE))

  # 2. cancellation
  result <- NULL
  cancel <- later_fd(callback, c(fd1, fd2), timeout = 0.2)
  expect_type(cancel, "closure")
  expect_true(cancel())
  Sys.sleep(0.25)
  expect_type(cancel(), "logical")
  run_now()
  expect_null(result)

  # 3. fd1 active
  later_fd(callback, c(fd1, fd2), timeout = 0.9)
  res <- nanonext::send(s2, "msg")
  Sys.sleep(0.2)
  run_now()
  expect_equal(result, c(TRUE, FALSE))

  # 4. both active
  res <- nanonext::send(s1, "msg")
  later_fd(callback, c(fd1, fd2), timeout = 1)
  Sys.sleep(0.2)
  run_now()
  expect_equal(result, c(TRUE, TRUE))

  # 5. fd2 active
  res <- nanonext::recv(s1)
  later_fd(callback, c(fd1, fd2), timeout = 1L)
  Sys.sleep(0.2)
  run_now()
  expect_equal(result, c(FALSE, TRUE))

  # 6. fd2 invalid - returns NA for fd2
  close(s2)
  later_fd(callback, c(fd1, fd2), timeout = 1)
  Sys.sleep(0.2)
  run_now()
  if (.Platform$OS.type != "windows")
    expect_equal(result, c(FALSE, NA))

  # 7. both fds invalid - returns all NA
  close(s1)
  later_fd(callback, c(fd1, fd2), timeout = 1)
  Sys.sleep(0.2)
  run_now()
  expect_equal(result, c(NA, NA))

})
