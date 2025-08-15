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

# next_op_secs works

    Code
      next_op_secs(loop)
    Condition
      Error in `nextOpSecs()`:
      ! CallbackRegistry does not exist.

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

# esoteric error handlers

    Code
      with_loop(loop, deleteCallbackRegistry(current_loop()$id))
    Condition
      Error in `deleteCallbackRegistry()`:
      ! Can't delete current loop.

---

    Code
      with_loop(loop, {
        .loops[[as.character(loop$id)]] <- NULL
        current_loop()
      })
    Condition
      Error in `current_loop()`:
      ! Current loop with id 43 not found.

---

    Code
      notify_r_ref_deleted(global_loop())
    Condition
      Error in `notify_r_ref_deleted()`:
      ! Can't notify that reference to global loop is deleted.

---

    Code
      deleteCallbackRegistry(global_loop()$id)
    Condition
      Error in `deleteCallbackRegistry()`:
      ! Can't delete global loop.

