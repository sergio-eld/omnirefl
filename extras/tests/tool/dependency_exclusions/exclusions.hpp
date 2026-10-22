// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#pragma once

#include <omnirefl/reflected_scope.hpp>

#include "model.hpp"

namespace dependency_exclusions {

// Requests register from the AST even though this function is never invoked.
inline void request_metadata_skips() {
  omni::tweaks::skip_metadata<opaque_alias>();
  // A specialization identifies the primary template for all its arguments.
  omni::tweaks::skip_metadata<view<char>>();
  omni::tweaks::skip_metadata<buffer<int, 2>>();
  omni::tweaks::skip_metadata<excluded_color>();
}

} // namespace dependency_exclusions
