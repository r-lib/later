#ifdef _WIN32
#ifndef FD_SETSIZE
#define FD_SETSIZE 2048  // Set max fds - only for select() fallback
#endif
#include <winsock2.h>
#else
#include <poll.h>
#endif
#include <Rcpp.h>
#include <unistd.h>
#include <cstdlib>
#include <atomic>
#include <memory>
#include "tinycthread.h"
#include "later.h"

#define LATER_INTERVAL 1024

#ifdef _WIN32
#define POLL_FUNC WSAPoll
#else
#define POLL_FUNC poll
#endif

typedef struct ThreadArgs_s {
  std::shared_ptr<std::atomic<bool>> flag;
  std::unique_ptr<Rcpp::Function> callback;
  std::unique_ptr<std::vector<int>> fds;
  double timeout;
  int rfds, wfds, efds, num_fds;
  int loop;
} ThreadArgs;

static void later_callback(void *arg) {

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(static_cast<std::shared_ptr<ThreadArgs>*>(arg));
  std::shared_ptr<ThreadArgs> args = *argsptr;
  const bool flag = args->flag->load();
  args->flag->store(true);
  if (!flag) {
    Rcpp::LogicalVector results = Rcpp::wrap(*args->fds);
    (*args->callback)(results);
  }

}

// CONSIDER: if necessary to add method for HANDLES on Windows. Would be different code to SOCKETs.
// TODO: implement re-usable background thread.
static int wait_thread(void *arg) {

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(static_cast<std::shared_ptr<ThreadArgs>*>(arg));
  std::shared_ptr<ThreadArgs> args = *argsptr;

  int result;

  const bool infinite = args->timeout == R_PosInf;
  int timeoutms = infinite || args->timeout < 0 ? 1000 : (int) (args->timeout * 1000);
  int repeats = infinite || args->timeout < 0 ? 0 : timeoutms / LATER_INTERVAL;

#ifndef POLLIN // fall back to select() for R <= 4.1 and older Windows

  fd_set readfds, writefds, exceptfds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  const int max_fd = *std::max_element(args->fds->begin(), args->fds->end());
  struct timeval tv;

  do {
    for (int i = 0; i < args->rfds; i++) {
      FD_SET((*args->fds)[i], &readfds);
    }
    for (int i = args->rfds; i < (args->rfds + args->wfds); i++) {
      FD_SET((*args->fds)[i], &writefds);
    }
    for (int i = args->rfds + args->wfds; i < args->num_fds; i++) {
      FD_SET((*args->fds)[i], &exceptfds);
    }
    tv.tv_sec = (repeats ? LATER_INTERVAL : timeoutms) / 1000;
    tv.tv_usec = ((repeats ? LATER_INTERVAL : timeoutms) % 1000) * 1000;

    result = select(max_fd + 1, &readfds, &writefds, &exceptfds, &tv);

    if (args->flag->load()) return 1;
    if (result) break;
  } while (infinite || (repeats-- && (timeoutms -= LATER_INTERVAL)));

  if (result > 0) {
    for (int i = 0; i < args->rfds; i++) {
      (*args->fds)[i] = FD_ISSET((*args->fds)[i], &readfds);
    }
    for (int i = args->rfds; i < (args->rfds + args->wfds); i++) {
      (*args->fds)[i] = FD_ISSET((*args->fds)[i], &writefds);
    }
    for (int i = args->rfds + args->wfds; i < args->num_fds; i++) {
      (*args->fds)[i] = FD_ISSET((*args->fds)[i], &exceptfds);
    }
  } else if (result == 0) {
    std::memset(args->fds->data(), 0, args->num_fds * sizeof(int));
  } else {
    for (int i = 0; i < args->num_fds; i++) {
      (*args->fds)[i] = R_NaInt;
    }
  }

#else

  std::vector<struct pollfd> pollfds;
  pollfds.reserve(args->num_fds);
  struct pollfd pfd;
  for (int i = 0; i < args->rfds; i++) {
    pfd.fd = (*args->fds)[i];
    pfd.events = POLLIN;
    pfd.revents = 0;
    pollfds.push_back(pfd);
  }
  for (int i = args->rfds; i < (args->rfds + args->wfds); i++) {
    pfd.fd = (*args->fds)[i];
    pfd.events = POLLOUT;
    pfd.revents = 0;
    pollfds.push_back(pfd);
  }
  for (int i = args->rfds + args->wfds; i < args->num_fds; i++) {
    pfd.fd = (*args->fds)[i];
    pfd.events = 0;
    pfd.revents = 0;
    pollfds.push_back(pfd);
  }

  do {
    result = POLL_FUNC(pollfds.data(), args->num_fds, repeats ? LATER_INTERVAL : timeoutms);
    if (args->flag->load()) return 1;
    if (result) break;
  } while (infinite || (repeats-- && (timeoutms -= LATER_INTERVAL)));

  if (result > 0) {
    for (int i = 0; i < args->rfds; i++) {
      (*args->fds)[i] = pollfds[i].revents == 0 ? 0 : pollfds[i].revents & POLLIN ? 1: R_NaInt;
    }
    for (int i = args->rfds; i < (args->rfds + args->wfds); i++) {
      (*args->fds)[i] = pollfds[i].revents == 0 ? 0 : pollfds[i].revents & POLLOUT ? 1 : R_NaInt;
    }
    for (int i = args->rfds + args->wfds; i < args->num_fds; i++) {
      (*args->fds)[i] = pollfds[i].revents != 0;
    }
  } else if (result == 0) {
    std::memset(args->fds->data(), 0, args->num_fds * sizeof(int));
  } else {
    for (int i = 0; i < args->num_fds; i++) {
      (*args->fds)[i] = R_NaInt;
    }
  }

#endif // POLLIN

  execLaterNative2(later_callback, static_cast<void *>(argsptr.release()), 0, args->loop);

  return 0;

}

// [[Rcpp::export]]
Rcpp::RObject execLater_fd(Rcpp::Function callback, Rcpp::IntegerVector readfds, Rcpp::IntegerVector writefds,
                           Rcpp::IntegerVector exceptfds, Rcpp::NumericVector timeoutSecs, Rcpp::IntegerVector loop_id) {

  const int rfds = static_cast<int>(readfds.size());
  const int wfds = static_cast<int>(writefds.size());
  const int efds = static_cast<int>(exceptfds.size());
  const int num_fds = rfds + wfds + efds;
  if (num_fds == 0)
    Rcpp::stop("No file descriptors supplied");

  std::shared_ptr<ThreadArgs> args = std::make_shared<ThreadArgs>();

  args->callback = std::unique_ptr<Rcpp::Function>(new Rcpp::Function(callback));
  args->flag = std::make_shared<std::atomic<bool>>();
  args->timeout = timeoutSecs[0];
  args->loop = loop_id[0];
  args->rfds = rfds;
  args->wfds = wfds;
  args->efds = efds;
  args->num_fds = num_fds;
  args->fds = std::unique_ptr<std::vector<int>>(new std::vector<int>());
  args->fds->reserve(num_fds);
  for (int i = 0; i < rfds; i++) {
    args->fds->push_back(readfds[i]);
  }
  for (int i = 0; i < wfds; i++) {
    args->fds->push_back(writefds[i]);
  }
  for (int i = 0; i < efds; i++) {
    args->fds->push_back(exceptfds[i]);
  }

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(new std::shared_ptr<ThreadArgs>(args));

  tct_thrd_t thr;
  if (tct_thrd_create(&thr, &wait_thread, static_cast<void *>(argsptr.release())) != tct_thrd_success)
    Rcpp::stop("Thread creation failed");
  tct_thrd_detach(thr);

  Rcpp::XPtr<std::shared_ptr<std::atomic<bool>>> xptr(new std::shared_ptr<std::atomic<bool>>(args->flag), true);

  return Rcpp::RObject(xptr);

}

// [[Rcpp::export]]
Rcpp::LogicalVector fd_cancel(Rcpp::RObject xptr) {

  if (TYPEOF(xptr) != EXTPTRSXP || R_ExternalPtrAddr(xptr) == NULL)
    Rcpp::stop("Invalid external pointer");

  Rcpp::XPtr<std::shared_ptr<std::atomic<bool>>> flag(xptr);

  if ((*flag)->load())
    return false;

  (*flag)->store(true);
  return true;

}
