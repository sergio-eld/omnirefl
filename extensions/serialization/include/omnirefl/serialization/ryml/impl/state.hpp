#pragma once

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
namespace omni {
namespace ryml {

template <typename Default>
constexpr render_diagnostics_t<Default> render_diagnostics_t<Default>::_default{};

} // namespace ryml
} // namespace omni
#endif
