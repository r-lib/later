#ifdef __unix__
#ifndef __APPLE__ 

#include "timestamp.h"
#include <time.h>

class TimestampImplUnix : public TimestampImpl {
private:
  timespec time;

public:
  TimestampImplUnix() {
    clock_gettime(CLOCK_MONOTONIC, &this->time);
  }
  
  TimestampImplUnix(double secs) {
    clock_gettime(CLOCK_MONOTONIC, &this->time);
    time_t wholeSecs = (long)secs;
    long nanos = (secs - wholeSecs) * 1e9;
    this->time.tv_sec += wholeSecs;
    this->time.tv_nsec += nanos;
    while (this->time.tv_nsec > 1e9) {
      this->time.tv_sec++;
      this->time.tv_nsec -= 1e9;
    }
    while (this->time.tv_nsec < 0) {
      this->time.tv_sec--;
      this->time.tv_nsec += 1e9;
    }
  }
  
  virtual bool future() const {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return this->time.tv_sec > now.tv_sec ||
      (this->time.tv_sec == now.tv_sec && this->time.tv_nsec > now.tv_nsec);
  }
  
  virtual bool less(const TimestampImpl* other) const {
    const TimestampImplUnix* other_impl = dynamic_cast<const TimestampImplUnix*>(other);
    return this->time.tv_sec < other_impl->time.tv_sec ||
      (this->time.tv_sec == other_impl->time.tv_sec && this->time.tv_nsec < other_impl->time.tv_nsec);
  }
  
  virtual bool greater(const TimestampImpl* other) const {
    const TimestampImplUnix* other_impl = dynamic_cast<const TimestampImplUnix*>(other);
    return this->time.tv_sec > other_impl->time.tv_sec ||
      (this->time.tv_sec == other_impl->time.tv_sec && this->time.tv_nsec > other_impl->time.tv_nsec);
  }
  
  virtual double diff_secs(const TimestampImpl* other) const {
    const TimestampImplUnix* other_impl = dynamic_cast<const TimestampImplUnix*>(other);
    return difftime(this->time, other_impl->time);
  }
};

Timestamp::Timestamp() : p_impl(new TimestampImplUnix()) {}
Timestamp::Timestamp(double secs) : p_impl(new TimestampImplUnix(secs)) {}

#endif // __APPLE__
#endif // __unix__
