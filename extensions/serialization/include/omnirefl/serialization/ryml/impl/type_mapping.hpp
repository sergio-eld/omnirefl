#pragma once

#include <c4/format.hpp>

#include <cassert>
#include <limits>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_string_like<T>,
    is_write_state<State>>::value))
auto from_string_like_t::operator()(State context,
  const T &value,
  ::ryml::NodeRef node) const
  -> void {
  const auto &text = adapt::as_string(value);
  using storage_type = decltype(adapt::as_string(value));
  using text_type = compat::remove_cvref_t<storage_type>;
  static_assert(std::is_same<char, typename text_type::value_type>::value,
    "Ryml string mapping currently supports only char text.");

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

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_write_state<State>::value))
void from_boolean_t::operator()(State,
  const T &value,
  ::ryml::NodeRef node) const {
  // C4's plain bool conversion emits 0/1 rather than document booleans.
  node.set_serialized(c4::fmt::boolalpha(value));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_integral<T>::value
    && !std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_write_state<State>::value))
void from_integral_t::operator()(State,
  const T &value,
  ::ryml::NodeRef node) const {
  node.set_serialized(value);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_floating_point<T>::value && is_write_state<State>::value))
void from_floating_point_t::operator()(State,
  const T &value,
  ::ryml::NodeRef node) const {
  // Preserve enough significant digits to recover the original value.
  node.set_serialized(c4::fmt::real(value,
    std::numeric_limits<T>::max_digits10, c4::FTOA_FLEX));
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value && is_write_state<State>::value))
auto from_arithmetic_t::operator()(State context,
  const T &value,
  ::ryml::NodeRef node) const -> void {
  const auto write = typename traits::select<
    traits::case_<std::is_same<bool, compat::remove_cv_t<T>>::value,
      from_boolean_t>,
    traits::case_<std::is_integral<T>::value, from_integral_t>,
    from_floating_point_t>::type{};

  write(std::move(context), value, node);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_optional<T>, is_read_state<State>>::value))
auto to_optional_like_t::operator()(State context,
  ::ryml::ConstNodeRef node,
  T *into) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  if (!node.is_container() && node.val_is_null()
    && (!node.has_val_tag()
      || ::ryml::TAG_NULL == ::ryml::to_tag(node.val_tag()))) {
    adapt::reset(into);
    return omni::ryml::detail::make_node_accumulator(
      std::move(context)).finish();
  }

  return core::read_node(std::move(context), node,
    omni::type_t<typename T::value_type>{})
    .transform([into](typename T::value_type &&value) -> compat::monostate {
      adapt::emplace(into, std::move(value));
      return {};
    });
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_optional<T>, is_write_state<State>>::value))
auto from_optional_like_t::operator()(State context,
  const T &value,
  ::ryml::NodeRef node) const
  -> void {
  if (adapt::has_value(value))
    core::write_node(std::move(context), adapt::get_value(value), node);
  else {
    node.set_val("null");
    node.set_val_style(::ryml::VAL_PLAIN);
  }
}

namespace reflected_scope {
namespace detail {

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename Field, typename State),
  (is_read_state<State>::value))
void map_node_to_field(omni::ryml::detail::node_accumulator<State> *result,
  ::ryml::ConstNodeRef node, Field field) {
  // The record event fold stops before invoking any later field.
  assert(!result->stopped());
  const auto mapping = mapping_to_type(omni::type_t<typename Field::type>{});
  process_node_to_field_using(result,
    mapping,
    node,
    field,
    omni::ryml::detail::node_call_priority<2>{});
}

} // namespace detail
} // namespace reflected_scope
} // namespace ryml
} // namespace omni
