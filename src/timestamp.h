#ifndef _TIMESTAMP_H_
#define _TIMESTAMP_H_

#include <memory>

// Impl abstract class; implemented by platform-specific classes
class TimestampImpl {
public:
  virtual ~TimestampImpl() {}
  virtual bool future() const = 0;
  virtual bool less(const TimestampImpl* other) const = 0;
  virtual bool greater(const TimestampImpl* other) const = 0;
  virtual double diff_secs(const TimestampImpl* other) const = 0;
  virtual double as_double() const = 0;
};

class Timestamp {
private:
  std::shared_ptr<const TimestampImpl> p_impl;

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

  // Diff
  double diff_secs(const Timestamp& other) const {
    return p_impl->diff_secs(other.p_impl.get());
  }

  double as_double() const {
    return p_impl->as_double();
  };
};

#endif // _TIMESTAMP_H_
