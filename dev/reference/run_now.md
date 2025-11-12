# Execute scheduled operations

Normally, operations scheduled with
[`later()`](https://later.r-lib.org/dev/reference/later.md) will not
execute unless/until no other R code is on the stack (i.e. at the
top-level). If you need to run blocking R code for a long time and want
to allow scheduled operations to run at well-defined points of your own
operation, you can call `run_now()` at those points and any operations
that are due to run will do so.

## Usage

``` r
run_now(timeoutSecs = 0L, all = TRUE, loop = current_loop())
```

## Arguments

- timeoutSecs:

  Wait (block) for up to this number of seconds waiting for an operation
  to be ready to run. If `0`, then return immediately if there are no
  operations that are ready to run. If `Inf` or negative, then wait as
  long as it takes (if none are scheduled, then this will block
  forever).

- all:

  If `FALSE`, `run_now()` will execute at most one scheduled operation
  (instead of all eligible operations). This can be useful in cases
  where you want to interleave scheduled operations with your own logic.

- loop:

  A handle to an event loop. Defaults to the currently-active loop.

## Value

A logical indicating whether any callbacks were actually run.

## Details

If one of the callbacks throws an error, the error will *not* be caught,
and subsequent callbacks will not be executed (until `run_now()` is
called again, or control returns to the R prompt). You must use your own
[tryCatch](https://rdrr.io/r/base/conditions.html) if you want to handle
errors.
