#pragma once

#include <eld/pattern_matching.hpp>
#include <omnirefl/refl.hpp>

#include <map>
#include <string>
#include <type_traits>
#include <vector>

namespace example_impl {
struct print_field_names_simple_t {
  template <typename T>
  void operator()(const T &t, std::vector<std::string> &out) const {
    const auto &fields = omni::reflected(t).fields;
    out.reserve(fields.size());
    for (const auto &f : fields)
      out.emplace_back(std::string(f.name));
  }
} const static print_field_names_simple{};

struct print_field_names_recursive_t {
  template <typename T>
  void operator()(const T &t, std::vector<std::string> &out) const {
    namespace pm = pattern_matching;

    const auto &fields = omni::reflected(t).fields;
    out.reserve(fields.size());
    for (const auto &f : fields) {
      out.emplace_back(f.name);
      f
        | pm::matched_in_place( //
          pm::m_is<std::vector>([&](const auto &v) {
            // !!! nested type in vector is expected to be a struct for this example
            using nested_type = typename std::decay_t<decltype(v)>::value_type;
            std::vector<std::string> sub;
            // ad hoc. as of this writing calling `.fields` is only possible on a reflected_binding,
            // which requires a reference.
            // todo: add similar method, like `omni::reflected_t<nested_type>::fields`
            static const nested_type ad_hoc_dummy{};
            sub.reserve(omni::reflected(ad_hoc_dummy).fields.size());
            (*this)(ad_hoc_dummy, sub);
            for (const auto &s : sub)
              out.emplace_back(std::string(f.name) + "[]." + s);
          }),
          pm::m_if<omni::is_reflected>([&](const auto &v) {
            std::vector<std::string> sub;
            sub.reserve(omni::reflected(v).fields.size());
            (*this)(v, sub);
            for (const auto &s : sub)
              out.emplace_back(std::string(f.name) + "." + s);
          }),
          pm::m_any([](const auto &) { /*no-op*/ }));
    }
  }
} const static print_field_names_recursive{};

struct print_field_values_recursive_t {
  template <typename T>
  void operator()(const T &t, std::vector<std::string> &out) const {
    namespace pm = pattern_matching;
    for (auto f : omni::reflected(t).fields) {
      f
        | pm::matched_in_place( //
          pm::m_if<std::is_fundamental>([&](const auto &v) { //
            out.emplace_back(std::string(f.name) + ": " + std::to_string(v));
          }),
          pm::m_is<std::basic_string>([&](const auto &v) { //
            out.emplace_back(std::string(f.name) + ": \"" + v + "\"");
          }),
          pm::m_is<std::vector>([&](const auto &v) {
            // !!! nested type in vector is expected to be a struct for this example
            using nested_type = typename std::decay_t<decltype(v)>::value_type;
            std::vector<std::string> sub;
            // ad hoc. as of this writing calling `.fields` is only possible on a reflected_binding,
            // which requires a reference.
            // todo: add similar method, like `omni::reflected_t<nested_type>::fields`
            static const nested_type ad_hoc_dummy{};
            sub.reserve(omni::reflected(ad_hoc_dummy).fields.size());
            for (size_t i = 0; i < v.size(); ++i) {
              (*this)(v[i], sub);
              for (const auto &s : sub) {
                out.emplace_back(std::string(f.name) + "[" + std::to_string(i) + "]." + s);
              }
              sub.clear();
            }
          }),
          pm::m_if<omni::is_reflected>([&](const auto &v) {
            std::vector<std::string> sub;
            sub.reserve(omni::reflected(v).fields.size());
            (*this)(v, sub);
            for (const auto &s : sub)
              out.emplace_back(std::string(f.name) + "." + s);
          }),
          pm::m_any([](const auto &) { /*no-op*/ }));
    }
  }
} const static print_field_values_recursive{};

// naive implementation, does not support containers
struct simple_from_map_t {
  template <typename T>
  void operator()(T &to, const std::map<std::string, std::string> &from) const noexcept {
    namespace pm = pattern_matching;
    for (auto f : omni::reflected(to).fields) {
      const auto it = from.find(std::string(f.name));
      if (it == from.cend())
        continue;
      f
        | pm::matched_in_place(
          [&](const int &, const omni::setter<int> &set_value) {
            set_value(std::stoi(it->second));
          },
          [&](const std::string &, const omni::setter<std::string> &set_value) {
            set_value(it->second);
          });
    }
  }
} const static simple_from_map{};
} // namespace example_impl

namespace example_types {
struct championship {
  std::string name;
  std::string title;
};

struct wrestler {
  std::string name;
  int age;
  std::string catchphrase{};
  std::vector<championship> titles{};

  class info {
    public:
    std::string ring_name;
    std::string signature_move;
    int debut_year;
  } info{};
  // todo: add support for an unnamed nested struct (specialization is possible via
  // `decltype(std::declval<person>().unnamed_nested)`
};

struct settable {
  std::string str;
  int i;
};
} // namespace example_types
