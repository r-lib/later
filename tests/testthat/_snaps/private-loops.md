# Private event loops

    Code
      with_loop(l, later(function() message("foo")))
    Condition
      Error in `with_loop()`:
      ! loop has been destroyed!

---

    Code
      with_loop(l, run_now())
    Condition
      Error in `with_loop()`:
      ! loop has been destroyed!

---

    Code
      destroy_loop(global_loop())
    Condition
      Error in `destroy_loop()`:
      ! Can't destroy global loop.

# Temporary event loops

    Code
      with_loop(l, {
        later(function() x <<- x + 1)
        run_now()
      })
    Condition
      Error in `with_loop()`:
      ! loop has been destroyed!

# parameter validation works

    Code
      create_loop(parent = "invalid")
    Condition
      Error in `create_loop()`:
      ! `parent` must be NULL or an event_loop object.

---

    Code
      destroy_loop(global_loop())
    Condition
      Error in `destroy_loop()`:
      ! Can't destroy global loop.

---

    Code
      loop <- create_loop(parent = NULL)
      destroy_loop(loop)
      with_loop(loop, { })
    Condition
      Error in `with_loop()`:
      ! loop has been destroyed!

