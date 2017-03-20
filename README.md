# later

Run an R function or formula sometime in the future, when no other R code is on the execution stack. Similar to JavaScript's `setTimeout(func, 0);` or `process.nextTick(func)`.

The mechanism used by this package is inspired by Simon Urbanek's [background](https://github.com/s-u/background) package and similar code in Rhttpd.

## Installation

```r
remotes::install_github("jcheng5/later")
```

## Usage

Pass a function:

```r
later::later(function() {
  print("Got here!")
})
```

Or a formula:

```r
later::later(~print("Got here!"))
```

These may appear to execute immediately, but if you force R to execute a longer-running expression you'll see that execution comes later.

```r
a <- function() {
  later::later(~print("This executes later"))
  Sys.sleep(3)
  print("Done with `a`")
}
a()
```