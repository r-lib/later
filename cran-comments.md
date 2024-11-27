## R CMD check results

0 errors | 0 warnings | 1 note

I'm so sorry to be re-submitting this so soon after my previous submission.
That version (1.4.0) has started causing problems for deployed projects
that use packages that link to {later}, like {httpuv} and {promises},
because their CRAN binaries are built against the latest CRAN {later} but
they may have pinned their locally installed {later} to an older version.
This causes R_GetCCallable calls in those dependent packages to fail on
package startup.

This submission is intended to fix this problem by gracefully detecting the
version mismatch, and simply not performing R_GetCCallable calls that will
not succeed.

More details here:
https://github.com/r-lib/later/issues/203
