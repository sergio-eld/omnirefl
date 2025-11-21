#pragma once

#include <string>

#include <omnirefl/reflected_scope.hpp>

namespace interface_test {

struct tagged_type_t {
  int first;
  std::string second;
};

enum class enum_type_t {
  zero,
  one,
};

namespace type_identity {

// tagged: omni::reflected_tagged_t<T>
struct tagged_type_name_reflected_tagged_t_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_tagged_t<T>::name();
  }
} const static tagged_type_name_reflected_tagged_t{};

// tagged: omni::reflected_tagged<T>()
struct tagged_type_name_reflected_tagged_fn_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_tagged<T>().name();
  }
} const static tagged_type_name_reflected_tagged_fn{};

// tagged: omni::reflected_tagged(t)
struct tagged_type_name_reflected_tagged_lv_t {
  template <typename T>
  std::string operator()(T &&t) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_tagged(std::forward<T>(t)).name();
  }
} const static tagged_type_name_reflected_tagged_lv{};

// tagged: omni::reflected_t<T>
struct tagged_type_name_reflected_t_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_t<T>::name();
  }
} const static tagged_type_name_reflected_t{};

// tagged: omni::reflected<T>()
struct tagged_type_name_reflected_fn_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected<T>().name();
  }
} const static tagged_type_name_reflected_fn{};

// tagged: omni::reflected(t)
struct tagged_type_name_reflected_lv_t {
  template <typename T>
  std::string operator()(T &&t) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected(std::forward<T>(t)).name();
  }
} const static tagged_type_name_reflected_lv{};

// enum: omni::reflected_enum_t<T>
struct enum_type_name_reflected_enum_t_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_enum_t<T>::name();
  }
} const static enum_type_name_reflected_enum_t{};

// enum: omni::reflected_enum<T>()
struct enum_type_name_reflected_enum_fn_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_enum<T>().name();
  }
} const static enum_type_name_reflected_enum_fn{};

// enum: omni::reflected_enum(e)
struct enum_type_name_reflected_enum_lv_t {
  template <typename T>
  std::string operator()(T &&e) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_enum(std::forward<T>(e)).name();
  }
} const static enum_type_name_reflected_enum_lv{};

// enum: omni::reflected_t<T>
struct enum_type_name_reflected_t_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected_t<T>::name();
  }
} const static enum_type_name_reflected_t{};

// enum: omni::reflected<T>()
struct enum_type_name_reflected_fn_t {
  template <typename T>
  std::string operator()(const T &) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected<T>().name();
  }
} const static enum_type_name_reflected_fn{};

// enum: omni::reflected(e)
struct enum_type_name_reflected_lv_t {
  template <typename T>
  std::string operator()(T &&e) const {
    if (!omni::is_reflected<T>::value)
      return "not reflected";
    return omni::reflected(std::forward<T>(e)).name();
  }
} const static enum_type_name_reflected_lv{};

} // namespace type_identity
} // namespace interface_test
