#pragma once

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
namespace omni {
namespace fn {

template <typename Default>
constexpr branch_t<Default> branch_t<Default>::_default{};

template <typename Default>
constexpr cond_t<Default> cond_t<Default>::_default{};

template <template <typename> class Trait>
constexpr predicate_t<Trait> predicate_t<Trait>::_default{};

} // namespace fn
} // namespace omni
#endif
