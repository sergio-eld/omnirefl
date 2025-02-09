#pragma once

// As of this writing, this header should not be publicly exposed by a reflected target
// that is being linked by a non-reflected one.
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

} // namespace odr
