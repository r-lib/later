#include <boost/shared_ptr.hpp>

// Impl abstract class; implemented by platform-specific classes
class TimestampImpl {
public:
  virtual ~TimestampImpl() {}
  virtual bool future() const = 0;
  virtual bool less(const TimestampImpl* other) const = 0;
  virtual bool greater(const TimestampImpl* other) const = 0;
};

class Timestamp {
private:
  boost::shared_ptr<const TimestampImpl> p_impl;
  
public:
  Timestamp();
  Timestamp(double secs);

  // Is this timestamp in the future?
  bool future() const {
    return p_impl->future();
  }
  
  // Comparison operators
  bool operator<(const Timestamp& other) const {
    return p_impl->less(other.p_impl.get());
  }
  bool operator>(const Timestamp& other) const {
    return p_impl->greater(other.p_impl.get());
  }
};
