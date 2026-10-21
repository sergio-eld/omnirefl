// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#pragma once

#include <omnirefl/reflected_scope.hpp>

#include "model.hpp"

namespace dependency_exclusions {

// A specialization identifies the primary template for all its arguments.
omni::exclude_dependencies<opaque_alias, view<char>, buffer<int, 2>,
  excluded_color> exclusions;

} // namespace dependency_exclusions
