#ifndef _LATER_TIMECONV_H_
#define _LATER_TIMECONV_H_

// Some platforms (Win32, previously some Mac versions) use
// tinycthread.h to provide timespec. Whether tinycthread
// defines timespec or not, we want it to be consistent for
// anyone who uses these functions.
#include "tinycthread.h"

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
