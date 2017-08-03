#ifdef __APPLE__

#include "timestamp.h"
#include <mach/mach_time.h>

namespace {
mach_timebase_info_data_t s_timebase;
} // namespace

class TimestampImplDarwin : public TimestampImpl {
private:
  uint64_t mach_time;
public:
  TimestampImplDarwin() {
    this->mach_time = mach_absolute_time();
  }
  
  TimestampImplDarwin(double secs) {
    double nanosecs = secs * 1e9;
    
    if (s_timebase.denom == 0) {
      mach_timebase_info(&s_timebase);
    }
    
    uint64_t now = mach_absolute_time();
    this->mach_time = now + nanosecs * s_timebase.denom / s_timebase.numer;
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

  virtual double diff_secs(const TimestampImpl* other) const {
    const TimestampImplDarwin* other_impl = reinterpret_cast<const TimestampImplDarwin*>(other);
    int64_t diff = this->mach_time - other_impl->mach_time;
    return (diff * s_timebase.numer / s_timebase.denom) / 1e9;
  }
};

Timestamp::Timestamp() : p_impl(new TimestampImplDarwin()) {}
Timestamp::Timestamp(double secs) : p_impl(new TimestampImplDarwin(secs)) {}

#endif // __APPLE__
