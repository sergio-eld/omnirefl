#pragma once

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
namespace omni {

template <typename Default>
constexpr reflected_call_t<Default> reflected_call_t<Default>::_default{};

} // namespace omni
#endif
