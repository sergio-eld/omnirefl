#pragma once

#include <omnirefl/serialization/ryml/mapping.hpp>

#include <c4/format.hpp>

#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {

template <typename T, typename Config>
auto from_string_like::operator()(const T &value,
  ::ryml::NodeRef node,
  mapping_state<Config> context) const
  -> compat::enable_if_t<supports<T>()> {
  const auto &text = adapt::as_string(value);
  using storage_type = decltype(adapt::as_string(value));
  using text_type = compat::remove_cvref_t<storage_type>;
  static_assert(std::is_same<char, typename text_type::value_type>::value,
    "Ryml string mapping requires char text; provide as_string and "
    "assign_string conversions for non-char strings.");

  using view_type = compat::basic_string_view<typename text_type::value_type,
    typename text_type::traits_type>;

  // Owning conversions are temporary: copy them even in a reference tree.
  constexpr bool borrows_source = std::is_lvalue_reference<storage_type>::value
    || std::is_same<text_type, view_type>::value;

  const c4::csubstr scalar{text.data(), text.size()};
  if (context.borrow_strings && borrows_source)
    node.set_val(scalar);
  else
    node.set_serialized(scalar);

  node.set_val_style(::ryml::VAL_DQUO);
}

template <typename T, typename Config>
auto to_string_like::operator()(::ryml::ConstNodeRef node,
  T *into,
  mapping_state<Config> context) const
  -> compat::enable_if_t<supports<T>(),
    with_diagnostics<compat::monostate, diagnostics>> {
  auto result = omni::ryml::detail::make_node_accumulator(context);
  if (node.is_container()) {
    result.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      json::value_kind::string,
      node,
      context.path));

    return std::move(result).finish();
  }

  const auto scalar = node.val();
  using storage_type = compat::remove_cvref_t<decltype(adapt::as_string(
    std::declval<const T &>()))>;

  static_assert(std::is_same<char, typename storage_type::value_type>::value,
    "Ryml string mapping requires char text; provide as_string and "
    "assign_string conversions for non-char strings.");

  using view_type = compat::basic_string_view<typename storage_type::value_type,
    typename storage_type::traits_type>;

  adapt::assign_string(into, view_type{scalar.str, scalar.len});
  return std::move(result).finish();
}

template <typename T, typename Config>
auto from_arithmetic::operator()(const T &value,
  ::ryml::NodeRef node,
  mapping_state<Config>) const
  -> compat::enable_if_t<std::is_arithmetic<T>::value> {
  const auto write = typename traits::select<
    traits::case_<std::is_same<bool, T>::value,
      omni::ryml::detail::write_boolean>,
    traits::case_<std::is_floating_point<T>::value,
      omni::ryml::detail::write_real>,
    omni::ryml::detail::write_integer>::type{};

  write(value, &node);
}

template <typename T, typename Config>
auto to_arithmetic::operator()(::ryml::ConstNodeRef node,
  T *into,
  mapping_state<Config> context) const
  -> compat::enable_if_t<supports<T>(),
    with_diagnostics<compat::monostate, diagnostics>> {
  auto result = omni::ryml::detail::make_node_accumulator(context);
  const auto expected = std::is_same<bool, T>::value
    ? json::value_kind::boolean
    : std::is_unsigned<T>::value
      ? json::value_kind::unsigned_integer
      : std::is_integral<T>::value
        ? json::value_kind::integer
        : json::value_kind::number;

  if (node.is_container()) {
    result.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      expected,
      node,
      context.path));

    return std::move(result).finish();
  }

  const auto scalar = node.val();
  const bool valid = std::is_same<bool, T>::value
    ? !node.is_val_quoted()
      && ("true" == scalar
        || "false" == scalar
        || "True" == scalar
        || "False" == scalar
        || "TRUE" == scalar
        || "FALSE" == scalar)
    : std::is_unsigned<T>::value
      ? scalar.is_unsigned_integer()
      : std::is_integral<T>::value
        ? scalar.is_integer()
        : scalar.is_real();

  if (!valid) {
    result.report(omni::ryml::detail::make_issue(
      json::issue_code::invalid_scalar,
      expected,
      node,
      context.path));

    return std::move(result).finish();
  }

  const auto converted = omni::ryml::detail::from_chars<T>(scalar);
  if (!converted) {
    result.report(omni::ryml::detail::make_issue(
      converted.error(),
      expected,
      node,
      context.path));

    return std::move(result).finish();
  }

  *into = *converted;
  return std::move(result).finish();
}

template <typename T, typename Config>
auto from_optional_like::operator()(const T &value,
  ::ryml::NodeRef node,
  mapping_state<Config> context) const
  -> compat::enable_if_t<traits::is_optional<T>::value> {
  if (adapt::has_value(value)) {
    core::write_node(adapt::get_value(value), node, context);
  } else {
    node.set_val("null");
    node.set_val_style(::ryml::VAL_PLAIN);
  }
}

template <typename T, typename Config>
auto to_optional_like::operator()(::ryml::ConstNodeRef node,
  T *into,
  mapping_state<Config> context) const
  -> compat::enable_if_t<traits::is_optional<T>::value,
    with_diagnostics<compat::monostate, diagnostics>> {
  if (!node.is_container() && node.val_is_null()
    && (!node.has_val_tag()
      || ::ryml::TAG_NULL == ::ryml::to_tag(node.val_tag()))) {
    adapt::reset(into);
    return omni::ryml::detail::make_node_accumulator(context).finish();
  }

  typename T::value_type value{};
  auto result = core::read_node(node, &value, context);
  if (result.value)
    adapt::emplace(into, std::move(value));

  return result;
}

} // namespace ryml
} // namespace omni
