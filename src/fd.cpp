#ifdef _WIN32
#ifndef FD_SETSIZE
#define FD_SETSIZE 2048  // Increase max fds - only for select() fallback
#endif
#include <winsock2.h>
#else
#include <poll.h>
#endif
#include <Rcpp.h>
#include <unistd.h>
#include <cstdlib>
#include <atomic>
#include "tinycthread.h"
#include "later.h"
#include "callback_registry_table.h"

#if R_VERSION < R_Version(4, 5, 0)
inline SEXP R_mkClosure(SEXP formals, SEXP body, SEXP env) {
  SEXP fun = Rf_allocSExp(CLOSXP);
  SET_FORMALS(fun, formals);
  SET_BODY(fun, body);
  SET_CLOENV(fun, env);
  return fun;
}
#endif

extern CallbackRegistryTable callbackRegistryTable;
extern SEXP later_fdcancel;
extern SEXP later_invisibleSymbol;

#ifdef _WIN32
#define POLL_FUNC WSAPoll
#else
#define POLL_FUNC poll
#endif

typedef struct ThreadArgs_s {
  std::shared_ptr<std::atomic<bool>> flag;
  std::unique_ptr<std::vector<int>> fds;
  SEXP callback;
  double timeout;
  int rfds, wfds, efds;
  int num_fds;
  int loop;
} ThreadArgs;

static void later_callback(void *arg) {

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(static_cast<std::shared_ptr<ThreadArgs>*>(arg));
  std::shared_ptr<ThreadArgs> args = *argsptr;
  const bool flag = args->flag->load();
  args->flag->store(true);
  if (!flag) {
    std::memcpy((void *) DATAPTR_RO(CADR(args->callback)), args->fds->data(), args->num_fds * sizeof(int));
    Rf_eval(args->callback, R_GlobalEnv);
  }
  R_ReleaseObject(args->callback);

}

// CONSIDER: if necessary to add method for HANDLES on Windows. Would be different code to SOCKETs.
// TODO: implement re-usable background thread.
static int wait_thread(void *arg) {

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(static_cast<std::shared_ptr<ThreadArgs>*>(arg));
  std::shared_ptr<ThreadArgs> args = *argsptr;

  int result;

#ifndef POLLIN // fall back to select() for R on older Windows

  fd_set readfds, writefds, exceptfds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  const int max_fd = *std::max_element(args->fds->begin(), args->fds->end());
  for (int i = 0; i < args->rfds; i++) {
    FD_SET((*args->fds)[i], &readfds);
  }
  for (int i = args->rfds; i < (args->rfds + args->wfds); i++) {
    FD_SET((*args->fds)[i], &writefds);
  }
  for (int i = args->rfds + args->wfds; i < args->num_fds; i++) {
    FD_SET((*args->fds)[i], &exceptfds);
  }

  struct timeval tv;
  const int use_timeout = args->timeout != R_PosInf;
  if (use_timeout) {
    tv.tv_sec = args->timeout < 0 ? 1 : (int) args->timeout;
    tv.tv_usec = args->timeout < 0 ? 0 : ((int) (args->timeout * 1000)) % 1000 * 1000;
  }

  result = select(max_fd + 1, &readfds, &writefds, &exceptfds, use_timeout ? &tv : NULL);

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

  int timeout = args->timeout == R_PosInf ? -1 : args->timeout < 0 ? 1000 : (int) (args->timeout * 1000);

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

  result = POLL_FUNC(pollfds.data(), args->num_fds, timeout);

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

  callbackRegistryTable.scheduleCallback(later_callback, static_cast<void *>(argsptr.release()), 0, args->loop);

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
  std::unique_ptr<std::vector<int>> fdvals(new std::vector<int>(num_fds));
  std::shared_ptr<std::atomic<bool>> flag = std::make_shared<std::atomic<bool>>();
  args->flag = flag;
  args->timeout = timeoutSecs[0];
  args->loop = loop_id[0];
  args->num_fds = num_fds;
  args->rfds = rfds;
  args->wfds = wfds;
  args->efds = efds;
  for (int i = 0; i < rfds; i++) {
    (*fdvals)[i] = readfds[i];
  }
  for (int i = 0; i < wfds; i++) {
    (*fdvals)[rfds + i] = writefds[i];
  }
  for (int i = 0; i < efds; i++) {
    (*fdvals)[rfds + wfds + i] = exceptfds[i];
  }
  args->fds = std::move(fdvals);

  SEXP call = Rf_lcons(callback, Rf_cons(Rf_allocVector(LGLSXP, num_fds), R_NilValue));
  R_PreserveObject(call);
  args->callback = call;

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(new std::shared_ptr<ThreadArgs>(args));

  tct_thrd_t thr;
  if (tct_thrd_create(&thr, &wait_thread, static_cast<void *>(argsptr.release())) != tct_thrd_success)
    Rcpp::stop("Thread creation failed");
  tct_thrd_detach(thr);

  Rcpp::XPtr<std::shared_ptr<std::atomic<bool>>> xptr(new std::shared_ptr<std::atomic<bool>>(flag), true);
  SEXP body, func;
  PROTECT(body = Rf_lang2(later_invisibleSymbol, Rf_lang2(later_fdcancel, xptr)));
  func = R_mkClosure(R_NilValue, body, R_BaseEnv);
  UNPROTECT(1);

  return func;

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
