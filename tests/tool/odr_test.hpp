#pragma once

#include <omnirefl/reflection.hpp> //< for in-header call

#include <sstream>
#include <string>
#include <vector>

// reflected visitor is intended to be invoked in different .cpp files, in
// different compiled object files, to ensure no odr violation occurs
namespace odr_test {

struct get_field_name_values_t {
  template <typename T>
  std::vector<std::string> operator()(omni::record_binding_t<T> value) const {
    // todo: use range for loop
    return omni::compat::apply(_collect_fields_info{}, value.public_fields());
  }

  private:
  static std::string _one(const std::string &s) {
    return s;
  }

  template <typename T>
  static std::string _one(const T &i) {
    return std::to_string(i);
  }

  template <typename T>
  static std::string _one(const std::vector<T> &v) {
    std::stringstream ss;
    ss << "[";

    for (std::size_t i = 0; i < v.size(); ++i)
      ss << (0 == i ? "" : ", ") << _one(v[i]);

    ss << "]";
    return ss.str();
  }

  // generic "lambda" for test-compatibility with C++11
  struct _collect_fields_info {
    template <typename... FieldBinding>
    std::vector<std::string> operator()(FieldBinding... f) const noexcept {
      return {(std::string(f.name()) + ": " + _one(f.value()))...};
    }
  };
} const static get_field_name_values{};

struct input {
  int m_int;
  std::string m_string;
  std::vector<std::string> m_vec;
};

template <typename T>
std::vector<std::string> in_header_call(const T &value) noexcept {
  return omni::reflected_call(get_field_name_values, value);
}

// defined in test_static_odr.cpp
std::vector<std::string> get_field_name_values_from(const input &);

} // namespace odr_test
