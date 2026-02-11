# Changelog

## later (development version)

- Improved responsiveness when idle at the R console on POSIX systems
  ([\#251](https://github.com/r-lib/later/issues/251)).

- Fixes [\#249](https://github.com/r-lib/later/issues/249): Moved the
  contents of `inst/include/later.h` into `later_api.h` to ensure R
  headers are not included before Rcpp headers when Rcpp auto-includes
  `$PACKAGE.h` in RcppExports.cpp. The public API header remains
  `later_api.h` ([\#250](https://github.com/r-lib/later/issues/250)).

## later 1.4.5

CRAN release: 2026-01-08

- Now requires R \>= 3.5.0 (for `R_UnwindProtect()`) and Rcpp \>=
  1.0.10. Removed legacy non-unwind-protect code paths that were
  previously used as a fallback on older R versions
  ([\#241](https://github.com/r-lib/later/issues/241)).

## later 1.4.4

CRAN release: 2025-08-27

- Fixed timings in a test
  ([\#237](https://github.com/r-lib/later/issues/237)). No user-facing
  changes.

## later 1.4.3

CRAN release: 2025-08-20

- Fixed [\#215](https://github.com/r-lib/later/issues/215): The
  `autorun` argument of
  [`create_loop()`](https://later.r-lib.org/dev/reference/create_loop.md),
  long deprecated, is removed
  ([\#222](https://github.com/r-lib/later/issues/222)).

- Fixed [\#167](https://github.com/r-lib/later/issues/167):
  `.Random.seed` is no longer affected when the package is loaded
  ([\#220](https://github.com/r-lib/later/issues/220)).

- Set file-level variables as `static` to avoid triggering
  `-Wmissing-variable-declarations`
  ([@michaelchirico](https://github.com/michaelchirico),
  [\#163](https://github.com/r-lib/later/issues/163)).

## later 1.4.2

CRAN release: 2025-04-08

- Fixed [\#208](https://github.com/r-lib/later/issues/208): Fixed
  `keyword is hidden by macro definition` compiler warning when using a
  C23 compiler. ([@shikokuchuo](https://github.com/shikokuchuo),
  [\#209](https://github.com/r-lib/later/issues/209))

## later 1.4.1

CRAN release: 2024-11-27

- Fixed [\#203](https://github.com/r-lib/later/issues/203): Resolves an
  issue where packages that have `LinkingTo: later` (including
  `promises` and `httpuv`) and were built against `later` 1.4.0, would
  fail to load on systems that actually had older versions of `later`
  installed, erroring out with the message “function ‘execLaterFdNative’
  not provided by package ‘later’”. With this fix, such dependent
  packages should gracefully deal with older versions at load time, and
  complain with helpful error messages if newer C interfaces (than are
  available on the installed `later`) are accessed.
  ([\#204](https://github.com/r-lib/later/issues/204))

## later 1.4.0

CRAN release: 2024-11-26

- Adds [`later_fd()`](https://later.r-lib.org/dev/reference/later_fd.md)
  which executes a function when a file descriptor is ready for reading
  or writing, at some indeterminate time in the future (subject to an
  optional timeout). This facilitates an event-driven approach to
  asynchronous or streaming downloads.
  ([@shikokuchuo](https://github.com/shikokuchuo) and
  [@jcheng5](https://github.com/jcheng5),
  [\#190](https://github.com/r-lib/later/issues/190))

- Fixed [\#186](https://github.com/r-lib/later/issues/186): Improvements
  to package load time as `rlang` is now only loaded when used. This is
  a notable efficiency for packages with only a ‘linking to’ dependency
  on `later`. Also updates to native symbol registration from dynamic
  lookup. ([@shikokuchuo](https://github.com/shikokuchuo) and
  [@wch](https://github.com/wch),
  [\#187](https://github.com/r-lib/later/issues/187))

- Fixed [\#191](https://github.com/r-lib/later/issues/191): Errors
  raised in later callbacks were being re-thrown as generic C++
  std::runtime_error with Rcpp \>= 1.0.10 (since 2022!).
  ([@shikokuchuo](https://github.com/shikokuchuo) and
  [@lionel-](https://github.com/lionel-),
  [\#192](https://github.com/r-lib/later/issues/192))

## later 1.3.2

CRAN release: 2023-12-06

- Fixed `unused variable` compiler warning.
  ([@MichaelChirico](https://github.com/MichaelChirico),
  [\#176](https://github.com/r-lib/later/issues/176))

- Fixed [\#177](https://github.com/r-lib/later/issues/177): The order of
  includes in `later.h` could cause compilation errors on some
  platforms. ([@jeroen](https://github.com/jeroen),
  [\#178](https://github.com/r-lib/later/issues/178))

- Closed [\#181](https://github.com/r-lib/later/issues/181): Fix R CMD
  check warning re error() format strings (for r-devel).
  ([\#133](https://github.com/r-lib/later/issues/133))

## later 1.3.1

CRAN release: 2023-05-02

- For C function declarations that take no parameters, added `void`
  parameter. ([\#172](https://github.com/r-lib/later/issues/172))

## later 1.3.0

CRAN release: 2021-08-18

- Closed [\#148](https://github.com/r-lib/later/issues/148): When later
  was attached,
  [`parallel::makeForkCluster()`](https://rdrr.io/r/parallel/makeCluster.html)
  would fail. ([\#149](https://github.com/r-lib/later/issues/149))

- Fixed [\#150](https://github.com/r-lib/later/issues/150): It was
  possible for callbacks to execute in the wrong order if the clock time
  was changed in between the scheduling of two callbacks.
  ([\#151](https://github.com/r-lib/later/issues/151))

## later 1.2.0

CRAN release: 2021-04-23

- Closed [\#138](https://github.com/r-lib/later/issues/138): later is
  now licensed as MIT.
  ([\#139](https://github.com/r-lib/later/issues/139))

- Closed [\#140](https://github.com/r-lib/later/issues/140): Previously,
  the event loop stopped running if the R process was forked.
  ([\#141](https://github.com/r-lib/later/issues/141))

- Closed [\#143](https://github.com/r-lib/later/issues/143): Packages
  which link to later no longer need to take a direct dependency on
  Rcpp, because `later.h` no longer includes `Rcpp.h`.
  ([\#144](https://github.com/r-lib/later/issues/144))

- Removed dependency on the BH package. C++11 is now required.
  ([\#147](https://github.com/r-lib/later/issues/147))

## later 1.1.0.1

CRAN release: 2020-06-05

- Private event loops are now automatically run by their parent. That
  is, whenever an event loop is run, its children event loops are
  automatically run. The
  [`create_loop()`](https://later.r-lib.org/dev/reference/create_loop.md)
  function has a new parameter `parent`, which defaults to the current
  loop. The auto-running behavior can be disabled by using
  `create_loop(parent=NULL)`.
  ([\#119](https://github.com/r-lib/later/issues/119))

- Fixed [\#73](https://github.com/r-lib/later/issues/73),
  [\#109](https://github.com/r-lib/later/issues/109): Previously, later
  did not build on some platforms, notably ARM, because the `-latomic`
  linker was needed on those platforms. A configure script now detects
  when `-latomic` is needed.
  ([\#114](https://github.com/r-lib/later/issues/114))

- Previously, `execLaterNative` was initialized when the package was
  loaded, but not `execLaterNative2`, resulting in a warning message in
  some cases. ([\#116](https://github.com/r-lib/later/issues/116))

## later 1.0.0

CRAN release: 2019-10-04

- Added private event loops: these are event loops that can be run
  independently from the global event loop. These are useful when you
  have code that schedules callbacks with
  [`later()`](https://later.r-lib.org/dev/reference/later.md), and you
  want to call
  [`run_now()`](https://later.r-lib.org/dev/reference/run_now.md) block
  and wait for those callbacks to execute before continuing. Without
  private event loops, if you call
  [`run_now()`](https://later.r-lib.org/dev/reference/run_now.md) to
  wait until a particular callback has finished, you might inadvertantly
  run other callbacks that were scheduled by other code. With private
  event loops, you can create a private loop, schedule a callback on it,
  then call
  [`run_now()`](https://later.r-lib.org/dev/reference/run_now.md) on
  that loop until it executes, all without interfering with the global
  loop. ([\#84](https://github.com/r-lib/later/issues/84))

## later 0.8.0

CRAN release: 2019-02-11

- Fixed issue [\#77](https://github.com/r-lib/later/issues/77): On some
  platforms, the system’s C library has support for C11-style threads,
  but there is no `threads.h` header file. In this case, later’s
  configure script tried to use the tinycthread, but upon linking, there
  were function name conflicts between tinycthread and the system’s C
  library. Later no longer tries to use the system’s `threads.h`, and
  the functions in tinycthread were renamed so that they do not
  accidentally link to the system C library’s C11-style thread
  functions. PR [\#79](https://github.com/r-lib/later/issues/79)

- Added `all` argument to
  [`run_now()`](https://later.r-lib.org/dev/reference/run_now.md);
  defaults to `TRUE`, but if set to `FALSE`, then `run_now` will run at
  most one later operation before returning. PR
  [\#75](https://github.com/r-lib/later/issues/75)

- Fixed issue [\#74](https://github.com/r-lib/later/issues/74): Using
  later with R at the terminal on POSIX could cause 100% CPU. This was
  caused by later accidentally provoking R to call its input handler
  continuously. PR [\#76](https://github.com/r-lib/later/issues/76)

- Fixed issue [\#73](https://github.com/r-lib/later/issues/73): Linking
  later on ARM failed because `boost::atomic` requires the
  `-lboost_atomic` flag. Now later tries to use `std::atomic` when
  available (when the compiler supports C++11), and falls back to
  `boost::atomic` if not. PR
  [\#80](https://github.com/r-lib/later/issues/80)

## later 0.7.5

CRAN release: 2018-09-18

- Fixed issue where the order of callbacks scheduled by native
  later::later could be nondeterministic if they are scheduled too
  quickly. This was because callbacks were sorted by the time at which
  they come due, which could be identical. Later now uses the order of
  insertion as a tiebreaker. PR
  [\#69](https://github.com/r-lib/later/issues/69)

## later 0.7.4

CRAN release: 2018-08-31

- Fixed issue [\#45](https://github.com/r-lib/later/issues/45) and
  [\#63](https://github.com/r-lib/later/issues/63): glibc 2.28 and musl
  (used on Arch and Alpine Linux) added support for C11-style threads.h,
  which masked functions from the tinycthread library used by later.
  Later now detects support for threads.h and uses it if available;
  otherwise it uses tinycthread. PR
  [\#64](https://github.com/r-lib/later/issues/64)

## later 0.7.3

CRAN release: 2018-06-08

- Fixed issue [\#57](https://github.com/r-lib/later/issues/57): If a
  user interrupt occurred when later (internally) called
  [`sys.nframe()`](https://rdrr.io/r/base/sys.parent.html), the R
  process would crash. PR
  [\#58](https://github.com/r-lib/later/issues/58)

## later 0.7.2

CRAN release: 2018-05-01

- Fixed issue [\#48](https://github.com/r-lib/later/issues/48):
  Occasional timedwait errors from later::run_now. Thanks,
  [@vnijs](https://github.com/vnijs)! PR
  [\#49](https://github.com/r-lib/later/issues/49)

- Fixed a build warning on OS X 10.11 and earlier. PR
  [\#54](https://github.com/r-lib/later/issues/54)

## later 0.7.1

CRAN release: 2018-03-07

- Fixed issue [\#39](https://github.com/r-lib/later/issues/39): Calling
  the C++ function
  [`later::later()`](https://later.r-lib.org/dev/reference/later.md)
  from a different thread could cause an R GC event to occur on that
  thread, leading to memory corruption. PR
  [\#40](https://github.com/r-lib/later/issues/40)

- Decrease latency of repeated top-level execution.

## later 0.7 (unreleased)

- Fixed issue [\#22](https://github.com/r-lib/later/issues/22): GC
  events could cause an error message:
  `Error: unimplemented type 'integer' in 'coerceToInteger'`. PR
  [\#23](https://github.com/r-lib/later/issues/23)

- Fixed issues [\#25](https://github.com/r-lib/later/issues/25),
  [\#29](https://github.com/r-lib/later/issues/29), and
  [\#31](https://github.com/r-lib/later/issues/31): If errors occurred
  when callbacks were executed by R’s input handler (as opposed to by
  [`run_now()`](https://later.r-lib.org/dev/reference/run_now.md)), then
  they would not be properly handled by R and put the terminal in a
  problematic state. PR [\#33](https://github.com/r-lib/later/issues/33)

- Fixed issue [\#37](https://github.com/r-lib/later/issues/37): High CPU
  usage on Linux. PR [\#38](https://github.com/r-lib/later/issues/38)

- Fixed issue [\#36](https://github.com/r-lib/later/issues/36): Failure
  to build on OS X \<=10.12 (thanks
  [@mingwandroid](https://github.com/mingwandroid)). PR
  [\#21](https://github.com/r-lib/later/issues/21)

## later 0.6

CRAN release: 2017-11-04

- Fix a hang on address sanitized (ASAN) builds of R. Issue
  [\#16](https://github.com/r-lib/later/issues/16), PR
  [\#17](https://github.com/r-lib/later/issues/17)

- The [`run_now()`](https://later.r-lib.org/dev/reference/run_now.md)
  function now takes a `timeoutSecs` argument. If no tasks are ready to
  run at the time `run_now(timeoutSecs)` is invoked, we will wait up to
  `timeoutSecs` for one to become ready. The default value of `0` means
  [`run_now()`](https://later.r-lib.org/dev/reference/run_now.md) will
  return immediately if no tasks are ready, which is the same behavior
  as in previous releases. PR
  [\#19](https://github.com/r-lib/later/issues/19)

- The [`run_now()`](https://later.r-lib.org/dev/reference/run_now.md)
  function used to return only when it was unable to find any more tasks
  that were due. This means that if tasks were being scheduled at an
  interval faster than the tasks are executed,
  [`run_now()`](https://later.r-lib.org/dev/reference/run_now.md) would
  never return. This release changes that behavior so that a timestamp
  is taken as
  [`run_now()`](https://later.r-lib.org/dev/reference/run_now.md) begins
  executing, and only tasks whose timestamps are earlier or equal to it
  are run. PR [\#18](https://github.com/r-lib/later/issues/18)

- Fix compilation errors on Solaris. Reported by Brian Ripley. PR
  [\#20](https://github.com/r-lib/later/issues/20)

## later 0.5

CRAN release: 2017-10-05

- Fix a hang on Fedora 25+ which prevented the package from being
  installed successfully. Reported by
  [@lepennec](https://github.com/lepennec). Issue
  [\#7](https://github.com/r-lib/later/issues/7), PR
  [\#10](https://github.com/r-lib/later/issues/10)

- Fixed issue [\#12](https://github.com/r-lib/later/issues/12): When an
  exception occurred in a callback function, it would cause future
  callbacks to not execute. PR
  [\#13](https://github.com/r-lib/later/issues/13)

- Added
  [`next_op_secs()`](https://later.r-lib.org/dev/reference/next_op_secs.md)
  function to report the number of seconds before the next scheduled
  operation. PR [\#15](https://github.com/r-lib/later/issues/15)

## later 0.4

CRAN release: 2017-08-23

- Add
  [`loop_empty()`](https://later.r-lib.org/dev/reference/loop_empty.md)
  function, which returns `TRUE` if there are currently no callbacks
  that are scheduled to execute in the present or future.

- On POSIX platforms, fix an issue where socket connections hang when
  written to/read from while a later callback is scheduled. The fix
  required stopping the input handler from being called in several
  spurious situations: 1) when callbacks are already being run, 2) when
  R code is busy executing (we used to try as often as possible, now we
  space it out a bit), and 3) when all the scheduled callbacks are in
  the future. To accomplish this, we use a background thread that acts
  like a timer to poke the file descriptor whenever the input handler
  needs to be run–similar to what we already do for Windows. Issue
  [\#4](https://github.com/r-lib/later/issues/4)

- On all platforms, don’t invoke callbacks if callbacks are already
  being invoked (unless explicitly requested by a caller to
  [`run_now()`](https://later.r-lib.org/dev/reference/run_now.md)).

## later 0.3

CRAN release: 2017-06-25

Initial release.
