# run_now doesn't go past a failed task

    Code
      run_now()
    Condition
      Error:
      ! boom

# When callbacks have tied timestamps, they respect order of creation

    Code
      testCallbackOrdering()

# Callbacks cannot affect the caller

    Code
      g()
    Condition
      Error:
      ! no function to return from, jumping to top level

