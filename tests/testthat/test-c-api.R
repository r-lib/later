context("C++ API")

test_that("header and DLL API versions match", {
  Rcpp::cppFunction(
    code = '
      int later_dll_api_version() {
        int (*dll_api_version)() = (int (*)()) R_GetCCallable("later", "apiVersion");
        return (*dll_api_version)();
      }
    '
  )

  Rcpp::cppFunction(
    depends = 'later',
    includes = '
      #include <later_api.h>
    ',
    code = '
      int later_h_api_version() {
        return LATER_H_API_VERSION;
      }
    '
  )

  expect_identical(later_dll_api_version(), later_h_api_version())
})
