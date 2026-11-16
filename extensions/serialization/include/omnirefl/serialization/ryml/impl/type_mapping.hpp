#pragma once

#include <c4/format.hpp>

#include <limits>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {

namespace detail {

// Pointer adapters preserve the value-producing mapper's diagnostics.
template <typename T, typename Value>
auto assign_mapped_value(T *into, with_diagnostics_t<Value, diagnostics> result)
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  if (result.value)
    *into = std::move(*result.value);

  return {
    /*value=*/result.value
      ? compat::make_optional(compat::monostate{}) : compat::nullopt,
    /*diagnostics=*/std::move(result.diagnostics),
  };
}

} // namespace detail

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_string_like<T>, is_read_state<State>>::value))
auto to_string_like_t::operator()(::ryml::ConstNodeRef node,
  omni::type_t<T>,
  State context) const
  -> with_diagnostics_t<T, diagnostics> {
  auto result = omni::ryml::detail::make_node_accumulator(context);
  if (node.is_container()) {
    result.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      json::value_kind::string,
      node,
      context.path));

    return {compat::nullopt, std::move(result.result.diagnostics)};
  }

  const auto scalar = node.val();
  using storage_type = compat::remove_cvref_t<decltype(adapt::as_string(
    std::declval<const T &>()))>;

  static_assert(std::is_same<char, typename storage_type::value_type>::value,
    "Ryml string mapping requires char text; provide as_string and "
    "assign_string conversions for non-char strings.");

  using view_type = compat::basic_string_view<typename storage_type::value_type,
    typename storage_type::traits_type>;

  T value{};
  adapt::assign_string(&value, view_type{scalar.str, scalar.len});
  return {
    /*value=*/std::move(value),
    /*diagnostics=*/std::move(result.result.diagnostics),
  };
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_string_like<T>, is_read_state<State>>::value))
auto to_string_like_t::operator()(::ryml::ConstNodeRef node,
  T *into,
  State context) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  return detail::assign_mapped_value(into,
    (*this)(node, omni::type_t<compat::remove_cv_t<T>>{}, context));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_string_like<T>,
    is_write_state<State>>::value))
auto from_string_like_t::operator()(const T &value,
  ::ryml::NodeRef node,
  State context) const
  -> void {
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

namespace detail {

// Share conversion and diagnostics while each public mapper selects its
// accepted scalar format and expected kind.
template <typename T, typename State, typename Validate>
auto read_scalar(::ryml::ConstNodeRef node, omni::type_t<T>, State context,
  json::value_kind expected, Validate validate)
  -> with_diagnostics_t<T, diagnostics> {
  auto result = omni::ryml::detail::make_node_accumulator(context);
  if (node.is_container()) {
    result.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      expected,
      node,
      context.path));

    return {compat::nullopt, std::move(result.result.diagnostics)};
  }

  const auto scalar = node.val();
  if (!validate(scalar)) {
    result.report(omni::ryml::detail::make_issue(
      json::issue_code::invalid_scalar,
      expected,
      node,
      context.path));

    return {compat::nullopt, std::move(result.result.diagnostics)};
  }

  const auto converted = omni::ryml::detail::from_chars<compat::remove_cv_t<T>>(
    scalar);

  if (!converted) {
    result.report(omni::ryml::detail::make_issue(
      converted.error(),
      expected,
      node,
      context.path));

    return {compat::nullopt, std::move(result.result.diagnostics)};
  }

  return {
    /*value=*/ *converted,
    /*diagnostics=*/std::move(result.result.diagnostics),
  };
}

} // namespace detail

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_same<bool, compat::remove_cv_t<T>>::value
    && !std::is_const<T>::value && is_read_state<State>::value))
auto to_boolean_t::operator()(::ryml::ConstNodeRef node, omni::type_t<T>,
  State context) const
  -> with_diagnostics_t<T, diagnostics> {
  return detail::read_scalar(node, omni::type_t<T>{}, context,
    json::value_kind::boolean,
    [node](c4::csubstr scalar) {
      return !node.is_val_quoted()
        && ("true" == scalar || "false" == scalar
          || "True" == scalar || "False" == scalar
          || "TRUE" == scalar || "FALSE" == scalar);
    });
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_same<bool, compat::remove_cv_t<T>>::value
    && !std::is_const<T>::value && is_read_state<State>::value))
auto to_boolean_t::operator()(::ryml::ConstNodeRef node, T *into,
  State context) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  return detail::assign_mapped_value(into,
    (*this)(node, omni::type_t<compat::remove_cv_t<T>>{}, context));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_write_state<State>::value))
void from_boolean_t::operator()(const T &value, ::ryml::NodeRef node,
  State) const {
  // C4's plain bool conversion emits 0/1 rather than document booleans.
  node.set_serialized(c4::fmt::boolalpha(value));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_integral<T>::value
    && !std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_read_state<State>::value))
auto to_integer_t::operator()(::ryml::ConstNodeRef node, omni::type_t<T>,
  State context) const
  -> with_diagnostics_t<T, diagnostics> {
  const auto expected = std::is_unsigned<T>::value
    ? json::value_kind::unsigned_integer : json::value_kind::integer;

  return detail::read_scalar(node, omni::type_t<T>{}, context, expected,
    [](c4::csubstr scalar) {
      return std::is_unsigned<T>::value
        ? scalar.is_unsigned_integer() : scalar.is_integer();
    });
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_integral<T>::value
    && !std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_read_state<State>::value))
auto to_integer_t::operator()(::ryml::ConstNodeRef node, T *into,
  State context) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  return detail::assign_mapped_value(into,
    (*this)(node, omni::type_t<compat::remove_cv_t<T>>{}, context));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_integral<T>::value
    && !std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_write_state<State>::value))
void from_integer_t::operator()(const T &value, ::ryml::NodeRef node,
  State) const {
  node.set_serialized(value);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_floating_point<T>::value && is_read_state<State>::value))
auto to_real_t::operator()(::ryml::ConstNodeRef node, omni::type_t<T>,
  State context) const
  -> with_diagnostics_t<T, diagnostics> {
  return detail::read_scalar(node, omni::type_t<T>{}, context,
    json::value_kind::number,
    [](c4::csubstr scalar) { return scalar.is_real(); });
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_floating_point<T>::value && is_read_state<State>::value))
auto to_real_t::operator()(::ryml::ConstNodeRef node, T *into,
  State context) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  return detail::assign_mapped_value(into,
    (*this)(node, omni::type_t<compat::remove_cv_t<T>>{}, context));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_floating_point<T>::value && is_write_state<State>::value))
void from_real_t::operator()(const T &value, ::ryml::NodeRef node,
  State) const {
  // Preserve enough significant digits to recover the original value.
  node.set_serialized(c4::fmt::real(value,
    std::numeric_limits<T>::max_digits10, c4::FTOA_FLEX));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value && is_read_state<State>::value))
auto to_arithmetic_t::operator()(::ryml::ConstNodeRef node, omni::type_t<T>,
  State context) const
  -> with_diagnostics_t<T, diagnostics> {
  const auto read = typename traits::select<
    traits::case_<std::is_same<bool, compat::remove_cv_t<T>>::value,
      to_boolean_t>,
    traits::case_<std::is_integral<T>::value, to_integer_t>,
    to_real_t>::type{};

  return read(node, omni::type_t<T>{}, context);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value && is_read_state<State>::value))
auto to_arithmetic_t::operator()(::ryml::ConstNodeRef node, T *into,
  State context) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  return detail::assign_mapped_value(into,
    (*this)(node, omni::type_t<compat::remove_cv_t<T>>{}, context));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value && is_write_state<State>::value))
auto from_arithmetic_t::operator()(const T &value,
  ::ryml::NodeRef node, State context) const -> void {
  const auto write = typename traits::select<
    traits::case_<std::is_same<bool, compat::remove_cv_t<T>>::value,
      from_boolean_t>,
    traits::case_<std::is_integral<T>::value, from_integer_t>,
    from_real_t>::type{};

  write(value, node, context);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_optional<T>, is_read_state<State>>::value))
auto to_optional_like_t::operator()(::ryml::ConstNodeRef node,
  T *into,
  State context) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
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

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_optional<T>, is_write_state<State>>::value))
auto from_optional_like_t::operator()(const T &value,
  ::ryml::NodeRef node,
  State context) const
  -> void {
  if (adapt::has_value(value))
    core::write_node(adapt::get_value(value), node, context);
  else {
    node.set_val("null");
    node.set_val_style(::ryml::VAL_PLAIN);
  }
}

} // namespace ryml
} // namespace omni
