#pragma once

#include <cpp_pm/pattern_matching.hpp>
#include <omnirefl/refl.h>

#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace example_impl {
struct print_field_names_t {
  template <typename T>
  void operator()(const T &t, std::vector<std::string> &out) const {
    namespace pm = pattern_matching;
    for (auto f : omni::reflected(t).fields) {
      const omni::string_view name = f.name;
      f
        | pm::matched_in_place( //
          pm::m_if<std::is_fundamental>([&](const auto &v) { //
            out.emplace_back(std::string(name) + ": " + std::to_string(v));
          }),
          pm::m_is<std::basic_string>([&](const auto &s) { //
            out.emplace_back(std::string(name) + ": \"" + s + "\"");
          }),
          pm::m_is<std::vector>([&](const auto &v) { //
            std::vector<std::string> sub;
            sub.reserve(v.size());
            for (const auto &i : v)
              (*this)(i, sub);

            std::stringstream ss;
            ss << '[';
            if (!sub.empty())
              ss << sub.front();
            for (size_t i = 1; i < sub.size(); ++i)
              ss << ",\n  " << sub[i];
            ss << ']';
            out.emplace_back(std::string(name) + ": " + std::move(ss).str());
          }),
          pm::m_if<omni::is_reflected>([&](const auto &v) { //
            std::vector<std::string> sub;
            sub.reserve(omni::reflected(v).fields.size());
            (*this)(v, sub);

            std::stringstream ss;
            ss << '{';
            if (!sub.empty())
              ss << sub.front();
            for (size_t i = 1; i < sub.size(); ++i)
              ss << ",\n  " << sub[i];
            ss << '}';
            out.emplace_back(std::string(name) + ": " + std::move(ss).str());
          })
          // , pm::m_any([](const auto &) {})
        );
    }
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
