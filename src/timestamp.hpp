#include <memory>

class TimestampImpl {
public:
  virtual bool future() const = 0;
  virtual bool less(const TimestampImpl* other) const = 0;
  virtual bool greater(const TimestampImpl* other) const = 0;
};

class Timestamp {
private:
  std::shared_ptr<const TimestampImpl> p_impl;
  
public:
  Timestamp();
  Timestamp(double secs);

  bool future() const {
    return p_impl->future();
  }
  bool operator<(const Timestamp& other) const {
    return p_impl->less(other.p_impl.get());
  }
  bool operator>(const Timestamp& other) const {
    return p_impl->greater(other.p_impl.get());
  }
};
