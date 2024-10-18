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
  later_fd(callback, c(fd1, fd2), 1)
  Sys.sleep(1.1)
  run_now()
  expect_equal(result, c(FALSE, FALSE))

  # 2. fd1 active
  later_fd(callback, c(fd1, fd2), 1)
  res <- nanonext::send(s2, "msg")
  Sys.sleep(0.1)
  run_now()
  expect_equal(result, c(TRUE, FALSE))

  # 3. both active
  res <- nanonext::send(s1, "msg")
  later_fd(callback, c(fd1, fd2), 1)
  Sys.sleep(0.1)
  run_now()
  expect_equal(result, c(TRUE, TRUE))

  # 4. fd2 active
  res <- nanonext::recv(s1)
  later_fd(callback, c(fd1, fd2), 1)
  Sys.sleep(0.1)
  run_now()
  expect_equal(result, c(FALSE, TRUE))

  close(s2)
  close(s1)

})
