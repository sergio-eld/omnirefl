#pragma once

#include <omnirefl/reflection.hpp>

namespace aggregate_test {

struct convert {
  template <typename From, typename To>
  To operator()(omni::binding_t<From> from, omni::meta_t<To>) const {
    return omni::refl::aggregate_into<To>(from.public_fields());
  }
};

} // namespace aggregate_test
