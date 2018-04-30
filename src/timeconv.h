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

inline timespec addSeconds(const timespec& time, double secs) {
  timespec ts = time;
  ts.tv_sec += (time_t)secs;
  ts.tv_nsec += (secs - (time_t)secs) * 1e9;
  if (ts.tv_nsec < 0) {
    ts.tv_nsec += 1e9;
    ts.tv_sec--;
  }
  if (ts.tv_nsec >= 1e9) {
    ts.tv_nsec -= 1e9;
    ts.tv_sec++;
  }
  return ts;
}
