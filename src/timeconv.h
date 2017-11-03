#include <sys/time.h>

inline timespec timevalToTimespec(const timeval& tv) {
  timespec ts;
  ts.tv_sec = tv.tv_sec;
  ts.tv_nsec = tv.tv_usec * 1000;
  return ts;
}

inline timeval timespecToTimeval(const timespec& ts) {
  timeval tv;
  tv.tv_sec = ts.tv_sec;
  tv.tv_usec = ts.tv_nsec / 1000;
  return tv;
}
