#ifdef _WIN32
#include <winsock2.h>
#else
#include <poll.h>
#endif
#include <Rcpp.h>
#include <unistd.h>
#include <cstdlib>
#include "later.h"
#include "callback_registry_table.h"

extern CallbackRegistryTable callbackRegistryTable;

typedef struct ThreadArgs_s {
  SEXP callback;
  double timeout;
  std::unique_ptr<std::vector<int>> fds;
  int num_fds;
  int loop;
} ThreadArgs;

static void later_callback(void *arg) {

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(static_cast<std::shared_ptr<ThreadArgs>*>(arg));
  std::shared_ptr<ThreadArgs> args = *argsptr;
  Rf_eval(args->callback, R_GlobalEnv);
  R_ReleaseObject(args->callback);

}

// CONSIDER: add interface to allow monitoring of different event types.
// CONSIDER: add method for HANDLES on Windows. Assuming we only accept integer
// values for both, we could use heuristics: check if it is a valid SOCKET or
// else assume HANDLE - but this is not fool-proof.
// Otherwise would require an interface for user to specify type.
static void *select_thread(void *arg) {

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(static_cast<std::shared_ptr<ThreadArgs>*>(arg));
  std::shared_ptr<ThreadArgs> args = *argsptr;

  int timeout = args->timeout == R_PosInf ? -1 : args->timeout < 0 ? 1000 : (int) (args->timeout * 1000);
  int result;


  std::vector<struct pollfd> pollfds;
  for (int i = 0; i < args->num_fds; i++) {
    struct pollfd pfd;
    pfd.fd = (*args->fds)[i];
    pfd.events = POLLIN;
    pfd.revents = 0;
    pollfds.push_back(pfd);
  }
#ifdef _WIN32
  result = WSAPoll(pollfds.data(), args->num_fds, timeout);
#else
  result = poll(pollfds.data(), args->num_fds, timeout);
#endif

  int *values = (int *) DATAPTR_RO(CADR(args->callback));

  if (result) {
    for (int i = 0; i < args->num_fds; i++) {
      values[i] = pollfds[i].revents & POLLIN ? 1 : pollfds[i].revents & POLLNVAL ? R_NaInt : 0;
    }
  } else {
    for (int i = 0; i < args->num_fds; i++) {
      values[i] = 0;
    }
  }

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
Rcpp::LogicalVector execLater_fd(Rcpp::Function callback, Rcpp::IntegerVector fds, Rcpp::NumericVector timeoutSecs, Rcpp::IntegerVector loop_id) {

  int num_fds = (int) fds.size();
  double timeout = timeoutSecs[0];
  int loop = loop_id[0];

  SEXP call = Rf_lcons(callback, Rf_cons(Rf_allocVector(LGLSXP, num_fds), R_NilValue));
  R_PreserveObject(call);

  std::shared_ptr<ThreadArgs> args = std::make_shared<ThreadArgs>();
  std::unique_ptr<std::vector<int>> fdvals(new std::vector<int>(num_fds));
  args->num_fds = num_fds;
  args->timeout = timeout;
  args->loop = loop;
  args->callback = call;

  if (num_fds)
    std::memcpy(fdvals->data(), fds.begin(), num_fds * sizeof(int));
  args->fds = std::move(fdvals);

  std::unique_ptr<std::shared_ptr<ThreadArgs>> argsptr(new std::shared_ptr<ThreadArgs>(args));

#ifdef _WIN32

  HANDLE hThread = CreateThread(NULL, 0, select_thread_win, static_cast<LPVOID>(argsptr.release()), 0, NULL);

  if (hThread == NULL) {
    Rcpp::stop("thread creation error: " + std::to_string(GetLastError()));
  }

  CloseHandle(hThread);

#else

  pthread_attr_t attr;
  pthread_t thr;

  // TODO: actually allocate global pthread_attr and cache for re-use
  if (pthread_attr_init(&attr))
    Rcpp::stop("thread creation error: " + std::string(strerror(errno)));

  if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) ||
      pthread_create(&thr, &attr, select_thread, static_cast<void *>(argsptr.release()))) {
    pthread_attr_destroy(&attr);
    Rcpp::stop("thread creation error: " + std::string(strerror(errno)));
  }

  if (pthread_attr_destroy(&attr))
    Rcpp::stop("thread creation error: " + std::string(strerror(errno)));

#endif

  // TODO: Add cancellation: clean way is to insert something that is also
  // waited on that we can signal to return. But due to the setup overhead
  // perhaps have this be optional. Otherwise can retain a reference to the
  // thead to forcefully terminate it - but am wary of potential edge cases.

  return true;

}
