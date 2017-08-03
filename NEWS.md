## later 0.4

* Add `loop_empty()` function, which returns `TRUE` if there are currently no callbacks that are scheduled to execute in the present or future.

* On POSIX platforms, fix an issue where socket connections hang when written to/read from while a later callback is scheduled. The fix required stopping the input handler from being called in several spurious situations: 1) when callbacks are already being run, 2) when R code is busy executing (we used to try as often as possible, now we space it out a bit), and 3) when all the scheduled callbacks are in the future. To accomplish this, we use a background thread that acts like a timer to poke the file descriptor whenever the input handler needs to be run--similar to what we already do for Windows. [Issue #4](https://github.com/r-lib/later/issues/4)

* On all platforms, don't invoke callbacks if callbacks are already being invoked (unless explicitly requested by a caller to `run_now()`).


## later 0.3

Initial release.
