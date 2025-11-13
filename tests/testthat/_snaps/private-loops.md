# Private event loops

    Code
      with_loop(l, later(function() message("foo")))
    Condition
      Error in `with_loop()`:
      ! loop has been destroyed!

---

    Code
      run_now(loop = l)
    Condition
      Error:
      ! CallbackRegistry does not exist.

---

    Code
      destroy_loop(global_loop())
    Condition
      Error:
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

# list_queue

    Code
      list_queue(l)
    Condition
      Error:
      ! CallbackRegistry does not exist.

# next_op_secs works

    Code
      next_op_secs(loop)
    Condition
      Error:
      ! CallbackRegistry does not exist.

# parameter validation works

    Code
      with_loop(loop, destroy_loop(loop))
    Condition
      Error:
      ! Can't destroy current loop.

---

    Code
      with_loop(loop, { })
    Condition
      Error in `with_loop()`:
      ! loop has been destroyed!

---

    Code
      loop_empty(loop)
    Condition
      Error:
      ! CallbackRegistry does not exist.

---

    Code
      create_loop(parent = "invalid")
    Condition
      Error in `create_loop()`:
      ! `parent` must be NULL or an event_loop object.

# esoteric error handlers

    Code
      notify_r_ref_deleted(global_loop())
    Condition
      Error:
      ! Can't notify that reference to global loop is deleted.

---

    Code
      with_loop(loop, notify_r_ref_deleted(loop))
    Condition
      Error:
      ! Can't notify that reference to current loop is deleted.

---

    Code
      with_loop(loop, {
        .loops[[as.character(loop$id)]] <- NULL
        current_loop()
      })
    Condition
      Error in `current_loop()`:
      ! Current loop with id 43 not found.

