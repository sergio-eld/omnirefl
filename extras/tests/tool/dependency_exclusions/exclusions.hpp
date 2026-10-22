// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#pragma once

#include <omnirefl/reflected_scope.hpp>

#include "model.hpp"

OMNI_SKIP_METADATA(dependency_exclusions::opaque_alias);
// A specialization identifies the primary template for all its arguments.
OMNI_SKIP_METADATA(dependency_exclusions::view<char>);
OMNI_SKIP_METADATA(dependency_exclusions::buffer<int, 2>);
OMNI_SKIP_METADATA(dependency_exclusions::excluded_color);
