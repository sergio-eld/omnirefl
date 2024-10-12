#pragma once

#include <omnirefl/refl.h>

#include <string>
#include <vector>

namespace example_impl {
struct print_field_names_t {
  template <typename T>
  void operator()(const T &t, std::vector<std::string> &out) const {
    const auto &refl = omni::reflected(t);
    out.reserve(refl.fields.size());
    for (const auto &[name, value] : refl.fields) {
      out.push_back(std::string(name));
      // todo: recursion
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
