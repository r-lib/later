#ifdef _WIN32

#include "timestamp.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class TimestampImplWin32 : public TimestampImpl {
private:
  LARGE_INTEGER performanceCount;

public:
  TimestampImplWin32() {
    QueryPerformanceCounter(&this->performanceCount);
  }
  
  TimestampImplWin32(double secs) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&this->performanceCount);
    this->performanceCount.QuadPart += static_cast<LONGLONG>(secs * (double)freq.QuadPart);
  }
  
  virtual bool future() const {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return this->performanceCount.QuadPart > now.QuadPart;
  }
  
  virtual bool less(const TimestampImpl* other) const {
    const TimestampImplWin32* other_impl = dynamic_cast<const TimestampImplWin32*>(other);
    return this->performanceCount.QuadPart < other_impl->performanceCount.QuadPart;
  }
  
  virtual bool greater(const TimestampImpl* other) const {
    const TimestampImplWin32* other_impl = dynamic_cast<const TimestampImplWin32*>(other);
    return this->performanceCount.QuadPart > other_impl->performanceCount.QuadPart;
  }
};

Timestamp::Timestamp() : p_impl(new TimestampImplWin32()) {}
Timestamp::Timestamp(double secs) : p_impl(new TimestampImplWin32(secs)) {}

#endif // WIN32
