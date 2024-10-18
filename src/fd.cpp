#ifdef _WIN32
#include <winsock2.h>
#endif
#include <Rcpp.h>
#include <unistd.h>
#include <stdlib.h>
#include "later.h"
#include "debug.h"

typedef struct thread_args_s {
  SEXP callback;
  R_xlen_t num_fds;
  int *fds;
  fd_set read_fds;
  struct timeval tv;
  int max_fd;
  int loop;
  bool timer_inf;
} thread_args;

static void later_callback(void *arg) {

  thread_args *args = (thread_args *) arg;
  SEXP results, call;
  PROTECT(results = Rf_allocVector(LGLSXP, args->num_fds));
  int *res = INTEGER(results);
  memcpy(res, args->fds, args->num_fds * sizeof(int));
  PROTECT(call = Rf_lcons(args->callback, Rf_cons(results, R_NilValue)));
  Rf_eval(call, R_GlobalEnv);
  UNPROTECT(2);
  R_ReleaseObject(args->callback);
  R_Free(args->fds);
  R_Free(args);

}

// TODO: add method for HANDLES on Windows
static void *select_thread(void *arg) {

  thread_args *args = (thread_args *) arg;

  int ready = select(args->max_fd + 1, &args->read_fds, NULL, NULL, args->timer_inf ? NULL : &args->tv);

  if (ready < 0) {
    // TODO: fix errno on Windows
    err_printf("select error: %s\n", strerror(errno));
    R_ReleaseObject(args->callback);
    R_Free(args->fds);
    R_Free(args);
    return NULL;
  }

  for (R_xlen_t i = 0; i < args->num_fds; i++) {
    args->fds[i] = FD_ISSET(args->fds[i], &args->read_fds) != 0;
  }

  execLaterNative2(later_callback, arg, 0, args->loop);

  return NULL;

}

#ifdef _WIN32
static DWORD WINAPI select_thread_win(LPVOID lpParameter) {
  select_thread(lpParameter);
  return 1;
}
#endif

// [[Rcpp::export]]
Rcpp::LogicalVector execLater_fd(Rcpp::Function callback, Rcpp::IntegerVector fds, Rcpp::NumericVector timeoutSecs, Rcpp::IntegerVector loop_id) {

  R_xlen_t num_fds = fds.size();
  int loop = loop_id[0];
  int max_fd = -1;

  thread_args *args = R_Calloc(1, thread_args);
  int *fdvals = R_Calloc(num_fds, int);
  args->fds = fdvals;
  args->num_fds = num_fds;
  R_PreserveObject(callback);
  args->callback = callback;
  args->loop = loop;

  FD_ZERO(&args->read_fds);

  if (num_fds)
    memcpy(fdvals, fds.begin(), num_fds * sizeof(int));
  for (R_xlen_t i = 0; i < num_fds; i++) {
    FD_SET(fdvals[i], &args->read_fds);
    max_fd = std::max(max_fd, fdvals[i]);
  }
  args->max_fd = max_fd;

  if (timeoutSecs[0] == R_PosInf) {
    args->timer_inf = true;
  } else if (timeoutSecs[0] > 0) {
    args->tv.tv_sec = (int) timeoutSecs[0];
    args->tv.tv_usec = ((int) (timeoutSecs[0] * 1000)) % 1000 * 1000;
  }

#ifdef _WIN32

  HANDLE hThread = CreateThread(NULL, 0, select_thread_win, (LPVOID) args, 0, NULL);

  if (hThread == NULL) {
    Rcpp::stop("thread creation error: " + std::to_string(GetLastError()));
  }

  CloseHandle(hThread);

#else

  pthread_attr_t attr;
  pthread_t t;

  if (pthread_attr_init(&attr))
    Rcpp::stop("pthread error: " + std::string(strerror(errno)));

  if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) ||
      pthread_create(&t, &attr, select_thread, (void *) args)) {
    pthread_attr_destroy(&attr);
    Rcpp::stop("pthread error: " + std::string(strerror(errno)));
  }

  if (pthread_attr_destroy(&attr)) {
    Rcpp::stop("pthread error: " + std::string(strerror(errno)));
  }

#endif

  // TODO: add cancellation

  return true;

}
