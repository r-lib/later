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

#ifdef _WIN32
typedef HANDLE thread_object;
#else
typedef pthread_t thread_object;
#endif

typedef struct ThreadArgs_s {
  SEXP callback;
  double timeout;
  std::unique_ptr<std::vector<int>> fds;
  int rfds, wfds, efds;
  int num_fds;
  int loop;
} ThreadArgs;

static void thread_finalizer(SEXP xptr) {

  if (R_ExternalPtrAddr(xptr) == NULL) return;
  thread_object *xp = (thread_object *) R_ExternalPtrAddr(xptr);
#ifdef _WIN32
  CloseHandle(*xp);
#endif
  R_Free(xp);

}

static void later_callback(void *arg) {

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(static_cast<std::shared_ptr<ThreadArgs>*>(arg));
  std::shared_ptr<ThreadArgs> args = *argsptr;
  Rf_eval(args->callback, R_GlobalEnv);
  R_ReleaseObject(args->callback);

}

// CONSIDER: if necessary to add method for HANDLES on Windows. Would be different code to SOCKETs.
// TODO: implement re-usable background thread.
static void *select_thread(void *arg) {

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(static_cast<std::shared_ptr<ThreadArgs>*>(arg));
  std::shared_ptr<ThreadArgs> args = *argsptr;

  int result;
  int *values;

#ifndef POLLIN // fall back to select() for R on older Windows

  fd_set readfds, writefds, exceptfds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  int max_fd = *std::max_element(args->fds->begin(), args->fds->end());
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
  int use_timeout = args->timeout != R_PosInf;
  if (use_timeout) {
    tv.tv_sec = args->timeout < 0 ? 1 : (int) args->timeout;
    tv.tv_usec = args->timeout < 0 ? 0 : ((int) (args->timeout * 1000)) % 1000 * 1000;
  }

  result = select(max_fd + 1, &readfds, &writefds, &exceptfds, use_timeout ? &tv : NULL);

  values = (int *) DATAPTR_RO(CADR(args->callback));
  if (result > 0) {
    for (int i = 0; i < args->rfds; i++) {
      values[i] = FD_ISSET((*args->fds)[i], &readfds);
    }
    for (int i = args->rfds; i < (args->rfds + args->wfds); i++) {
      FD_ISSET((*args->fds)[i], &writefds);
    }
    for (int i = args->rfds + args->wfds; i < args->num_fds; i++) {
      FD_ISSET((*args->fds)[i], &exceptfds);
    }
  } else {
    for (int i = 0; i < args->num_fds; i++) {
      values[i] = result == 0 ? 0 : R_NaInt;
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
#ifdef _WIN32
  result = WSAPoll(pollfds.data(), args->num_fds, timeout);
#else
  result = poll(pollfds.data(), args->num_fds, timeout);
#endif

  values = (int *) DATAPTR_RO(CADR(args->callback));
  if (result > 0) {
    for (int i = 0; i < args->num_fds; i++) {
      values[i] = pollfds[i].revents & (POLLIN | POLLOUT) ? 1 : pollfds[i].revents & (POLLNVAL | POLLHUP | POLLERR) ? R_NaInt : 0;
    }
  } else {
    for (int i = 0; i < args->num_fds; i++) {
      values[i] = result == 0 ? 0 : R_NaInt;
    }
  }

#endif // POLLIN

  callbackRegistryTable.scheduleCallback(later_callback, static_cast<void *>(argsptr.release()), 0, args->loop);

  return nullptr;

}

#ifdef _WIN32
static DWORD WINAPI select_thread_win(LPVOID lpParameter) {
  select_thread(lpParameter);
  return 1;
}
#endif

// [[Rcpp::export]]
Rcpp::RObject execLater_fd(Rcpp::Function callback, Rcpp::IntegerVector readfds, Rcpp::IntegerVector writefds,
                                 Rcpp::IntegerVector exceptfds, Rcpp::NumericVector timeoutSecs, Rcpp::IntegerVector loop_id) {

  int rfds = static_cast<int>(readfds.size());
  int wfds = static_cast<int>(writefds.size());
  int efds = static_cast<int>(exceptfds.size());
  int num_fds = rfds + wfds + efds;
  if (num_fds == 0)
    Rf_error("later_fd: no file descriptors supplied");
  double timeout = timeoutSecs[0];
  int loop = loop_id[0];

  SEXP call = Rf_lcons(callback, Rf_cons(Rf_allocVector(LGLSXP, num_fds), R_NilValue));
  R_PreserveObject(call);

  std::shared_ptr<ThreadArgs> args = std::make_shared<ThreadArgs>();
  std::unique_ptr<std::vector<int>> fdvals(new std::vector<int>(num_fds));
  args->timeout = timeout;
  args->loop = loop;
  args->callback = call;
  args->num_fds = num_fds;
  args->rfds = rfds;
  args->wfds = wfds;
  args->efds = efds;
  if (rfds)
    std::memcpy(fdvals->data(), readfds.begin(), rfds * sizeof(int));
  if (wfds)
    std::memcpy(fdvals->data() + rfds * sizeof(int), writefds.begin(), wfds * sizeof(int));
  if (efds)
    std::memcpy(fdvals->data() + (rfds + wfds) * sizeof(int), exceptfds.begin(), efds * sizeof(int));
  args->fds = std::move(fdvals);

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(new std::shared_ptr<ThreadArgs>(args));

  thread_object *thr = R_Calloc(1, thread_object);

#ifdef _WIN32

  *thr = CreateThread(NULL, 0, select_thread_win, static_cast<LPVOID>(argsptr.release()), 0, NULL);

  if (*thr == NULL)
    Rcpp::stop("thread creation error: " + std::to_string(GetLastError()));

#else

  pthread_attr_t pt_attr;
  if (pthread_attr_init(&pt_attr))
    Rcpp::stop("thread attr error: " + std::string(strerror(errno)));
  if (pthread_attr_setdetachstate(&pt_attr, PTHREAD_CREATE_DETACHED) ||
      pthread_create(thr, &pt_attr, select_thread, static_cast<void *>(argsptr.release()))) {
    pthread_attr_destroy(&pt_attr);
    Rcpp::stop("thread creation error: " + std::string(strerror(errno)));
  }
  if (pthread_attr_destroy(&pt_attr))
    Rcpp::stop("thread attr error: " + std::string(strerror(errno)));

#endif

  SEXP xptr, body, func;
  xptr = R_MakeExternalPtr(thr, R_NilValue, R_NilValue);
  PROTECT(body = Rf_lcons(later_fdcancel, Rf_cons(xptr, R_NilValue)));
  R_RegisterCFinalizerEx(xptr, thread_finalizer, TRUE);
  func = R_mkClosure(R_NilValue, body, R_BaseEnv);
  UNPROTECT(1);

  return func;

}

// [[Rcpp::export]]
Rcpp::LogicalVector fd_cancel(Rcpp::RObject xptr) {

  if (TYPEOF(xptr) != EXTPTRSXP || R_ExternalPtrAddr(xptr) == NULL)
    Rcpp::stop("invalid external pointer");

  thread_object *thr = (thread_object *) R_ExternalPtrAddr(xptr);

#ifdef _WIN32
  return TerminateThread(*thr, 0) != 0;
#else
  return pthread_cancel(*thr) == 0;
#endif

}
