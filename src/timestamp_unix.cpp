#ifndef _WIN32

#include <sys/time.h>
#include "timestamp.h"
#include "timeconv.h"

void get_current_time(timespec *ts) {
  timeval tv;
  gettimeofday(&tv, NULL);
  *ts = timevalToTimespec(tv);
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
    
    time_t wholeSecs = (long)secs;
    long nanos = (secs - wholeSecs) * 1e9;
    this->time.tv_sec += wholeSecs;
    this->time.tv_nsec += nanos;
    while (this->time.tv_nsec >= 1e9) {
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
