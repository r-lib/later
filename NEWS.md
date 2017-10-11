## later 0.6

* Fix a hang on address sanitized (ASAN) builds of R. [Issue #16](https://github.com/r-lib/later/issues/16), [PR #17](https://github.com/r-lib/later/pull/17)

* The `run_now()` function used to return only when it was unable to find any more tasks that were due. This means that if tasks were being scheduled at an interval faster than the tasks are executed, `run_now()` would never return. This release changes that behavior so that a timestamp is taken as `run_now()` begins executing, and only tasks whose timestamps are earlier or equal to it are run.

## later 0.5

* Fix a hang on Fedora 25+ which prevented the package from being installed successfully. Reported by @lepennec. [Issue #7](https://github.com/r-lib/later/issues/7), [PR #10](https://github.com/r-lib/later/pull/10)

* Fixed [issue #12](https://github.com/r-lib/later/issues/12): When an exception occurred in a callback function, it would cause future callbacks to not execute. [PR #13](https://github.com/r-lib/later/pull/13)

* Added `next_op_secs()` function to report the number of seconds before the next scheduled operation. [PR #15](https://github.com/r-lib/later/pull/15)

## later 0.4

* Add `loop_empty()` function, which returns `TRUE` if there are currently no callbacks that are scheduled to execute in the present or future.

* On POSIX platforms, fix an issue where socket connections hang when written to/read from while a later callback is scheduled. The fix required stopping the input handler from being called in several spurious situations: 1) when callbacks are already being run, 2) when R code is busy executing (we used to try as often as possible, now we space it out a bit), and 3) when all the scheduled callbacks are in the future. To accomplish this, we use a background thread that acts like a timer to poke the file descriptor whenever the input handler needs to be run--similar to what we already do for Windows. [Issue #4](https://github.com/r-lib/later/issues/4)

* On all platforms, don't invoke callbacks if callbacks are already being invoked (unless explicitly requested by a caller to `run_now()`).


## later 0.3

Initial release.
