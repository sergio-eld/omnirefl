// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#pragma once

#include <omnirefl/reflected_scope.hpp>

#include "model.hpp"

extern template void
  omni::tweaks::skip_metadata<dependency_exclusions::opaque_alias>();
// A specialization identifies the primary template for all its arguments.
extern template void
  omni::tweaks::skip_metadata<dependency_exclusions::view<char>>();
extern template void
  omni::tweaks::skip_metadata<dependency_exclusions::buffer<int, 2>>();
extern template void
  omni::tweaks::skip_metadata<dependency_exclusions::excluded_color>();
