#ifndef _OPTIONAL_H_
#define _OPTIONAL_H_

template<class T>
class Optional {
  bool has;
  T value;

public:
  Optional() : has(false), value() {
  }

  Optional(const T& val) : has(true), value(val) {
  }

  const T& operator*() const {
    return this->value;
  }
  T& operator*() {
    return this->value;
  }
  T* operator->() {
    return &this->value;
  }
  void operator=(const T& value) {
    this->value = value;
    this->has = true;
  }

  bool has_value() const {
    return has;
  }

  void reset() {
    // Creating a new object may be problematic or expensive for some classes;
    // however, for the types we use in later, this is OK. If Optional is used
    // for more types in the future, we could switch to a different
    // implementation of optional.
    this->value = T();
    this->has= false;
  }
};

#endif // _OPTIONAL_H_
