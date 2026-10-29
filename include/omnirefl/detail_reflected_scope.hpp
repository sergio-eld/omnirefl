// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include <omnirefl/compat.hpp>
#include <omnirefl/traits.hpp>

// Included after the public entity and availability declarations.
namespace omni {

// Defined beside reflected_call; earlier helper and friend signatures need
// only the declaration.
template <typename T>
struct type_t;

namespace detail {

// Generated metadata has internal linkage so it remains local to each
// instrumented translation unit.
namespace {

/**
 * Generated-header specialization point.
 * The defaulted second parameter keeps generated specializations dependent,
 * delaying their bodies until `T` is complete.
 */
template <typename T, typename = T>
struct _reflected;

} // namespace

// Clang tooling fails to resolve `detail::_meta` from public declarations when
// the alias itself is in the unnamed namespace. Keep only the generated
// specialization local to the translation unit.
template <typename T>
using _meta =
#if defined(OMNI_DETAIL_UNGENERATED_REFLECTION)
  // Use the domain type as a placeholder before generated metadata exists.
  compat::decay_t<T>;
#else
  // Specializations are generated for unqualified types.
  _reflected<compat::decay_t<T>>;
#endif

#if defined(OMNI_DETAIL_UNGENERATED_REFLECTION)
// Before generation, metadata wrappers receive the domain type as a placeholder.
template <typename _M>
constexpr reflected_entity _metadata_entity() noexcept {
  return std::is_enum<compat::decay_t<_M>>::value
    ? reflected_entity::enumeration
    : reflected_entity::record;
}
#else
// Keep lookup in the signature so unavailable metadata fails by substitution
// when reflected(type_t<T>{}) also considers the value-binding overload.
template <typename _M>
constexpr auto _metadata_entity() noexcept -> decltype(_M::entity()) {
  return _M::entity();
}
#endif

template <typename _M>
using _reflected_type =
#if defined(OMNI_DETAIL_UNGENERATED_REFLECTION)
  compat::decay_t<_M>;
#else
  typename _M::type;
#endif

template <typename Owner, typename FieldMeta>
struct _is_writable_field:
    std::integral_constant<bool,
      !FieldMeta::is_const()
        && (FieldMeta::is_static()
          || !std::is_const<typename std::remove_reference<Owner>::type>::value
          || FieldMeta::is_mutable())> {};

namespace {

// C++11 constexpr functions require a single return expression.
constexpr bool _same_field_name(const char *lhs, const char *rhs) noexcept {
  return *lhs == *rhs && ('\0' == *lhs || _same_field_name(lhs + 1, rhs + 1));
}

// Generated field accessors use unqualified member access, so substitution
// reflects C++ lookup from the final record, including base ambiguity.
template <typename Record, typename Field, typename = void>
struct _is_public_field_visible_from: std::false_type {};

template <typename Record, typename Field>
struct _is_public_field_visible_from<Record,
  Field,
  compat::void_t<decltype(Field::value(std::declval<Record &>()))>>:
    std::true_type {};

// Multiple inheritance paths to one static declaration share a metadata type.
template <typename Record,
  typename OwnFields,
  typename RemainingBaseFields,
  typename Collected = std::tuple<>>
struct _all_visible_public_fields;

template <typename Record,
  typename... OwnField,
  typename BaseField,
  typename... Tail,
  typename... Collected>
struct _all_visible_public_fields<Record,
  std::tuple<OwnField...>,
  std::tuple<BaseField, Tail...>,
  std::tuple<Collected...>>:
    _all_visible_public_fields<Record,
      std::tuple<OwnField...>,
      std::tuple<Tail...>,
      compat::conditional_t<
        _is_public_field_visible_from<Record, BaseField>::value
          && !_meta<Record>::declares_member_name(BaseField::name())
          && (!BaseField::is_static()
            || !compat::disjunction<
              std::is_same<BaseField, Collected>...>::value),
        std::tuple<Collected..., BaseField>,
        std::tuple<Collected...>>> {};

template <typename Record, typename... OwnField, typename... Collected>
struct _all_visible_public_fields<Record,
  std::tuple<OwnField...>,
  std::tuple<>,
  std::tuple<Collected...>> {
  using type = std::tuple<Collected..., OwnField...>;
};

template <typename Bases>
struct _expand_bases;

template <typename... Bases>
struct _expand_bases<std::tuple<Bases...>> {
  using type = decltype(std::tuple_cat(std::declval<std::tuple<>>(),
    std::declval<
      typename _all_visible_public_fields<typename _meta<Bases>::type,
        typename _meta<Bases>::own_public_fields_t,
        typename _expand_bases<
          typename _meta<Bases>::public_bases_t>::type>::type>()...));
};

template <typename _M>
using _all_visible_public_fields_t =
  typename _all_visible_public_fields<typename _M::type,
    typename _M::own_public_fields_t,
    typename _expand_bases<typename _M::public_bases_t>::type>::type;

template <typename Record, typename Method, typename = void>
struct _is_public_method_visible_from: std::false_type {};

template <typename Record, typename _M>
struct _is_public_method_visible_from<Record,
  function_meta_t<_M>,
  compat::void_t<decltype(_M::template pointer<Record>())>>:
    std::integral_constant<bool,
      std::is_same<decltype(_M::template pointer<Record>()),
        decltype(_M::pointer())>::value
        && (_M::is_static()
          || std::is_convertible<Record *, typename _M::owner_type *>::value)> {
};

template <typename Record,
  typename OwnMethods,
  typename RemainingBaseMethods,
  typename Collected = std::tuple<>>
struct _all_visible_public_methods;

template <typename Record,
  typename... OwnMethod,
  typename BaseMethod,
  typename... Tail,
  typename... Collected>
struct _all_visible_public_methods<Record,
  std::tuple<OwnMethod...>,
  std::tuple<BaseMethod, Tail...>,
  std::tuple<Collected...>>:
    _all_visible_public_methods<Record,
      std::tuple<OwnMethod...>,
      std::tuple<Tail...>,
      compat::conditional_t<
        _is_public_method_visible_from<Record, BaseMethod>::value
          && !_meta<Record>::declares_member_name(BaseMethod::name())
          && (!BaseMethod::is_static()
            || !compat::disjunction<
              std::is_same<BaseMethod, Collected>...>::value),
        std::tuple<Collected..., BaseMethod>,
        std::tuple<Collected...>>> {};

template <typename Record, typename... OwnMethod, typename... Collected>
struct _all_visible_public_methods<Record,
  std::tuple<OwnMethod...>,
  std::tuple<>,
  std::tuple<Collected...>> {
  using type = std::tuple<Collected..., OwnMethod...>;
};

template <typename Bases>
struct _expand_base_methods;

template <typename... Bases>
struct _expand_base_methods<std::tuple<Bases...>> {
  using type = decltype(std::tuple_cat(std::declval<std::tuple<>>(),
    std::declval<
      typename _all_visible_public_methods<typename _meta<Bases>::type,
        typename _meta<Bases>::own_public_methods_t,
        typename _expand_base_methods<
          typename _meta<Bases>::public_bases_t>::type>::type>()...));
};

template <typename _M>
using _all_visible_public_methods_t =
  typename _all_visible_public_methods<typename _M::type,
    typename _M::own_public_methods_t,
    typename _expand_base_methods<typename _M::public_bases_t>::type>::type;

} // namespace

// TODO: [high] Handle `meta_for` queries in the tool so _meta no longer needs
// a domain-type placeholder before generation.

} // namespace detail

namespace refl {
namespace detail {

// Keep unsupported-type diagnostics dependent until the failing path is used.
template <typename>
struct _dependent_false: std::false_type {};

template <typename T, typename = T>
struct aggregate_into_t {
  static_assert(_dependent_false<T>::value,
    "omni::refl::aggregate_into: destination type is not a supported "
    "aggregate");

  // Generated reflection headers provide `from` through an
  // `aggregate_into_t<T>` specialization for each supported `T`.
  template <typename Fields>
  static T from(Fields &&);
};

template <typename TargetField, typename Field, typename Target, bool>
struct _construct_field_t;

template <typename TargetField, typename Field, typename Target>
struct _construct_field_t<TargetField, Field, Target, true> {
  static Target from(Field &field) {
    return static_cast<Target>(std::move(field).value());
  }
};

template <typename TargetField, typename Field, typename Target>
struct _construct_field_t<TargetField, Field, Target, false> {
  static_assert(_dependent_false<TargetField>::value,
    "omni::refl::aggregate_into: destination field is not constructible "
    "from the same-named source field");

  static Target from(Field &);
};

template <std::size_t Index,
  typename TargetField,
  typename Fields,
  typename Target,
  bool End>
struct _get_t;

// Specializations keep missing-field and construction diagnostics on the
// selected lookup path; unrelated tuple elements must not be instantiated.
template <std::size_t Index,
  typename TargetField,
  typename Fields,
  typename Target>
struct _get_t<Index, TargetField, Fields, Target, true> {
  static_assert(_dependent_false<TargetField>::value,
    "omni::refl::aggregate_into: destination field is missing from the "
    "source fields");

  static Target from(Fields &);
};

template <std::size_t Index,
  typename TargetField,
  typename Fields,
  typename Target>
struct _get_t<Index, TargetField, Fields, Target, false> {
  using field = typename std::tuple_element<Index, Fields>::type;

  static Target from(Fields &fields) {
    return from(fields,
      std::integral_constant<bool,
        omni::detail::_same_field_name(TargetField::name(), field::name())>{});
  }

  private:
  static Target from(Fields &fields, /*same_field_name=*/std::true_type) {
    using source = decltype(std::declval<field &&>().value());
    return _construct_field_t<TargetField,
      field,
      Target,
      std::is_constructible<Target,
        source>::value>::from(std::get<Index>(fields));
  }

  static Target from(Fields &fields, /*same_field_name=*/std::false_type) {
    return _get_t<Index + 1,
      TargetField,
      Fields,
      Target,
      Index + 1 == std::tuple_size<Fields>::value>::from(fields);
  }
};

} // namespace detail
} // namespace refl
} // namespace omni
