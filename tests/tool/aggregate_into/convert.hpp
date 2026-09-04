#pragma once

#include <omnirefl/reflection.hpp>

namespace aggregate_test {

// C++11 compatibility for the generic conversion lambda available from C++14.
// Negative translation units reuse the named callable from this header.
struct convert {
  template <typename From, typename _M>
  typename omni::record_meta_t<_M>::reflected_type operator()(
    omni::record_binding_t<From> from,
    omni::record_meta_t<_M>) const {
    using To = typename omni::record_meta_t<_M>::reflected_type;

    return omni::refl::aggregate_into<To>(from.public_fields());
  }
};

} // namespace aggregate_test
