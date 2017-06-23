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
  
  bool has_value() const {
    return has;
  }
};
