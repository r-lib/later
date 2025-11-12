# Relative time to next scheduled operation

Returns the duration between now and the earliest operation that is
currently scheduled, in seconds. If the operation is in the past, the
value will be negative. If no operation is currently scheduled, the
value will be `Inf`.

## Usage

``` r
next_op_secs(loop = current_loop())
```

## Arguments

- loop:

  A handle to an event loop.
