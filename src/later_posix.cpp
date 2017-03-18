#ifndef _WIN32

#include <Rcpp.h>
#include <Rinternals.h>
#include <R_ext/eventloop.h>
#include <unistd.h>
#include <queue>

#include "later.h"

using namespace Rcpp;

extern void* R_GlobalContext;
extern void* R_TopLevelContext;


// Whether we have initialized the input handler.
int initialized = 0;

// The handles to the read and write ends of a pipe. We use this pipe
// to signal R's input handler callback mechanism that we want to be
// called back.
int pipe_in, pipe_out;

// Whether the file descriptor is ready for reading, i.e., whether
// the input handler callback is scheduled to be called. We use this
// to avoid unnecessarily writing to the pipe.
int hot = 0;

// The buffer we're using for the pipe. This doesn't have to be large,
// in theory it only ever holds zero or one byte.
size_t BUF_SIZE = 256;
void *buf;

// The queue of user-provided callbacks that are scheduled to be
// executed.
std::queue<Rcpp::Function> callbacks;

static void async_input_handler(void *data) {
  if (!at_top_level()) {
    // It's not safe to run arbitrary callbacks when other R code
    // is already running. Wait until we're back at the top level.
    return;
  }

  if (read(pipe_out, buf, BUF_SIZE) < 0) {
    // TODO: This sets a warning but it doesn't get displayed until
    // after the next R command is executed. Can we make it sooner?
    Rf_warning("Failed to read out of pipe for later package");
  }
  hot = 0;
  
  // TODO: What to do about errors that occur in async handlers?
  while (!callbacks.empty()) {
    Rcpp::Function first = callbacks.front();
    callbacks.pop();
    first();
  }
}

void ensureInitialized() {
  if (!initialized) {
    buf = malloc(BUF_SIZE);
    
    int pipes[2];
    if (pipe(pipes)) {
      free(buf);
      Rf_error("Failed to create pipe");
      return;
    }
    pipe_out = pipes[0];
    pipe_in = pipes[1];
    
    addInputHandler(R_InputHandlers, pipe_out, async_input_handler, 20);
    
    initialized = 1;
  }
}

void doExecLater(Rcpp::Function callback) {
  callbacks.push(callback);
  
  if (!hot) {
    write(pipe_in, "a", 1);
    hot = 1;
  }
}

#endif // ifndef _WIN32