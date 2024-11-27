context("test-later-fd.R")

test_that("later_fd", {
  skip_if_not_installed("nanonext")

  result <- NULL
  callback <- function(x) result <<- x
  s1 <- nanonext::socket(listen = "inproc://nanonext")
  on.exit(close(s1))
  s2 <- nanonext::socket(dial = "inproc://nanonext")
  on.exit(close(s2), add = TRUE)
  fd1 <- nanonext::opt(s1, "recv-fd")
  fd2 <- nanonext::opt(s2, "recv-fd")

  # timeout
  later_fd(callback, c(fd1, fd2), timeout = 0)
  run_now(1)
  expect_equal(result, c(FALSE, FALSE))
  later_fd(callback, c(fd1, fd2), exceptfds = c(fd1, fd2), timeout = 0)
  run_now(1)
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
  run_now(1.3)
  expect_equal(result, c(FALSE, FALSE))

  # fd1 ready
  later_fd(callback, c(fd1, fd2), timeout = 0.9)
  res <- nanonext::send(s2, "msg")
  run_now(1)
  expect_equal(result, c(TRUE, FALSE))

  # both fd1, fd2 ready
  res <- nanonext::send(s1, "msg")
  Sys.sleep(0.1)
  later_fd(callback, c(fd1, fd2), timeout = 1)
  run_now(1)
  expect_equal(result, c(TRUE, TRUE))

  # no exceptions
  later_fd(callback, c(fd1, fd2), exceptfds = c(fd1, fd2), timeout = -0.1)
  run_now(1)
  expect_equal(result, c(TRUE, TRUE, FALSE, FALSE))

  # fd2 ready
  res <- nanonext::recv(s1)
  later_fd(callback, c(fd1, fd2), timeout = 1L)
  run_now(1)
  expect_equal(result, c(FALSE, TRUE))

  # fd2 invalid
  res <- nanonext::recv(s2)
  later_fd(callback, c(fd1, fd2), exceptfds = c(fd1, fd2), timeout = 0.1)
  close(s2)
  run_now(1)
  expect_length(result, 4L)

  # both fd1, fd2 invalid
  close(s1)
  later_fd(callback, c(fd1, fd2), c(fd1, fd2), timeout = 0)
  run_now(1)
  expect_equal(result, c(NA, NA, NA, NA))

  # no fds supplied
  later_fd(callback, timeout = -1)
  run_now(1)
  expect_equal(result, logical())

  on.exit()

})

test_that("loop_empty() reflects later_fd callbacks", {
  skip_if_not_installed("nanonext")

  s1 <- nanonext::socket(listen = "inproc://nanotest2")
  on.exit(close(s1))
  s2 <- nanonext::socket(dial = "inproc://nanotest2")
  on.exit(close(s2), add = TRUE)

  fd1 <- nanonext::opt(s1, "recv-fd")

  expect_true(loop_empty())

  cancel <- later_fd(~{}, fd1)
  expect_false(loop_empty())
  cancel()
  Sys.sleep(1.25) # check for cancellation happens every ~1 sec
  expect_true(loop_empty())

  later_fd(~{}, fd1, timeout = 0)
  expect_false(loop_empty())
  run_now(1)
  expect_true(loop_empty())

})

test_that("later_fd() errors when passed destroyed loops", {

  loop <- create_loop()
  destroy_loop(loop)
  expect_error(later_fd(identity, loop = loop), "CallbackRegistry does not exist")

})
