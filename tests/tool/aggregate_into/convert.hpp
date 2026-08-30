#pragma once

#include <omnirefl/reflection.hpp>

namespace aggregate_test {

// C++11 compatibility for the generic conversion lambda available from C++14.
// Negative translation units reuse the named callable from this header.
struct convert {
  template <typename From, typename To>
  To operator()(omni::binding_t<From> from, omni::meta_t<To>) const {
    return omni::refl::aggregate_into<To>(from.public_fields());
  }
};

} // namespace aggregate_test
