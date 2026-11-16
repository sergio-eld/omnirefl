#pragma once

#include <algorithm>
#include <utility>

namespace omni {
namespace ryml {
namespace reflected_scope {

template <typename ValueByAlias>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_enum<T>,
    is_read_state<State>>::value))
auto to_enum_t<ValueByAlias>::operator()(::ryml::ConstNodeRef node,
  T *into,
  State context) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  auto result = omni::ryml::detail::make_node_accumulator(context);
  // RapidYAML retains scalar text. Honor !!str and quotes; otherwise reject
  // plain null, numeric, and boolean forms before looking up the name.
  bool boolean{};
  const bool wrong_kind = node.is_container()
    || (node.has_val_tag()
      ? ::ryml::TAG_STR != ::ryml::to_tag(node.val_tag())
      : !node.is_val_quoted()
        && (node.val_is_null()
          || node.val().is_number()
          || c4::from_chars(node.val(), &boolean)));

  if (wrong_kind) {
    result.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      json::value_kind::string,
      node,
      context.path));

    return std::move(result).finish();
  }

  // C++11 pair forwarding constructors need not support constant evaluation.
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#else
  const
#endif
  auto enumerators = omni::meta_for<T>::enumerators();
  using enumerator_type = typename decltype(enumerators)::value_type;
  const auto find_name = [&enumerators](compat::string_view name) {
    return std::find_if(enumerators.begin(),
      enumerators.end(),
      [name](const enumerator_type &enumerator) {
        return name == compat::string_view{enumerator.second};
      });
  };

  const compat::string_view value{node.val().str, node.val().len};
  auto found = find_name(value);
  if (found == enumerators.end()) {
    if (const auto canonical = compat::invoke(this->value_by_alias, value))
      found = find_name(*canonical);
  }

  if (found == enumerators.end()) {
    result.report(omni::ryml::detail::make_issue(
      json::issue_code::invalid_scalar,
      json::value_kind::string,
      node,
      context.path));

    return std::move(result).finish();
  }

  *into = found->first;
  return std::move(result).finish();
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_enum<T>,
    is_write_state<State>>::value))
auto from_enum_t::operator()(T value, ::ryml::NodeRef node, State) const -> void {
  // C++11 pair forwarding constructors need not support constant evaluation.
#if 201402L <= OMNI_CPLUSPLUS
  constexpr
#else
  const
#endif
  auto enumerators = omni::meta_for<T>::enumerators();
  using enumerator_type = typename decltype(enumerators)::value_type;
  const auto found = std::find_if(enumerators.begin(),
    enumerators.end(),
    [value](const enumerator_type &enumerator) {
      return value == enumerator.first;
    });

  if (found != enumerators.end()) {
    node.set_serialized(c4::to_csubstr(found->second));
    node.set_val_style(::ryml::VAL_DQUO);
  }
}

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
