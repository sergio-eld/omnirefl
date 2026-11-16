// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sergei Kolesnik

#pragma once

/** (QoL) Guard library headers whose interfaces require instrumentation.
 * Intended for those library headers, not direct inclusion in users' .cpp
 * files. It checks prerequisites without running the tool or generating
 * metadata.
 *
 * Takes no arguments. Expands to static_assert, reporting missing generated
 * reflection outside instrumentation runs and explaining CMake integration.
 *
```cpp
// In a library header that requires reflected metadata:
#include <omnirefl/instrumentation_guard.hpp>

OMNI_REQUIRE_INSTRUMENTATION();

#include <omnirefl/reflected_scope.hpp>
```
 * @details The compilation mode is fixed when this header is first included.
 * Generated reflection must precede the guarded library header outside an
 * instrumentation run.
 */
#if defined(OMNI_TOOL_RUN) || defined(OMNI_INCLUDED_GENERATED_REFLECTION_HEADER)
#  define OMNI_REQUIRE_INSTRUMENTATION() static_assert(true, "")
#else
#  define OMNI_REQUIRE_INSTRUMENTATION() \
    static_assert(false, \
      "Include the generated reflection header before reflection-dependent " \
      "headers. With CMake, call omni_reflected_target(<target>).")
#endif
