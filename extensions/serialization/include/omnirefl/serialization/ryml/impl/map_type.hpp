#pragma once

#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>
#include <omnirefl/serialization/ryml/with_diagnostics.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <c4/format.hpp>

#include <cassert>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace core {

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (is_write_state<State>::value))
auto write_node(State context, const T &value) -> ::ryml::NodeRef {
  const auto mapping =
    compat::invoke(context.map_node, context, omni::type_t<T>{});
  return mapping(std::move(context), value);
}

} // namespace core

namespace detail {

template <typename State>
struct cpp11_write_member {
  ::ryml::NodeRef &node;
  State context;

  template <typename Record, typename _F>
  void operator()(omni::data_member_binding_t<Record, _F> field) const {
    auto child = this->node.append_child();
    child.set_key_serialized(field.name());
    core::write_node(
      this->context.at(child).template field<compat::remove_cvref_t<Record>>(
        omni::data_member_meta_t<_F>{}),
      field.value());
  }
};

} // namespace detail

namespace reflected_scope {

template <typename Strategy>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_record<T>,
    is_write_state<State>>::value))
auto map_record_t<Strategy>::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  auto node = context.node();
  assert(mapping::valid_record_strategy(
    compat::apply(detail::record_field_names{},
      omni::meta_for<T>::accessible_data_members()), this->strategy));
  node.set_map();
  const auto binding = omni::meta_for<T>::bind(value);
  binding.accessible_data_members()
    | omni::fn::filter(this->strategy.filter_fields)
    | omni::fn::each(omni::ryml::detail::cpp11_write_member<State>{
      /*node=*/node,
      /*context=*/std::move(context),
    });
  return node;
}

template <typename Strategy>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_enum<T>,
    is_write_state<State>>::value))
auto map_enum_t<Strategy>::operator()(State context, T value) const
  -> ::ryml::NodeRef {
  auto node = context.node();
  constexpr auto enumerators = omni::meta_for<T>::enumerators();
  for (const auto &enumerator : enumerators)
    if (value == enumerator.first) {
      node.set_serialized(c4::to_csubstr(enumerator.second));
      node.set_val_style(::ryml::VAL_DQUO);
      return node;
    }
  return node;
}

} // namespace reflected_scope

namespace detail {

template <typename T>
bool is_null(const T &value);

template <bool Optional>
struct is_null_case {
  template <typename T>
  static bool get(const T &) { return false; }
};

template <>
struct is_null_case<true> {
  template <typename T>
  static bool get(const T &value) {
    return !adapt::has_value(value) || is_null(adapt::get_value(value));
  }
};

template <typename T>
bool is_null(const T &value) {
  return is_null_case<traits::is_optional<T>::value>::get(value);
}

template <bool KeyValue>
struct write_range_case;

template <>
struct write_range_case<false> {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_write_state<State>::value))
  static void apply(State context, const T &value, ::ryml::NodeRef node) {
    using element_type = compat::range_value_t<T>;
    node.set_seq();
    // Separate iterator/sentinel declarations also support non-common ranges
    // before C++17 permitted them in range-based for loops.
    using std::begin;
    using std::end;
    auto current = begin(value);
    const auto last = end(value);
    for (; current != last; ++current) {
      const element_type &element = *current;
      if (!is_null(element)) {
        core::write_node(context.at(node.append_child()).element(), element);
      }
    }
  }
};

template <>
struct write_range_case<true> {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_write_state<State>::value))
  static void apply(State context, const T &value, ::ryml::NodeRef node) {
    static_assert(traits::is_string_like<traits::key_type_t<T>>::value,
      "map_range requires string-adaptable keys for JSON/YAML objects");

    node.set_map();
    using std::begin;
    using std::end;
    auto current = begin(value);
    const auto last = end(value);
    for (; current != last; ++current) {
      const auto &entry = *current;
      const auto &key = adapt::as_string(entry.first);
      using storage_type = decltype(adapt::as_string(entry.first));
      using text_type = compat::remove_cvref_t<storage_type>;
      static_assert(std::is_same<char, typename text_type::value_type>::value,
        "Ryml object keys require char text from the string adapter");

      using view_type = compat::basic_string_view<
        typename text_type::value_type,
        typename text_type::traits_type>;

      constexpr bool borrows_source =
        std::is_lvalue_reference<storage_type>::value
        || std::is_same<text_type, view_type>::value;

      const c4::csubstr text{key.data(), key.size()};
      auto child = node.append_child();
      // Owning key conversions are temporary, even for a reference tree.
      if (context.borrow_strings && borrows_source)
        child.set_key(text);
      else
        child.set_key_serialized(text);

      child.set_key_style(::ryml::KEY_DQUO);
      core::write_node(context.at(child).element(), entry.second);
    }
  }
};

} // namespace detail

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value && is_write_state<State>::value))
auto map_arithmetic_t::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  const auto write = typename traits::select<
    traits::case_<std::is_same<bool, compat::remove_cv_t<T>>::value,
      map_boolean_t>,
    traits::case_<std::is_integral<T>::value, map_integral_t>,
    map_floating_point_t>::type{};

  return write(std::move(context), value);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_integral<T>::value
    && !std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_write_state<State>::value))
auto map_integral_t::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  auto node = context.node();
  node.set_serialized(value);
  return node;
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_floating_point<T>::value && is_write_state<State>::value))
auto map_floating_point_t::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  auto node = context.node();
  // Preserve enough significant digits to recover the original value.
  node.set_serialized(c4::fmt::real(value,
    std::numeric_limits<T>::max_digits10, c4::FTOA_FLEX));
  return node;
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_write_state<State>::value))
auto map_boolean_t::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  auto node = context.node();
  // C4's plain bool conversion emits 0/1 rather than document booleans.
  node.set_serialized(c4::fmt::boolalpha(value));
  return node;
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_string_like<T>,
    is_write_state<State>>::value))
auto map_string_like_t::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  auto node = context.node();
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
  return node;
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_optional<T>, is_write_state<State>>::value))
auto map_optional_like_t::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  if (adapt::has_value(value))
    return core::write_node(std::move(context), adapt::get_value(value));

  auto node = context.node();
  node.set_val("null");
  node.set_val_style(::ryml::VAL_PLAIN);
  return node;
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::ryml::detail::is_mapped_range<const T>,
    is_write_state<State>>::value))
auto map_range_t::operator()(State context, const T &value) const
  -> ::ryml::NodeRef {
  auto node = context.node();
  using element_type = compat::range_value_t<T>;
  using element_reference = compat::range_reference_t<const T>;

  // Materialized non-scalar elements may own strings borrowed by the tree.
  static_assert(compat::disjunction<std::is_scalar<element_type>,
    compat::conjunction<std::is_lvalue_reference<element_reference>,
      std::is_same<element_type,
        compat::remove_cvref_t<element_reference>>>>::value,
    "Default range serialization requires lvalue references to value_type "
    "for non-scalar elements; specialize cp::from_type_t for value-producing "
    "or converting-proxy iterators.");

  detail::write_range_case<traits::is_key_value<T>::value>::apply(
    std::move(context),
    value,
    node);

  // TODO: [medium] Consider supporting generators and temporary-producing
  // iterators by copying materialized elements' strings into tree storage.
  return node;
}

} // namespace ryml
} // namespace omni
