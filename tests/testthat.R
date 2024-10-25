# See https://github.com/r-lib/testthat/issues/86
Sys.setenv("R_TESTS" = "")

library(testthat)
library(later)

DetailedSummaryReporter <- R6::R6Class("DetailedSummaryReporter", inherit = testthat::SummaryReporter,
  public = list(
    start_test = function(context, test) {
      self$cat_tight("    ", test, ": ")
    },
    end_test = function(context, test) {
      self$cat_line()
    },
    start_context = function(context) {
      self$cat_tight(context, ":\n")
    },
    end_context = function(context) { }
  )
)

test_check("later", reporter = DetailedSummaryReporter)
