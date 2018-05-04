context("test-private-loops.R")

describe("Private event loop", {
  it("changes current_loop()", {
    expect_identical(current_loop(), global_loop())
    
    with_private_loop({
      expect_false(identical(current_loop(), global_loop()))
    })
  })
  
  it("runs only its own tasks", {
    x <- 0
    later(~{x <<- 1}, 0)
    with_private_loop({
      expect_true(loop_empty())
      
      later(~{x <<- 2})
      run_now()
      
      expect_identical(x, 2)
      
      run_now(loop = global_loop())
      expect_identical(x, 1)
    })
  })
})
