#ifndef UTILS_H
#define UTILS_H

template <typename T>
std::string toString(T x) {
  std::stringstream ss;
  ss << x;
  return ss.str();
}

#endif
