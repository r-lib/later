context("test-later-fd.R")

test_that("later_fd", {
  skip_if_not_installed("nanonext")

  result <- NULL
  callback <- function(x) result <<- x
  s1 <- nanonext::socket(listen = "inproc://nanonext")
  s2 <- nanonext::socket(dial = "inproc://nanonext")
  fd1 <- nanonext::opt(s1, "recv-fd")
  fd2 <- nanonext::opt(s2, "recv-fd")

  # timeout
  later_fd(callback, c(fd1, fd2), timeout = 0)
  Sys.sleep(0.2)
  run_now()
  expect_equal(result, c(FALSE, FALSE))
  later_fd(callback, c(fd1, fd2), exceptfds = c(fd1, fd2), timeout = 0)
  Sys.sleep(0.2)
  run_now()
  expect_equal(result, c(FALSE, FALSE, FALSE, FALSE))

  # cancellation
  result <- NULL
  cancel <- later_fd(callback, c(fd1, fd2), timeout = 0.2)
  expect_type(cancel, "closure")
  expect_true(cancel())
  Sys.sleep(0.25)
  expect_false(cancel())
  expect_invisible(cancel())
  run_now()
  expect_null(result)

  # timeout (> 1 loop)
  later_fd(callback, c(fd1, fd2), timeout = 1.1)
  Sys.sleep(1.25)
  run_now()
  expect_equal(result, c(FALSE, FALSE))

  # fd1 ready
  later_fd(callback, c(fd1, fd2), timeout = 0.9)
  res <- nanonext::send(s2, "msg")
  Sys.sleep(0.2)
  run_now()
  expect_equal(result, c(TRUE, FALSE))

  # both fd1, fd2 ready
  res <- nanonext::send(s1, "msg")
  Sys.sleep(0.1)
  later_fd(callback, c(fd1, fd2), timeout = 1)
  Sys.sleep(0.1)
  run_now()
  expect_equal(result, c(TRUE, TRUE))

  # no exceptions
  later_fd(callback, c(fd1, fd2), exceptfds = c(fd1, fd2), timeout = -0.1)
  Sys.sleep(0.2)
  run_now()
  expect_equal(result, c(TRUE, TRUE, FALSE, FALSE))

  # fd2 ready
  res <- nanonext::recv(s1)
  later_fd(callback, c(fd1, fd2), timeout = 1L)
  Sys.sleep(0.2)
  run_now()
  expect_equal(result, c(FALSE, TRUE))

  # fd2 invalid
  res <- nanonext::recv(s2)
  later_fd(callback, c(fd1, fd2), exceptfds = c(fd1, fd2), timeout = 0.1)
  close(s2)
  Sys.sleep(0.2)
  run_now()
  expect_length(result, 4L)

  # both fd1, fd2 invalid
  close(s1)
  later_fd(callback, c(fd1, fd2), c(fd1, fd2), timeout = 0)
  Sys.sleep(0.2)
  run_now()
  expect_equal(result, c(NA, NA, NA, NA))

  # no fds supplied
  later_fd(callback, timeout = -1)
  Sys.sleep(0.2)
  run_now()
  expect_equal(result, logical())

})
