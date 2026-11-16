// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sergei Kolesnik

#pragma once

/** (QoL) Check reflection prerequisites without running instrumentation.
 * Takes no arguments. Expands to static_assert, reporting missing generated
 * reflection outside instrumentation runs and explaining CMake integration.
 *
```cpp
#include <omnirefl/instrumentation_guard.hpp>
OMNI_REQUIRE_INSTRUMENTATION();

// During instrumentation or generated-header compilation:
// static_assert(true, "");
```
 * @details This macro only checks the compilation mode; it does not run the
 * tool or generate metadata. Include this header after generated reflection,
 * or during instrumentation. The mode is fixed when this header is first
 * included. The check remains opt-in for headers that require reflection.
 */
#if defined(OMNI_TOOL_RUN) || defined(OMNI_INCLUDED_GENERATED_REFLECTION_HEADER)
#  define OMNI_REQUIRE_INSTRUMENTATION() static_assert(true, "")
#else
#  define OMNI_REQUIRE_INSTRUMENTATION() \
    static_assert(false, \
      "Include the generated reflection header before reflection-dependent " \
      "headers. With CMake, call omni_reflected_target(<target>).")
#endif
