#ifndef _WIN32

#include <sys/time.h>
#include "timestamp.h"
#include "timeconv.h"

void get_current_time(timespec *ts) {
  // CLOCK_MONOTONIC ensures that we never get timestamps that go backward in
  // time due to clock adjustment. https://github.com/r-lib/later/issues/150
  clock_gettime(CLOCK_MONOTONIC, ts);
}

class TimestampImplPosix : public TimestampImpl {
private:
  timespec time;

public:
  TimestampImplPosix() {
    get_current_time(&this->time);
  }
  
  TimestampImplPosix(double secs) {
    get_current_time(&this->time);
    
    this->time = addSeconds(this->time, secs);
  }
  
  virtual bool future() const {
    timespec now;
    get_current_time(&now);
    return this->time.tv_sec > now.tv_sec ||
      (this->time.tv_sec == now.tv_sec && this->time.tv_nsec > now.tv_nsec);
  }
  
  virtual bool less(const TimestampImpl* other) const {
    const TimestampImplPosix* other_impl = dynamic_cast<const TimestampImplPosix*>(other);
    return this->time.tv_sec < other_impl->time.tv_sec ||
      (this->time.tv_sec == other_impl->time.tv_sec && this->time.tv_nsec < other_impl->time.tv_nsec);
  }
  
  virtual bool greater(const TimestampImpl* other) const {
    const TimestampImplPosix* other_impl = dynamic_cast<const TimestampImplPosix*>(other);
    return this->time.tv_sec > other_impl->time.tv_sec ||
      (this->time.tv_sec == other_impl->time.tv_sec && this->time.tv_nsec > other_impl->time.tv_nsec);
  }
  
  virtual double diff_secs(const TimestampImpl* other) const {
    const TimestampImplPosix* other_impl = dynamic_cast<const TimestampImplPosix*>(other);
    double sec_diff = this->time.tv_sec - other_impl->time.tv_sec;
    sec_diff += (this->time.tv_nsec - other_impl->time.tv_nsec) / 1.0e9;
    return sec_diff;
  }
};

Timestamp::Timestamp() : p_impl(new TimestampImplPosix()) {}
Timestamp::Timestamp(double secs) : p_impl(new TimestampImplPosix(secs)) {}

#endif // _WIN32
