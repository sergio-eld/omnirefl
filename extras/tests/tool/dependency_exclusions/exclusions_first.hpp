// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#pragma once

#if defined(OMNI_TOOL_RUN)
#  include "exclusions.hpp"
#endif

#include <omnirefl/reflected_scope.hpp>

#include "model.hpp"

#if defined(OMNI_TOOL_RUN)
namespace exclusion_requests_first {
inline void request_metadata_skip() {
  omni::tweaks::skip_metadata<dependency_exclusions::opaque_alias>();
}
} // namespace exclusion_requests_first
#endif
