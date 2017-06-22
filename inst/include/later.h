// DO NOT include later.h directly from other packages; use later_api.h instead!
#ifndef _later_later_h
#define _later_later_h

#include <iostream>
#include <Rcpp.h>
namespace {
#include "_later_tinythread.h"
} // namespace

namespace later {

inline void later(void (*func)(void*), void* data, double secs) {
  // This function works by retrieving the later::execLaterNative function
  // pointer using R_GetCCallable the first time it's called (per compilation
  // unit, since it's inline). execLaterNative is designed to be safe to call
  // from any thread, but R_GetCCallable is only safe to call from R's main
  // thread (otherwise you get stack imbalance warnings or worse). Therefore,
  // we have to ensure that the first call to execLaterNative happens on the
  // main thread. We accomplish this using a statically initialized object,
  // in later_api.h. Therefore, any other packages wanting to call
  // execLaterNative need to use later_api.h, not later.h.
  //
  // You may wonder why we used the filenames later_api.h/later.h instead of
  // later.h/later_impl.h; it's because Rcpp treats $PACKAGE.h files
  // specially by including them in RcppExports.cpp, and we definitely
  // do not want the static initialization to happen there.
  
  // The function type for the real execLaterNative
  typedef void (*elnfun)(void (*func)(void*), void*, double);
  static elnfun eln = NULL;
  if (!eln) {
    // Initialize if necessary
    if (func) {
      // We're not initialized but someone's trying to actually schedule
      // some code to be executed!
      REprintf(
        "Warning: later::execLaterNative called in uninitialized state. "
        "If you're using <later.h>, please switch to <later_api.h>.\n"
      );
    }
    eln = (elnfun)R_GetCCallable("later", "execLaterNative");
  }
  
  // We didn't want to execute anything, just initialize
  if (!func) {
    return;
  }
  
  eln(func, data, secs);
}

class BackgroundTask {

public:
  BackgroundTask() {}
  virtual ~BackgroundTask() {}
  
  // Start executing the task  
  void begin() {
    new tthread::thread(BackgroundTask::task_main, this);
  }

protected:
  // The task to be executed on the background thread.
  // Neither the R runtime nor any R data structures may be
  // touched from the background thread; any values that need
  // to be passed into or out of the Execute method must be
  // included as fields on the Task subclass object.
  virtual void execute() = 0;
  
  // A short task that runs on the main R thread after the
  // background task has completed. It's safe to access the
  // R runtime and R data structures from here.
  virtual void complete() = 0;

private:
  static void task_main(void* data) {
    BackgroundTask* task = reinterpret_cast<BackgroundTask*>(data);
    // TODO: Error handling
    task->execute();
    later(&BackgroundTask::result_callback, task, 0);
  }
  
  static void result_callback(void* data) {
    BackgroundTask* task = reinterpret_cast<BackgroundTask*>(data);
    // TODO: Error handling
    task->complete();
    delete task;
  }
};

} // namespace later

#endif