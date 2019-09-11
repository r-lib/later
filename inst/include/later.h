// DO NOT include later.h directly from other packages; use later_api.h instead!
#ifndef _later_later_h
#define _later_later_h

#include <iostream>
#include <Rcpp.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
// Taken from http://tolstoy.newcastle.edu.au/R/e2/devel/06/11/1242.html
// Undefine the Realloc macro, which is defined by both R and by Windows stuff
#undef Realloc
// Also need to undefine the Free macro
#undef Free
#include <windows.h>
#else // _WIN32
#include <pthread.h>
#endif // _WIN32

namespace later {


// This should be incremented each time this file changes. If it mismatches
// the result from apiVersion() (in later.cpp), then there's a problem.
#define LATER_H_API_VERSION 2

#define GLOBAL_LOOP 0

// This header uses version 2 of the later C++ API, and another package's DLL
// will be compiled with this header. However, it is possible that the later
// DLL installed on the system will have a different version of API. If the
// version is 2 or higher, then we can call the `apiVersion()` function. But
// for version 1, that function did not exist, so we have to use a different
// method to check for the V1 API: We'll evaluate the R expression
// `packageVersion("later") < "0.8.0.9004"`. If it returns TRUE, then the
// installed version of the later package provides the V1 C++ API. If it
// returns FALSE, then the API is version 2 or greater.
inline bool dll_has_v1_api() {
  SEXP e, ret;
  bool result;
  Rprintf("Building expression\n");

  // This is the expression `packageVersion("later") < "0.8.0.9004"`
  e = PROTECT(
    Rf_lang3(
      Rf_install("<"),
      Rf_lang2(Rf_install("packageVersion"), Rf_mkString("later")),
      Rf_mkString("0.8.0.9004")
    )
  );
  Rprintf("Evaluating code\n");
  ret = PROTECT(Rf_eval(e, R_GlobalEnv));
  Rprintf("Evaluated code\n");
  // Rf_eval(Rf_lang2(Rf_install("print"), ret), R_GlobalEnv);

  if (TYPEOF(ret) == LGLSXP && LOGICAL(ret)[0]) {
    // If it's a logical vector and the first element is TRUE.
    result = true;
  } else {
    result = false;
  }

  UNPROTECT(2);
  Rprintf("Returning value\n");
  return result;
}

inline void later(void (*func)(void*), void* data, double secs, int loop) {
  // This function works by retrieving the later::execLaterNative2 function
  // pointer using R_GetCCallable the first time it's called (per compilation
  // unit, since it's inline). execLaterNative2 is designed to be safe to call
  // from any thread, but R_GetCCallable is only safe to call from R's main
  // thread (otherwise you get stack imbalance warnings or worse). Therefore,
  // we have to ensure that the first call to execLaterNative2 happens on the
  // main thread. We accomplish this using a statically initialized object,
  // in later_api.h. Therefore, any other packages wanting to call
  // execLaterNative2 need to use later_api.h, not later.h.
  //
  // You may wonder why we used the filenames later_api.h/later.h instead of
  // later.h/later_impl.h; it's because Rcpp treats $PACKAGE.h files
  // specially by including them in RcppExports.cpp, and we definitely
  // do not want the static initialization to happen there.

  // The function type for the real execLaterNative2
  typedef void (*elnfun)(void (*func)(void*), void*, double, int);
  static elnfun eln = NULL;
  if (!eln) {
    // Initialize if necessary
    if (func) {
      // We're not initialized but someone's trying to actually schedule
      // some code to be executed!
      REprintf(
        "Warning: later::execLaterNative2 called in uninitialized state. "
        "If you're using <later.h>, please switch to <later_api.h>.\n"
      );
    }
    eln = (elnfun)R_GetCCallable("later", "execLaterNative2");
  }

  // We didn't want to execute anything, just initialize
  if (!func) {
    return;
  }

  eln(func, data, secs, loop);
}

inline void later(void (*func)(void*), void* data, double secs) {

  static bool has_run = false;
  static bool v1_api;

  if (!has_run) {
    has_run = true;
    v1_api = dll_has_v1_api();
    if (v1_api) {
      Rprintf("later DLL has v1 API\n");
    } else {
      Rprintf("later DLL has v2 API\n");
    }
  }

  if (v1_api) {
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

  } else {
    later(func, data, secs, GLOBAL_LOOP);
  }
}


class BackgroundTask {

public:
  BackgroundTask() {}
  virtual ~BackgroundTask() {}

  // Start executing the task
  void begin() {
#ifndef _WIN32
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t t;
    pthread_create(&t, NULL, BackgroundTask::task_main, this);
    pthread_attr_destroy(&attr);
#else
    HANDLE hThread = ::CreateThread(
      NULL, 0,
      BackgroundTask::task_main_win,
      this,
      0,
      NULL
    );
    ::CloseHandle(hThread);
#endif
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
  static void* task_main(void* data) {
    BackgroundTask* task = reinterpret_cast<BackgroundTask*>(data);
    // TODO: Error handling
    task->execute();
    later(&BackgroundTask::result_callback, task, 0);
    return NULL;
  }

#ifdef _WIN32
  static DWORD WINAPI task_main_win(LPVOID lpParameter) {
    task_main(lpParameter);
    return 1;
  }
#endif

  static void result_callback(void* data) {
    BackgroundTask* task = reinterpret_cast<BackgroundTask*>(data);
    // TODO: Error handling
    task->complete();
    delete task;
  }
};

} // namespace later

#endif
