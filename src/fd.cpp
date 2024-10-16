#include <Rcpp.h>
#include <vector>
#include <algorithm>
#include <sys/select.h>
#include <unistd.h>

// [[Rcpp::export]]
Rcpp::LogicalVector check_fd_ready(Rcpp::IntegerVector fds) {
  int num_fds = fds.size();
  fd_set read_fds;
  struct timeval tv;
  int max_fd = -1;

  // Initialize the file descriptor set
  FD_ZERO(&read_fds);

  // Add each file descriptor to the set and find the maximum
  for (int i = 0; i < num_fds; i++) {
    int fd = fds[i];
    FD_SET(fd, &read_fds);
    max_fd = std::max(max_fd, fd);
  }

  // Set the timeout to 0 for immediate return
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  // Use select to check which file descriptors are ready
  int ready = select(max_fd + 1, &read_fds, NULL, NULL, &tv);

  // Check for errors
  if (ready == -1) {
    Rcpp::stop("Error in select: " + std::string(strerror(errno)));
  }

  // Create the result vector
  Rcpp::LogicalVector results(num_fds);

  // Check which file descriptors are set and update the results vector
  for (int i = 0; i < num_fds; i++) {
    results[i] = FD_ISSET(fds[i], &read_fds) != 0;
  }

  return results;
}