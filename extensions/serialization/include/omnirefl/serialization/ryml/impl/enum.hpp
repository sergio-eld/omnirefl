#pragma once

#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>
#include <omnirefl/serialization/ryml/with_diagnostics.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace core {
namespace reflected_scope {

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename Policy),
  (omni::is_reflected_enum<T>::value
    && mapping::is_enum_policy<Policy>::value))
auto parse_as_enum(::ryml::ConstNodeRef node, const Policy &policy)
  -> compat::expected<T, issue> {
  assert(core::expect_string_node(node));

  constexpr auto enumerators = omni::meta_for<T>::enumerators();
#if !defined(NDEBUG)
  std::array<compat::string_view, enumerators.size()> names{};
  for (std::size_t index = 0; index < enumerators.size(); ++index)
    names[index] = enumerators[index].second;
  assert(mapping::valid_aliases(names, policy.aliases_by_name));
#endif
  const auto find_name = [enumerators](compat::string_view name)
    -> compat::optional<T> {
    for (const auto &enumerator : enumerators)
      if (name == compat::string_view{enumerator.second})
        return enumerator.first;

    return compat::nullopt;
  };

  const compat::string_view value{node.val().str, node.val().len};
  if (const auto found = find_name(value))
    return *found;

  if (const auto canonical = mapping::resolve_alias(
      policy.aliases_by_name, value))
    if (const auto found = find_name(*canonical))
      return *found;

  return compat::unexpected<issue>{core::make_issue(
    json::issue_code::invalid_scalar, json::value_kind::string, node)};
}

} // namespace reflected_scope
} // namespace core

namespace reflected_scope {

template <typename Policy, typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (omni::is_reflected_enum<T>::value && is_read_state<State>::value))
auto map_node_to_enum_t<Policy, WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>) const -> with_diagnostics_t<T> {
  if (state.limit_exceeded())
    return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      state.config.error_policy, {}}, {}, {}}};

  if (this->with_node_check) {
    auto checked = core::expect_string_node(node);
    if (!checked)
      return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
        /*base=*/{},
#endif
        state.config.error_policy, {}}, {std::move(checked.error())}, {}}};
  }

  return omni::ryml::detail::node_result(std::move(state),
    core::reflected_scope::parse_as_enum<T>(node, this->policy), node);
}

template <typename Policy, typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (omni::is_reflected_enum<T>::value
    && std::is_assignable<T &, T &&>::value
    && is_read_state<State>::value))
auto map_node_to_enum_t<Policy, WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node, T *into) const
  -> with_diagnostics_t<compat::monostate> {
  assert(into);
  return (*this)(std::move(state), node, omni::type_t<T>{})
    .transform([into](T value) -> compat::monostate {
      *into = value;
      return {};
    });
}

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
