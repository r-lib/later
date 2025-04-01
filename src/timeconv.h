#ifndef _LATER_TIMECONV_H_
#define _LATER_TIMECONV_H_

#include <sys/time.h>
// Some platforms (Win32, previously some Mac versions) use
// tinycthread.h to provide timespec. Whether tinycthread
// defines timespec or not, we want it to be consistent for
// anyone who uses these functions.
#include "tinycthread.h"

inline timespec timevalToTimespec(const timeval& tv) {
  timespec ts;
  ts.tv_sec = tv.tv_sec;
  ts.tv_nsec = tv.tv_usec * 1000;
  return ts;
}

inline timeval timespecToTimeval(const timespec& ts) {
  timeval tv;
  tv.tv_sec = ts.tv_sec;
  tv.tv_usec = (suseconds_t) (ts.tv_nsec / 1000);
  return tv;
}

inline timespec addSeconds(const timespec& time, double secs) {
  timespec ts = time;
  ts.tv_sec += (time_t)secs;
  ts.tv_nsec += (secs - (time_t)secs) * 1e9L;
  if (ts.tv_nsec < 0) {
    ts.tv_nsec += 1e9L;
    ts.tv_sec--;
  }
  if (ts.tv_nsec >= 1e9L) {
    ts.tv_nsec -= 1e9L;
    ts.tv_sec++;
  }
  return ts;
}

#endif // _LATER_TIMECONV_H_
