# later

[![Build Status](https://travis-ci.org/jcheng5/later.svg?branch=master)](https://travis-ci.org/jcheng5/later)

Schedule an R function or formula to run after a specified period of time. Similar to JavaScript's `setTimeout` function. Like JavaScript, R is single-threaded so there's no guarantee that the operation will run exactly at the requested time, only that at least that much time will elapse.

To avoid bugs due to reentrancy, by default, scheduled operations only run when there is no other R code present on the execution stack; i.e., when R is sitting at the top-level prompt. You can force past-due operations to run at a time of your choosing by calling `later::run_now()`.

The mechanism used by this package is inspired by Simon Urbanek's [background](https://github.com/s-u/background) package and similar code in Rhttpd.

## Installation

```r
remotes::install_github("jcheng5/later")
```

## Usage

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
