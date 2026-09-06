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
// Ad hoc: the IDE may parse with an empty generated-header placeholder.
// Allow result conversion until generation supplies the real visitor result.
// Instrumentation uses `_tool_arg` instead so invalid visitors are diagnosed.
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

#if defined(OMNI_TOOL_RUN)
// Ad hoc: generated `_reflected` specializations do not exist during the
// instrumentation parse, so reflection availability cannot be queried yet.
// TODO: Handle `is_reflected` queries in the instrumentation instead of
// substituting a false public trait definition.
template <typename T, typename S>
struct is_reflected: std::false_type {};

// Ad hoc: generated entity metadata is unavailable during the tool run, but
// reflected_call must still select record- and enum-specific visitor overloads.
// Infer the entity from the source type until the generated header exists.
/// Metadata wrapper; valid only within a reflected scope.
template <typename _M,
  reflected_entity = std::is_enum<compat::decay_t<_M>>::value
    ? reflected_entity::enumeration
    : reflected_entity::record>
struct meta_t;

// Visitor overload resolution needs the actual public `binding_t` type before
// generated metadata exists. This stand-in exposes only the signature surface
// that Clang may instantiate during instrumentation.
// TODO(high): Model field and enumerator element types during instrumentation.
// Empty tuples preserve direct return-type queries but not element-dependent
// signatures.
/// Value binding; valid only within a reflected scope.
template <typename T,
  reflected_entity Entity = std::is_enum<compat::decay_t<T>>::value
    ? reflected_entity::enumeration
    : reflected_entity::record>
struct binding_t {
  using type = compat::decay_t<T>;
  using owning = std::integral_constant<bool, !std::is_reference<T>::value>;
  using storage_t = compat::conditional_t<owning::value,
    type,
    typename std::remove_reference<T>::type &>;

  // Both names preserve record/enum storage expressions in unevaluated visitor
  // return types; this declaration-only shape is never constructed.
  storage_t _record;
  storage_t _enum_value;

  constexpr const storage_t &value() const & noexcept;
  auto value() && noexcept
    -> decltype(std::move(std::declval<storage_t &>()));
  constexpr auto value() const && noexcept
    -> decltype(std::move(std::declval<const storage_t &>()));

  storage_t &ref() & noexcept;
  constexpr const storage_t &ref() const & noexcept;

  static constexpr reflected_entity entity() noexcept {
    return Entity;
  }

  template <reflected_entity E = Entity,
    typename std::enable_if<E == reflected_entity::record, int>::type = 0>
#  if defined(__cpp_constexpr) && 201304L <= __cpp_constexpr
  constexpr
#  endif
    std::tuple<> public_fields() &;

  template <reflected_entity E = Entity,
    typename std::enable_if<E == reflected_entity::record, int>::type = 0>
  constexpr std::tuple<> public_fields() const &;

  template <reflected_entity E = Entity,
    typename std::enable_if<E == reflected_entity::enumeration, int>::type = 0>
  static constexpr std::tuple<> enumerators() noexcept;

  // Model conversion through the bound value while Clang probes concrete
  // visitor parameter types; the instrumentation stand-in is never evaluated.
  template <typename U, reflected_entity E>
  constexpr operator binding_t<U, E>() const noexcept;
};

// Ad hoc: generated metadata does not exist yet, but visitor overload
// resolution may inspect the record and enum wrapper surfaces below.
// TODO(high): Model metadata wrappers in the instrumentation instead of
// maintaining reduced definitions.
template <typename _M>
struct meta_t<_M, reflected_entity::record> {
  using reflected_type = compat::decay_t<_M>;

  static constexpr reflected_entity entity() noexcept {
    return reflected_entity::record;
  }

  static constexpr const char *type_name() noexcept {
    return _M::type_name();
  }

  static constexpr const char *qualified_type_name() noexcept {
    return _M::qualified_type_name();
  }

  static constexpr const char *documentation() noexcept {
    return _M::documentation();
  }

  static constexpr bool has_bases() noexcept {
    return _M::has_bases();
  }

  static constexpr bool is_aggregatable() noexcept {
    return _M::is_aggregatable();
  }

  static constexpr std::tuple<> public_fields() noexcept;

  template <typename U,
    typename std::enable_if<
      std::is_same<compat::decay_t<U>, reflected_type>::value,
      int>::type = 0,
    typename Binding =
      compat::conditional_t<std::is_lvalue_reference<U &&>::value,
        U &&,
        compat::decay_t<U>>>
  static constexpr binding_t<Binding> bind(U &&) noexcept;

  template <typename U = reflected_type,
    typename std::enable_if<std::is_same<U, reflected_type>::value
        && std::is_default_constructible<U>::value
        && std::is_constructible<U, U &&>::value,
      int>::type = 0>
  static constexpr binding_t<reflected_type> bind() noexcept(
    std::is_nothrow_default_constructible<reflected_type>::value
      && std::is_nothrow_constructible<reflected_type,
        reflected_type &&>::value);
};

template <typename _M>
struct meta_t<_M, reflected_entity::enumeration> {
  using reflected_type = compat::decay_t<_M>;

  static constexpr reflected_entity entity() noexcept {
    return reflected_entity::enumeration;
  }

  static constexpr const char *type_name() noexcept {
    return _M::type_name();
  }

  static constexpr const char *qualified_type_name() noexcept {
    return _M::qualified_type_name();
  }

  static constexpr const char *documentation() noexcept {
    return _M::documentation();
  }

  static constexpr std::tuple<> enumerators() noexcept;

  template <typename U,
    typename std::enable_if<
      std::is_same<compat::decay_t<U>, reflected_type>::value,
      int>::type = 0,
    typename Binding =
      compat::conditional_t<std::is_lvalue_reference<U &&>::value,
        U &&,
        compat::decay_t<U>>>
  static constexpr binding_t<Binding> bind(U &&) noexcept;
};

namespace detail {

// Ad hoc: generated metadata is unavailable during instrumentation. These
// signatures preserve visitor overload resolution without constructing
// wrappers.
template <typename T>
constexpr meta_t<T> _tool_arg(type_t<T>) noexcept;

template <typename T>
binding_t<T &&> _tool_arg(T &&) noexcept;

} // namespace detail

// Ad hoc: queries must remain parseable so instrumentation can diagnose their
// source locations before generated metadata exists.
// TODO: Handle `reflected(...)` directly during instrumentation instead of
// providing tool-only overloads.
template <typename T>
constexpr meta_t<T> reflected(type_t<T>) noexcept {
  return {};
}

template <typename T,
  typename std::enable_if<!traits::is<type_t, compat::decay_t<T>>(),
    int>::type = 0>
constexpr auto reflected(T &&t) noexcept(
  noexcept(meta_t<compat::decay_t<T>>::bind(std::forward<T>(t))))
  -> decltype(meta_t<compat::decay_t<T>>::bind(std::forward<T>(t))) {
  return meta_t<compat::decay_t<T>>::bind(std::forward<T>(t));
}
#endif

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
