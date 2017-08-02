#ifndef _WIN32

#include <Rcpp.h>
#include <Rinternals.h>
#include <R_ext/eventloop.h>
#include <unistd.h>
#include <queue>

#include "later.h"
#include "callback_registry.h"

using namespace Rcpp;

extern void* R_GlobalContext;
extern void* R_TopLevelContext;

extern CallbackRegistry callbackRegistry;


// Whether we have initialized the input handler.
int initialized = 0;

// The handles to the read and write ends of a pipe. We use this pipe
// to signal R's input handler callback mechanism that we want to be
// called back.
int pipe_in, pipe_out;

// Whether the file descriptor is ready for reading, i.e., whether
// the input handler callback is scheduled to be called. We use this
// to avoid unnecessarily writing to the pipe.
bool hot = false;

// The buffer we're using for the pipe. This doesn't have to be large,
// in theory it only ever holds zero or one byte.
size_t BUF_SIZE = 256;
void *buf;

void set_fd(bool ready) {
  if (ready != hot) {
    if (ready) {
      ssize_t cbytes = write(pipe_in, "a", 1);
      (void)cbytes; // squelch compiler warning
      hot = true;
    } else {
      if (read(pipe_out, buf, BUF_SIZE) < 0) {
        // TODO: This sets a warning but it doesn't get displayed until
        // after the next R command is executed. Can we make it sooner?
        Rf_warning("Failed to read out of pipe for later package");
      }
      hot = false;
    }
  }
}

class SuspendFDReadiness {
public:
  SuspendFDReadiness() {
    set_fd(false);
  }
  ~SuspendFDReadiness() {
    if (!idle()) {
      set_fd(true);
    }
  }
};

static void async_input_handler(void *data) {
  if (!at_top_level()) {
    // It's not safe to run arbitrary callbacks when other R code
    // is already running. Wait until we're back at the top level.
    return;
  }

  // jcheng 2017-08-01: While callbacks are executing, make the file descriptor
  // not-ready so that our input handler is not even called back by R.
  // Previously we'd let the input handler run but return quickly, but this
  // seemed to cause R_SocketWait to hang (encountered while working with the
  // future package, trying to call value(future) with plan(multisession)).
  SuspendFDReadiness sfdr_scope;
  
  execCallbacks();
}

InputHandler* inputHandlerHandle;

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
    
    inputHandlerHandle = addInputHandler(R_InputHandlers, pipe_out, async_input_handler, 20);
    
    initialized = 1;
  }
}

void deInitialize() {
  if (initialized) {
    removeInputHandler(&R_InputHandlers, inputHandlerHandle);
    initialized = 0;
  }
}

void doExecLater(Rcpp::Function callback, double delaySecs) {
  callbackRegistry.add(callback, delaySecs);
  
  set_fd(true);
}

void doExecLater(void (*callback)(void*), void* data, double delaySecs) {
  callbackRegistry.add(callback, data, delaySecs);
  
  set_fd(true);
}

#endif // ifndef _WIN32
