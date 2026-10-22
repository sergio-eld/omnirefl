// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#pragma once

#if defined(OMNI_TOOL_RUN)
#  include "exclusions.hpp"
#endif

#include <omnirefl/reflected_scope.hpp>

#include "model.hpp"

#if defined(OMNI_TOOL_RUN)
OMNI_SKIP_METADATA(dependency_exclusions::opaque_alias);
#endif
