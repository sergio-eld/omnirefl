#pragma once

#include <omnirefl/refl.h>

#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace impl {
template <typename>
struct is_vector: std::false_type {};
template <typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>>: std::true_type {};
} // namespace impl
// todo: implementation for std::apply before C++17

namespace example_impl {
struct print_field_names_t {
  template <typename T>
  void operator()(std::string_view name, const T &val, std::vector<std::string> &out) const {
    std::stringstream ss;
    ss << name << ": ";
    if constexpr (impl::is_vector<T>()) {
      std::vector<std::string> sub;
      sub.reserve(val.size());
      for (const auto &v : val)
        (*this)(v, sub);
      ss << "[";
      for (const auto &s : sub)
        ss << s << ",\n";
      ss << "]";
    } else {
      ss << val;
    }
    out.emplace_back(ss.str());
  }

  template <typename T>
  void operator()(const T &t, std::vector<std::string> &out) const {
    // todo: use at least C++14-friendly code
    if constexpr (omni::is_reflected<T>()) {
      using reflected_t = omni::reflected_t<T>;
      std::apply(
        [&](const auto &...field) { ((*this)(field.name(), field.get_value(t), out), ...); },
        typename reflected_t::fields_t{});
    }

    // todo: design a convenient interface for reflection
    // const auto &refl = omni::reflected(t);
    // out.reserve(refl.fields.size());
    // for (const auto &[name, value] : refl.fields) {
    //   out.push_back(std::string(name));
    //   // todo: recursion
    //   // todo: matching
    //   // value.match_if<omni::is_reflected>([&](const auto &) { out.back() += "(reflected)"; });
    // }
  }
} const inline print_field_names{};
} // namespace example_impl

namespace example_types {
struct championship {
  std::string name;
  std::string title;
};

struct person {
  std::string name;
  int age;
  std::string catchphrase{};
  std::vector<championship> titles{};

  // todo: nested struct
};
} // namespace example_types
