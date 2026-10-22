// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#pragma once

#include <omnirefl/reflected_scope.hpp>

#if defined(OMNI_TOOL_RUN)
#  include "exclusions.hpp"
#endif

#include "model.hpp"

#if defined(OMNI_TOOL_RUN)
namespace exclusion_scope_first {
inline void request_metadata_skip() {
  omni::tweaks::skip_metadata<dependency_exclusions::opaque>();
}
} // namespace exclusion_scope_first
#endif
