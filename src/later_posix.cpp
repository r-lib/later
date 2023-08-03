#ifndef _WIN32

#include <Rcpp.h>
#include <Rinternals.h>
#include <R_ext/eventloop.h>
#include <unistd.h>
#include <queue>

#include "later.h"
#include "callback_registry.h"
#include "timer_posix.h"
#include "threadutils.h"
#include "debug.h"

using namespace Rcpp;

#define LATER_ACTIVITY 20
#define LATER_DUMMY_ACTIVITY 21

extern void* R_GlobalContext;
extern void* R_TopLevelContext;

// Whether we have initialized the input handler.
static int initialized = 0;

// The handles to the read and write ends of a pipe. We use this pipe
// to signal R's input handler callback mechanism that we want to be
// called back.
static int pipe_in  = -1;
static int pipe_out = -1;

static int dummy_pipe_in  = -1;
static int dummy_pipe_out = -1;

// Whether the file descriptor is ready for reading, i.e., whether
// the input handler callback is scheduled to be called. We use this
// to avoid unnecessarily writing to the pipe.
static bool hot = false;
// This mutex protects reading/writing of `hot` and of reading from/writing to
// the pipe.
Mutex m(tct_mtx_plain);

// The buffer we're using for the pipe. This doesn't have to be large,
// in theory it only ever holds zero or one byte.
size_t BUF_SIZE = 256;
void *buf;

void set_fd(bool ready) {
  Guard g(&m);

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

namespace {
void fd_on() {
  set_fd(true);
}

Timer timer(fd_on);
} // namespace

class ResetTimerOnExit {
public:
  ResetTimerOnExit() {
  }
  ~ResetTimerOnExit() {
    ASSERT_MAIN_THREAD()
    // Find the next event in the registry and, if there is one, set the timer.
    Optional<Timestamp> nextEvent = getGlobalRegistry()->nextTimestamp();
    if (nextEvent.has_value()) {
      timer.set(*nextEvent);
    }
  }
};

static void async_input_handler(void *data) {
  ASSERT_MAIN_THREAD()
  set_fd(false);

  if (!at_top_level()) {
    // It's not safe to run arbitrary callbacks when other R code
    // is already running. Wait until we're back at the top level.

    // jcheng 2017-08-02: We can't just leave the file descriptor hot and let
    // async_input_handler get invoked as fast as possible. Previously we did
    // this, but on POSIX systems, it interferes with R_SocketWait.
    // https://github.com/r-lib/later/issues/4
    // Instead, we set the file descriptor to cold, and tell the timer to fire
    // again in a few milliseconds. This should give enough breathing room that
    // we don't interfere with the sockets too much.
    timer.set(Timestamp(0.032));
    return;
  }

  // jcheng 2017-08-01: While callbacks are executing, make the file descriptor
  // not-ready so that our input handler is not even called back by R.
  // Previously we'd let the input handler run but return quickly, but this
  // seemed to cause R_SocketWait to hang (encountered while working with the
  // future package, trying to call value(future) with plan(multisession)).
  ResetTimerOnExit resetTimerOnExit_scope;

  // This try-catch is meant to be similar to the BEGIN_RCPP and VOID_END_RCPP
  // macros. They are needed for two reasons: first, if an exception occurs in
  // any of the callbacks, destructors will still execute; and second, if an
  // exception (including R-level error) occurs in a callback and it reaches
  // the top level in an R input handler, R appears to be unable to handle it
  // properly.
  // https://github.com/r-lib/later/issues/12
  // https://github.com/RcppCore/Rcpp/issues/753
  // https://github.com/r-lib/later/issues/31
  try {
    execCallbacksForTopLevel();
  }
  catch(Rcpp::internal::InterruptedException &e) {
    DEBUG_LOG("async_input_handler: caught Rcpp::internal::InterruptedException", LOG_INFO);
    REprintf("later: interrupt occurred while executing callback.\n");
  }
  catch(std::exception& e){
    DEBUG_LOG("async_input_handler: caught exception", LOG_INFO);
    std::string msg = "later: exception occurred while executing callback: \n";
    msg += e.what();
    msg += "\n";
    REprintf(msg.c_str());
  }
  catch( ... ){
    REprintf("later: c++ exception (unknown reason) occurred while executing callback.\n");
  }
}

InputHandler* inputHandlerHandle;
InputHandler* dummyInputHandlerHandle;

// If the real input handler has been removed, the dummy input handler removes
// itself. The real input handler cannot remove both; otherwise a segfault
// could occur.
static void remove_dummy_handler(void *data) {
  ASSERT_MAIN_THREAD()
  removeInputHandler(&R_InputHandlers, dummyInputHandlerHandle);
  if (dummy_pipe_in  > 0) {
    close(dummy_pipe_in);
    dummy_pipe_in  = -1;
  }
  if (dummy_pipe_out > 0) {
    close(dummy_pipe_out);
    dummy_pipe_out = -1;
  }
}

// Callback to run in child process after forking.
void child_proc_after_fork() {
  ASSERT_MAIN_THREAD()
  if (initialized) {
    removeInputHandler(&R_InputHandlers, inputHandlerHandle);

    if (pipe_in  > 0) {
      close(pipe_in);
      pipe_in = -1;
    }
    if (pipe_out > 0) {
      close(pipe_out);
      pipe_out = -1;
    }

    removeInputHandler(&R_InputHandlers, dummyInputHandlerHandle);
    if (dummy_pipe_in  > 0) {
      close(dummy_pipe_in);
      dummy_pipe_in  = -1;
    }
    if (dummy_pipe_out > 0) {
      close(dummy_pipe_out);
      dummy_pipe_out = -1;
    }

    initialized = 0;
  }
}

void ensureAutorunnerInitialized() {
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

    inputHandlerHandle = addInputHandler(R_InputHandlers, pipe_out, async_input_handler, LATER_ACTIVITY);

   // If the R process is forked, make sure that the child process doesn't mess
   // with the pipes. This also means that functions scheduled in the child
   // process with `later()` will only work if `run_now()` is called. In this
   // situation, there's also the danger that a function will be scheduled by
   // the parent process and then will be executed in the child process (in
   // addition to in the parent process).
   // https://github.com/r-lib/later/issues/140
   pthread_atfork(NULL, NULL, child_proc_after_fork);

    // Need to add a dummy input handler to avoid segfault when the "real"
    // input handler removes the subsequent input handler in the linked list.
    // See https://github.com/rstudio/httpuv/issues/78
    int dummy_pipes[2];
    if (pipe(dummy_pipes)) {
      Rf_error("Failed to create pipe");
      return;
    }
    dummy_pipe_out = dummy_pipes[0];
    dummy_pipe_in  = dummy_pipes[1];
    dummyInputHandlerHandle = addInputHandler(R_InputHandlers, dummy_pipe_out, remove_dummy_handler, LATER_DUMMY_ACTIVITY);

    initialized = 1;
  }
}

void deInitialize() {
  ASSERT_MAIN_THREAD()
  if (initialized) {
    removeInputHandler(&R_InputHandlers, inputHandlerHandle);
    if (pipe_in  > 0) {
      close(pipe_in);
      pipe_in = -1;
    }
    if (pipe_out > 0) {
      close(pipe_out);
      pipe_out = -1;
    }
    initialized = 0;

    // Trigger remove_dummy_handler()
    // Store `ret` because otherwise it raises a significant warning.
    ssize_t ret = write(dummy_pipe_in, "a", 1);
    (void)ret; // squelch compiler warning
  }
}

uint64_t doExecLater(std::shared_ptr<CallbackRegistry> callbackRegistry, Rcpp::Function callback, double delaySecs, bool resetTimer) {
  ASSERT_MAIN_THREAD()
  uint64_t callback_id = callbackRegistry->add(callback, delaySecs);

  // The timer needs to be reset only if we're using the global loop, because
  // this usage of the timer is relevant only when the event loop is driven by
  // R's input handler (at the idle console), and only the global loop is by
  // that.
  if (resetTimer)
    timer.set(*(callbackRegistry->nextTimestamp()));

  return callback_id;
}

uint64_t doExecLater(std::shared_ptr<CallbackRegistry> callbackRegistry, void (*callback)(void*), void* data, double delaySecs, bool resetTimer) {
  uint64_t callback_id = callbackRegistry->add(callback, data, delaySecs);

  if (resetTimer)
    timer.set(*(callbackRegistry->nextTimestamp()));

  return callback_id;
}

#endif // ifndef _WIN32
