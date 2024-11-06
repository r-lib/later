#include "fd.h"
#include <Rcpp.h>
#include <unistd.h>
#include <cstdlib>
#include <atomic>
#include <memory>
#include "tinycthread.h"
#include "later.h"
#include "callback_registry_table.h"

extern CallbackRegistryTable callbackRegistryTable;

class ThreadArgs {
public:
  ThreadArgs(
    int num_fds,
    struct pollfd *fds,
    double timeout,
    int loop
  )
    : timeout(createTimestamp(timeout)),
      flag(std::make_shared<std::atomic<bool>>(false)),
      fds(std::vector<struct pollfd>(fds, fds + num_fds)),
      results(std::vector<int>(num_fds)),
      loop(loop) {}

  ThreadArgs(
    const Rcpp::Function& func,
    int num_fds,
    struct pollfd *fds,
    double timeout,
    int loop
  ) : ThreadArgs(num_fds, fds, timeout, loop) {
    callback = std::unique_ptr<Rcpp::Function>(new Rcpp::Function(func));
  }

  ThreadArgs(
    void (*func)(int *, void *),
    void *data,
    int num_fds,
    struct pollfd *fds,
    double timeout,
    int loop
  ) : ThreadArgs(num_fds, fds, timeout, loop) {
    callback_native = std::bind(func, std::placeholders::_1, data);
  }

  Timestamp timeout;
  std::shared_ptr<std::atomic<bool>> flag;
  std::unique_ptr<Rcpp::Function> callback = nullptr;
  std::function<void (int *)> callback_native = nullptr;
  std::vector<struct pollfd> fds;
  std::vector<int> results;
  const int loop;

private:
  static Timestamp createTimestamp(double timeout) {
    if (timeout > 3e10) {
      timeout = 3e10; // "1000 years ought to be enough for anybody" --Bill Gates
    } else if (timeout < 0) {
      timeout = 1; // curl_multi_timeout() uses -1 to denote a default we set at 1s
    }
    return Timestamp(timeout);
  }

};

static void later_callback(void *arg) {
  ASSERT_MAIN_THREAD()

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(static_cast<std::shared_ptr<ThreadArgs>*>(arg));
  std::shared_ptr<ThreadArgs> args = *argsptr;
  const bool flag = args->flag->load();
  // Mark the cancellation flag so that future requests to fd_cancel return false
  args->flag->store(true);
  if (flag)
    return;
  if (args->callback != nullptr) {
    Rcpp::LogicalVector results(args->results.begin(), args->results.end());
    (*args->callback)(results);
  } else {
    args->callback_native(args->results.data());
  }

}

// CONSIDER: if necessary to add method for HANDLES on Windows. Would be different code to SOCKETs.
// TODO: implement re-usable background thread.
static int wait_thread(void *arg) {

  tct_thrd_detach(tct_thrd_current());

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(static_cast<std::shared_ptr<ThreadArgs>*>(arg));
  std::shared_ptr<ThreadArgs> args = *argsptr;

  // poll() whilst checking for cancellation at intervals

  int ready;
  double waitFor = std::fmax(args->timeout.diff_secs(Timestamp()), 0);
  do {
    // Never wait for longer than ~1 second so we can check for cancellation
    waitFor = std::fmin(waitFor, 1.024);
    ready = LATER_POLL_FUNC(args->fds.data(), args->fds.size(), static_cast<int>(waitFor * 1000));
    if (args->flag->load()) return 1;
    if (ready) break;
  } while ((waitFor = args->timeout.diff_secs(Timestamp())) > 0);

  // store pollfd revents in args->results for use by callback

  if (ready > 0) {
    for (std::size_t i = 0; i < args->fds.size(); i++) {
      (args->results)[i] = (args->fds)[i].revents == 0 ? 0 : (args->fds)[i].revents & (POLLIN | POLLOUT) ? 1: NA_INTEGER;
    }
  } else if (ready < 0) {
    std::fill(args->results.begin(), args->results.end(), NA_INTEGER);
  }

  callbackRegistryTable.scheduleCallback(later_callback, static_cast<void *>(argsptr.release()), 0, args->loop);

  return 0;

}

static int execLater_launch_thread(std::shared_ptr<ThreadArgs> args) {

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(new std::shared_ptr<ThreadArgs>(args));

  tct_thrd_t thr;

  return tct_thrd_create(&thr, &wait_thread, static_cast<void *>(argsptr.release())) != tct_thrd_success;

}

static SEXP execLater_fd_impl(const Rcpp::Function& callback, int num_fds, struct pollfd *fds, double timeout, int loop_id) {

  std::shared_ptr<ThreadArgs> args = std::make_shared<ThreadArgs>(callback, num_fds, fds, timeout, loop_id);

  if (execLater_launch_thread(args))
    Rcpp::stop("Thread creation failed");

  Rcpp::XPtr<std::shared_ptr<std::atomic<bool>>> xptr(new std::shared_ptr<std::atomic<bool>>(args->flag), true);
  return xptr;

}

// native version
static int execLater_fd_native(void (*func)(int *, void *), void *data, int num_fds, struct pollfd *fds, double timeout, int loop_id) {

  std::shared_ptr<ThreadArgs> args = std::make_shared<ThreadArgs>(func, data, num_fds, fds, timeout, loop_id);

  return execLater_launch_thread(args);

}

// [[Rcpp::export]]
Rcpp::RObject execLater_fd(Rcpp::Function callback, Rcpp::IntegerVector readfds, Rcpp::IntegerVector writefds,
                           Rcpp::IntegerVector exceptfds, Rcpp::NumericVector timeoutSecs, Rcpp::IntegerVector loop_id) {

  const int rfds = static_cast<int>(readfds.size());
  const int wfds = static_cast<int>(writefds.size());
  const int efds = static_cast<int>(exceptfds.size());
  const int num_fds = rfds + wfds + efds;
  const double timeout = num_fds ? timeoutSecs[0] : 0;
  const int loop = loop_id[0];

  std::vector<struct pollfd> pollfds;
  pollfds.reserve(num_fds);
  struct pollfd pfd;

  for (int i = 0; i < rfds; i++) {
    pfd.fd = readfds[i];
    pfd.events = POLLIN;
    pfd.revents = 0;
    pollfds.push_back(pfd);
  }
  for (int i = 0; i < wfds; i++) {
    pfd.fd = writefds[i];
    pfd.events = POLLOUT;
    pfd.revents = 0;
    pollfds.push_back(pfd);
  }
  for (int i = 0; i < efds; i++) {
    pfd.fd = exceptfds[i];
    pfd.events = 0;
    pfd.revents = 0;
    pollfds.push_back(pfd);
  }

  return execLater_fd_impl(callback, num_fds, pollfds.data(), timeout, loop);

}

// [[Rcpp::export]]
Rcpp::LogicalVector fd_cancel(Rcpp::RObject xptr) {

  Rcpp::XPtr<std::shared_ptr<std::atomic<bool>>> flag(xptr);

  if ((*flag)->load())
    return false;

  (*flag)->store(true);
  return true;

}

// Schedules a C function that takes a pointer to an integer array (provided by
// this function when calling back) and a void * argument, to execute on file
// descriptor readiness. Returns 0 upon success and 1 if creating the wait
// thread failed. NOTE: this is different to execLaterNative2() which returns 0
// on failure.
extern "C" int execLaterFdNative(void (*func)(int *, void *), void *data, int num_fds, struct pollfd *fds, double timeoutSecs, int loop_id) {
  ensureInitialized();
  return execLater_fd_native(func, data, num_fds, fds, timeoutSecs, loop_id);
}
