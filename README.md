# later

<!-- badges: start -->
[![R build status](https://github.com/r-lib/later/workflows/R-CMD-check/badge.svg)](https://github.com/r-lib/later/actions)
<!-- badges: end -->


Schedule an R function or formula to run after a specified period of time. Similar to JavaScript's `setTimeout` function. Like JavaScript, R is single-threaded so there's no guarantee that the operation will run exactly at the requested time, only that at least that much time will elapse.

To avoid bugs due to reentrancy, by default, scheduled operations only run when there is no other R code present on the execution stack; i.e., when R is sitting at the top-level prompt. You can force past-due operations to run at a time of your choosing by calling `later::run_now()`.

The mechanism used by this package is inspired by Simon Urbanek's [background](https://github.com/s-u/background) package and similar code in Rhttpd.

## Installation

```r
remotes::install_github("r-lib/later")
```

## Usage from R

Pass a function (in this case, delayed by 5 seconds):

```r
later::later(function() {
  print("Got here!")
}, 5)
```

Or a formula (in this case, run as soon as control returns to the top-level):

```r
later::later(~print("Got here!"))
```

## Usage from C++

You can also call `later::later` from C++ code in your own packages, to cause your own C-style functions to be called back. This is safe to call from either the main R thread or a different thread; in both cases, your callback will be invoked from the main R thread.

`later::later` is accessible from `later_api.h` and its prototype looks like this:

```cpp
void later(void (*func)(void*), void* data, double secs)
```

The first argument is a pointer to a function that takes one `void*` argument and returns void. The second argument is a `void*` that will be passed to the function when it's called back. And the third argument is the number of seconds to wait (at a minimum) before invoking.

To use the C++ interface, you'll need to add `later` to your `DESCRIPTION` file under both `LinkingTo` and `Imports`, and also make sure that your `NAMESPACE` file has an `import(later)` entry.

### Background tasks

Finally, this package also offers a higher-level C++ helper class to make it easier to execute tasks on a background thread. It is also available from `later_api.h` and its public/protected interface looks like this:

```cpp
class BackgroundTask {

public:
  BackgroundTask();
  virtual ~BackgroundTask();

  // Start executing the task
  void begin();

protected:
  // The task to be executed on the background thread.
  // Neither the R runtime nor any R data structures may be
  // touched from the background thread; any values that need
  // to be passed into or out of the Execute method must be
  // included as fields on the Task subclass object.
  virtual void execute() = 0;

  // A short task that runs on the main R thread after the
  // background task has completed. It's safe to access the
  // R runtime and R data structures from here.
  virtual void complete() = 0;
}
```

Create your own subclass, implementing a custom constructor plus the `execute` and `complete` methods.

It's critical that the code in your `execute` method not mutate any R data structures, call any R code, or cause any R allocations, as it will execute in a background thread where such operations are unsafe. You can, however, perform such operations in the constructor (assuming you perform construction only from the main R thread) and `complete` method. Pass values between the constructor and methods using fields.

```rcpp
#include <Rcpp.h>
#include <later_api.h>

class MyTask : public later::BackgroundTask {
public:
  MyTask(Rcpp::NumericVector vec) :
    inputVals(Rcpp::as<std::vector<double> >(vec)) {
  }

protected:
  void execute() {
    double sum = 0;
    for (std::vector<double>::const_iterator it = inputVals.begin();
      it != inputVals.end();
      it++) {

      sum += *it;
    }
    result = sum / inputVals.size();
  }

  void complete() {
    Rprintf("Result is %f\n", result);
  }

private:
  std::vector<double> inputVals;
  double result;
};
```

To run the task, `new` up your subclass and call `begin()`, e.g. `(new MyTask(vec))->begin()`. There's no need to keep track of the pointer; the task object will delete itself when the task is complete.

```r
// [[Rcpp::export]]
void asyncMean(Rcpp::NumericVector data) {
  (new MyTask(data))->begin();
}
```

It's not very useful to execute tasks on background threads if you can't get access to the results back in R. We'll soon be introducing a complementary R package that provides a suitable "promise" or "future" abstraction.
