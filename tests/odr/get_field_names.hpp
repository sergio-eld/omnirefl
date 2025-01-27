#pragma once

#include <omnirefl/refl.hpp>

#include <string>
#include <vector>

namespace odr {
struct print_field_names_t {
  template <typename T>
  void operator()(const T &t, std::vector<std::string> &result) const noexcept {
    for (auto f : omni::reflected(t).fields)
      result.emplace_back(f.name);
  }
} const static print_field_names{};

struct dummy {
  int oceanic;
};

std::vector<std::string> get_field_names_a(const dummy &);
std::vector<std::string> get_field_names_b(const dummy &);
} // namespace odr
