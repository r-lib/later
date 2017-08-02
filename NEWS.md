## later 0.4

* Add `loop_empty()` function, which returns `TRUE` if there are currently no callbacks that are scheduled to execute in the present or future.

* On POSIX platforms, prevent the input handler from being called spuriously when callbacks are already being run. (Fixes an issue where socket connections seem to starve when read from within a later callback.)

* On all platforms, don't invoke callbacks if callbacks are already being invoked (unless explicitly requested by a caller to `run_now()`).


## later 0.3

Initial release.
