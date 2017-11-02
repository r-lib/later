# See https://github.com/r-lib/testthat/issues/86
Sys.setenv("R_TESTS" = "")

library(testthat)
library(later)

test_check("later")
