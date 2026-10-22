// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#pragma once

#include <omnirefl/reflected_scope.hpp>

#if defined(OMNI_TOOL_RUN)
#  include "exclusions.hpp"
#endif

#include "model.hpp"

#if defined(OMNI_TOOL_RUN)
OMNI_SKIP_METADATA(dependency_exclusions::opaque);
#endif
