# Check if later loop is empty

Returns true if there are currently no callbacks that are scheduled to
execute in the present or future.

## Usage

``` r
loop_empty(loop = current_loop())
```

## Arguments

- loop:

  A handle to an event loop.
