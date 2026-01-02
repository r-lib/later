#ifdef _WIN32

#include "later.h"

#include <Rcpp.h>
#include <Rinternals.h>
#include <queue>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "debug.h"

using namespace Rcpp;

// Whether we have initialized the message-only window.
static int initialized = 0;

// The handle to the message-only window
static HWND hwnd;

// The ID of the timer
static UINT_PTR TIMER_ID = 1;

// The window message we use to run SetTimer on the main thread
static const UINT WM_SETUPTIMER = WM_USER + 101;

static void setupTimer() {
  if (!SetTimer(hwnd, TIMER_ID, USER_TIMER_MINIMUM, NULL)) {
    Rf_error("Failed to schedule callback timer");
  }
}

static bool executeHandlers() {
  if (!at_top_level()) {
    // It's not safe to run arbitrary callbacks when other R code
    // is already running. Wait until we're back at the top level.
    return false;
  }

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
    REprintf("later: interrupt occurred while executing callback.\n");
  }
  catch(Rcpp::LongjumpException& e){
    REprintf("later: exception occurred while executing callback.\n");
  }
  catch(std::exception& e){
    std::string msg = "later: exception occurred while executing callback: \n";
    msg += e.what();
    msg += "\n";
    REprintf("%s", msg.c_str());
  }
  catch( ... ){
    REprintf("later: c++ exception (unknown reason) occurred while executing callback.\n");
  }

  return idle(GLOBAL_LOOP);
}

LRESULT CALLBACK callbackWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_TIMER:
    if (executeHandlers()) {
      KillTimer(hwnd, TIMER_ID);
    }
    break;
  case WM_SETUPTIMER:
    setupTimer();
    break;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

void ensureAutorunnerInitialized() {
  if (!initialized) {
    static const char* class_name = "R_LATER_WINDOW_CLASS";
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = callbackWndProc;
    wc.hInstance = NULL;
    wc.lpszClassName = class_name;
    if (!RegisterClassEx(&wc)) {
      Rcpp::stop("Failed to register window class");
    }

    hwnd = CreateWindowEx(0, class_name, "dummy_name", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    if (!hwnd) {
      Rcpp::stop("Failed to create message-only window");
    }

    initialized = 1;
  }
}

uint64_t doExecLater(std::shared_ptr<CallbackRegistry> callbackRegistry, Rcpp::Function callback, double delaySecs, bool resetTimer) {
  uint64_t callback_id = callbackRegistry->add(callback, delaySecs);

  if (resetTimer)
    setupTimer();

  return callback_id;
}

uint64_t doExecLater(std::shared_ptr<CallbackRegistry> callbackRegistry, void (*func)(void*), void* data, double delaySecs, bool resetTimer) {
  uint64_t callback_id = callbackRegistry->add(func, data, delaySecs);

  if (resetTimer) {
    if (GetCurrentThreadId() == GetWindowThreadProcessId(hwnd, NULL)) {
      setupTimer();
    } else {
      // Not safe to setup the timer from this thread. Instead, send a
      // message to the main thread that the timer should be set up.
      PostMessage(hwnd, WM_SETUPTIMER, 0, 0);
    }
  }

  return callback_id;
}

#endif // ifdef _WIN32
