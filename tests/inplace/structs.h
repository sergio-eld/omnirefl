#pragma once

#include <omnirefl/refl.hpp>

#include <string>
#include <vector>

namespace example_impl {
// todo: as of now it has been copied from 'tests/simple/structs.h'.
// consider having a shared header
struct print_field_names_simple_t {
  template <typename T>
  void operator()(const T &t, std::vector<std::string> &out) const {
    const auto fields = omni::reflected(t).fields;
    out.reserve(fields.size());
    for (const auto &f : fields)
      out.emplace_back(std::string(f.name));
  }
} const static print_field_names_simple{};
} // namespace example_impl

namespace example {
struct in_header_person {
  std::string name;
  int age;
};
} // namespace example
