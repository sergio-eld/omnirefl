// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include <omnirefl/compat.hpp>

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

/// Generated-header specialization point.
/// The defaulted second parameter keeps generated specializations dependent,
/// delaying their bodies until `T` is complete.
template <typename T, typename = T>
struct _reflected;

} // namespace

// Clang tooling fails to resolve `detail::_meta` from public declarations when
// the alias itself is in the unnamed namespace. Keep only the generated
// specialization local to the translation unit.
template <typename T>
using _meta = //< Internal convenience accessor for generated metadata.
  _reflected<
    // Specializations are generated for unqualified types.
    compat::decay_t<T>>;

template <typename Owner, typename FieldMeta>
struct _is_writable_field:
    std::integral_constant<bool,
      !FieldMeta::is_const()
        && (!std::is_const<typename std::remove_reference<Owner>::type>::value
          || FieldMeta::is_mutable())> {};

#if !defined(OMNI_TOOL_RUN) \
  && !defined(OMNI_INCLUDED_GENERATED_REFLECTION_HEADER)
// Ad hoc: the IDE may parse before the generated header can name the callable's
// result. This placeholder is never executed.
struct _ungenerated_result {
  template <typename T>
  constexpr operator T() const
    noexcept(std::is_nothrow_default_constructible<T>::value) {
    return T{};
  }

  // Declaration only: this placeholder is never executed before generation.
  template <typename T>
  operator T &() const noexcept;
};
#endif

namespace {

#if defined(OMNI_ENABLE_INDEX_MODE) && OMNI_ENABLE_INDEX_MODE
// Index registrations must unwrap type tags to register the domain type.
template <typename T>
struct reflected_arg_type {
  using type = T;
};

template <typename T>
struct reflected_arg_type<type_t<T>> {
  using type = T;
};
#endif

// C++11 constexpr functions require a single return expression.
constexpr bool _same_field_name(const char *lhs, const char *rhs) noexcept {
  return *lhs == *rhs && ('\0' == *lhs || _same_field_name(lhs + 1, rhs + 1));
}

template <typename Field, typename Fields>
struct _contains_field_name;

template <typename Field>
struct _contains_field_name<Field, std::tuple<>>: std::false_type {};

template <typename Field, typename Head, typename... Tail>
struct _contains_field_name<Field, std::tuple<Head, Tail...>>:
    compat::conditional_t<_same_field_name(Field::name(), Head::name()),
      std::true_type,
      _contains_field_name<Field, std::tuple<Tail...>>> {};

// Generated field accessors use unqualified member access, so substitution
// reflects C++ lookup from the final record, including base ambiguity.
template <typename Record, typename Field, typename = void>
struct _is_public_field_visible_from: std::false_type {};

template <typename Record, typename Field>
struct _is_public_field_visible_from<Record,
  Field,
  compat::void_t<decltype(Field::value(std::declval<Record &>()))>>:
    std::true_type {};

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
          && !_contains_field_name<BaseField, std::tuple<OwnField...>>::value,
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

template <typename Meta>
using _all_visible_public_fields_t =
  typename _all_visible_public_fields<typename Meta::type,
    typename Meta::own_public_fields_t,
    typename _expand_bases<typename Meta::public_bases_t>::type>::type;

} // namespace
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
  static Target from(Fields &fields, std::true_type) {
    using source = decltype(std::declval<field &&>().value());
    return _construct_field_t<TargetField,
      field,
      Target,
      std::is_constructible<Target,
        source>::value>::from(std::get<Index>(fields));
  }

  static Target from(Fields &fields, std::false_type) {
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
