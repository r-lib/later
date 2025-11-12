# Private event loops

Normally, later uses a global event loop for scheduling and running
functions. However, in some cases, it is useful to create a *private*
event loop to schedule and execute tasks without disturbing the global
event loop. For example, you might have asynchronous code that queries a
remote data source, but want to wait for a full back-and-forth
communication to complete before continuing in your code – from the
caller's perspective, it should behave like synchronous code, and not do
anything with the global event loop (which could run code unrelated to
your operation). To do this, you would run your asynchronous code using
a private event loop.

## Usage

``` r
create_loop(parent = current_loop())

destroy_loop(loop)

exists_loop(loop)

current_loop()

with_temp_loop(expr)

with_loop(loop, expr)

global_loop()
```

## Arguments

- parent:

  The parent event loop for the one being created. Whenever the parent
  loop runs, this loop will also automatically run, without having to
  manually call
  [`run_now()`](https://later.r-lib.org/dev/reference/run_now.md) on
  this loop. If `NULL`, then this loop will not have a parent event loop
  that automatically runs it; the only way to run this loop will be by
  calling
  [`run_now()`](https://later.r-lib.org/dev/reference/run_now.md) on
  this loop.

- loop:

  A handle to an event loop.

- expr:

  An expression to evaluate.

## Details

`create_loop` creates and returns a handle to a private event loop,
which is useful when for scheduling tasks when you do not want to
interfere with the global event loop.

`destroy_loop` destroys a private event loop.

`exists_loop` reports whether an event loop exists – that is, that it
has not been destroyed.

`current_loop` returns the currently-active event loop. Any calls to
[`later()`](https://later.r-lib.org/dev/reference/later.md) or
[`run_now()`](https://later.r-lib.org/dev/reference/run_now.md) will use
the current loop by default.

`with_loop` evaluates an expression with a given event loop as the
currently-active loop.

`with_temp_loop` creates an event loop, makes it the current loop, then
evaluates the given expression. Afterwards, the new event loop is
destroyed.

`global_loop` returns a handle to the global event loop.
