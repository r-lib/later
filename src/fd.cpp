#ifdef _WIN32
#include <winsock2.h>
#endif
#include <Rcpp.h>
#include <unistd.h>
#include <stdlib.h>
#include "later.h"
#include "debug.h"

typedef struct thread_args_s {
  SEXP func;
  int *fds;
  R_xlen_t num_fds;
  fd_set read_fds;
  struct timeval tv;
  int max_fd;
  int loop;
  bool timerinf;
} thread_args;

static void later_callback(void *arg) {
  thread_args *args = (thread_args *) arg;
  SEXP results, call;
  PROTECT(results = Rf_allocVector(LGLSXP, args->num_fds));
  int *res = INTEGER(results);
  for (R_xlen_t i = 0; i < args->num_fds; i++) {
    res[i] = args->fds[i];
  }
  PROTECT(call = Rf_lcons(args->func, Rf_cons(results, R_NilValue)));
  Rf_eval(call, R_GlobalEnv);
  UNPROTECT(2);
  R_ReleaseObject(args->func);
  R_Free(args->fds);
  R_Free(args);
}

static void *select_thread(void *arg) {

  thread_args *args = (thread_args *) arg;

  int ready = select(args->max_fd + 1, &args->read_fds, NULL, NULL, args->timerinf ? NULL : &args->tv);

  if (ready < 0) {
    // TODO: errno on Windows
    err_printf("select error: %s\n", strerror(errno));
    R_ReleaseObject(args->func);
    R_Free(args->fds);
    R_Free(args);
    return NULL;
  }

  for (R_xlen_t i = 0; i < args->num_fds; i++) {
    args->fds[i] = FD_ISSET(args->fds[i], &args->read_fds) != 0;
  }

  execLaterNative2(later_callback, args, 0, args->loop);

  return NULL;

}

#ifdef _WIN32
static DWORD WINAPI select_thread_win(LPVOID lpParameter) {
  select_thread(lpParameter);
  return 1;
}
#endif

// [[Rcpp::export]]
Rcpp::LogicalVector execLater_fd(Rcpp::Function func, Rcpp::IntegerVector fds, Rcpp::NumericVector timeoutsecs, Rcpp::IntegerVector loop) {

  R_xlen_t num_fds = fds.size();
  int max_fd = -1;
  thread_args *args = R_Calloc(1, thread_args);
  args->fds = R_Calloc(num_fds, int);
  R_PreserveObject(func);
  args->func = func;
  args->loop = loop[0];

  FD_ZERO(&args->read_fds);

  for (int i = 0; i < num_fds; i++) {
    args->fds[i] = fds[i];
    FD_SET(args->fds[i], &args->read_fds);
    max_fd = std::max(max_fd, args->fds[i]);
  }
  args->max_fd = max_fd;
  args->num_fds = num_fds;

  if (timeoutsecs[0] == R_PosInf) {
    args->timerinf = true;
  } else if (timeoutsecs[0] > 0) {
    args->tv.tv_sec = (int) timeoutsecs[0];
    args->tv.tv_usec = ((int) timeoutsecs[0]) % 1 * 1^6;
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

  return true;

}
