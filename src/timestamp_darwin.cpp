#ifdef __APPLE__

#include "timestamp.h"
#include <mach/mach_time.h>

class TimestampImplDarwin : public TimestampImpl {
private:
  uint64_t mach_time;
  
public:
  TimestampImplDarwin() {
    this->mach_time = mach_absolute_time();
  }
  
  TimestampImplDarwin(double secs) {
    double nanosecs = secs * 1e9;
    
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    
    uint64_t now = mach_absolute_time();
    this->mach_time = now + nanosecs * timebase.denom / timebase.numer;
  }
  
  virtual bool future() const {
    return this->mach_time > mach_absolute_time();
  }
  
  virtual bool less(const TimestampImpl* other) const {
    return this->mach_time < reinterpret_cast<const TimestampImplDarwin*>(other)->mach_time;
  }
  
  virtual bool greater(const TimestampImpl* other) const {
    return this->mach_time > reinterpret_cast<const TimestampImplDarwin*>(other)->mach_time;
  }
};

Timestamp::Timestamp() : p_impl(new TimestampImplDarwin()) {}
Timestamp::Timestamp(double secs) : p_impl(new TimestampImplDarwin(secs)) {}

#endif // __APPLE__
