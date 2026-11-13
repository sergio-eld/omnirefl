#pragma once

#include <omnirefl/serialization/ryml/to_enum.hpp>

#include <algorithm>
#include <utility>

namespace omni {
namespace ryml {
namespace reflected_scope {

template <typename T>
auto to_enum_t::operator()(::ryml::ConstNodeRef node,
  T *into,
  const omni::ryml::detail::mapping_context &context) const
  -> compat::enable_if_t<omni::is_reflected_enum<T>::value,
    with_diagnostics<compat::monostate, diagnostics<false>>> {
  auto result = omni::ryml::detail::make_node_accumulator(context, node);
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

  const auto enumerators = omni::meta_for<T>::enumerators();
  const auto find_name = [&enumerators](compat::string_view name) {
    return std::find_if(enumerators.begin(),
      enumerators.end(),
      [name](const typename decltype(enumerators)::value_type &enumerator) {
        return name == compat::string_view{enumerator.second};
      });
  };

  const auto value = node.val();
  auto found = find_name({value.str, value.len});
  if (found == enumerators.end() && this->alias_by_name) {
    if (const auto canonical = this->alias_by_name({value.str, value.len}))
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

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
