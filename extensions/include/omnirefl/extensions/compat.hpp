#pragma once

#include <omnirefl/compat.hpp>

#if defined(__has_include)
#  if __has_include(<version>)
#    include <version>
#  endif
#endif

#if defined(__cpp_lib_optional) && 201606L <= __cpp_lib_optional
#  include <optional>
#else
#  include <tl/optional.hpp>
#endif

// The extensions use expected's monadic operations, not just value storage.
#if defined(__cpp_lib_expected) && 202211L <= __cpp_lib_expected
#  include <expected>
#else
#  include <tl/expected.hpp>
#endif

namespace omni {
namespace compat {

#if defined(__cpp_lib_optional) && 201606L <= __cpp_lib_optional
using std::optional;
using std::nullopt;
using std::make_optional;
#else
using tl::optional;
using tl::nullopt;
using tl::make_optional;
#endif

#if defined(__cpp_lib_expected) && 202211L <= __cpp_lib_expected
using std::expected;
using std::unexpected;
#else
using tl::expected;
using tl::unexpected;
#endif

} // namespace compat
} // namespace omni
