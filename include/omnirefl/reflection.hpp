// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

// Public reflection interface and generated metadata contract.

#pragma once

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include <omnirefl/compat.hpp>

#if defined(OMNI_ENABLE_INDEX_MODE) && OMNI_ENABLE_INDEX_MODE
#  include <omnirefl/unique_id.hpp>
#endif

namespace omni {

namespace detail {
namespace {

// `= T` is kept for frontend compatibility with generated metadata.
template <typename T, typename = T>
struct _reflected;

template <typename T>
using _meta = detail::_reflected<compat::decay_t<T>>;

constexpr bool _same_field_name(const char *lhs, const char *rhs) noexcept {
  return *lhs == *rhs && ('\0' == *lhs || _same_field_name(lhs + 1, rhs + 1));
}

template <typename Field, typename Fields>
struct _contains_field_name;

template <typename Field>
struct _contains_field_name<Field, std::tuple<>>: std::false_type {};

template <typename Field, typename Head, typename... Tail>
struct _contains_field_name<Field, std::tuple<Head, Tail...>>:
    std::conditional<_same_field_name(Field::name(), Head::name()),
      std::true_type,
      _contains_field_name<Field, std::tuple<Tail...>>>::type {};

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
      typename std::conditional<
        _is_public_field_visible_from<Record, BaseField>::value
          && !_contains_field_name<BaseField, std::tuple<OwnField...>>::value,
        std::tuple<Collected..., BaseField>,
        std::tuple<Collected...>>::type> {};

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

enum class reflected_entity {
  record, // struct | class | union
  enumeration, // enum | enum class
};

// todo: consider extending reflected entity metadata for fundamental/scalar
// field types.

// todo: what about field pointer? `reflected_field<T::name>` is a valid
// use-case, to get a string "name" for example

/// todo: use the tool's pass to detect the invalid usage
/// do not add specializations
/// (!!!) do not instantiate this outside of a reflected scope
///
/// note: T is decayed by generated specialization
template <typename T, typename = void>
struct is_reflected;

#if defined(OMNI_TOOL_RUN)
template <typename T, typename S>
struct is_reflected: std::false_type {};
#endif

#if defined(OMNI_TYPE_T_DEFINED)
template <typename T>
struct type_t;
#else
// `type_t` is an instrumentation primitive recognized by the reflection tool.
// todo: consider moving its shared definition to a standalone header.
template <typename T>
struct type_t {
  using type = T;
};
#  define OMNI_TYPE_T_DEFINED
#endif

#if OMNI_CPLUSPLUS >= 201402L
template <typename T>
constexpr type_t<T> type{};
#endif

#if defined(OMNI_TOOL_RUN)
template <typename T, reflected_entity = reflected_entity::record>
struct meta_t;

// Generated entity metadata is unavailable during the tool run. Language-level
// enum detection keeps record_binding_t/enum_binding_t visitor overloads
// deducible there.
template <typename T,
  reflected_entity = std::is_enum<compat::decay_t<T>>::value
    ? reflected_entity::enumeration
    : reflected_entity::record>
struct binding_t;
#else
template <typename T, reflected_entity = detail::_meta<T>::entity()>
struct meta_t;

template <typename T,
  reflected_entity = detail::_meta<compat::decay_t<T>>::entity()>
struct binding_t;
#endif

/// Binding for a reflected record without exposing the entity discriminator.
template <typename T>
using record_binding_t = binding_t<T, reflected_entity::record>;

/// Binding for a reflected enumeration without exposing the entity
/// discriminator.
template <typename T>
using enum_binding_t = binding_t<T, reflected_entity::enumeration>;

template <typename Owner, typename FieldMeta>
struct field_meta_t;

template <typename Record, typename FieldMeta>
struct field_binding_t;

namespace detail {

template <typename Owner, typename FieldMeta>
struct _is_writable_field:
    std::integral_constant<bool,
      !FieldMeta::is_const()
        && (!std::is_const<typename std::remove_reference<Owner>::type>::value
          || FieldMeta::is_mutable())> {};

} // namespace detail

// todo: provide QoL predicates and tuple filtering that combine field/owner
// write eligibility with value assignability before calling set_value.

#if defined(__cpp_concepts)
// refactorme: consider replacing tag-only concepts with structural
// public-interface concepts. They would advertise the callable surface
// (`type_name`, `public_fields`, `enumerators`, field `value`/`set_value`,
// etc.) instead of only proving that a wrapper type was produced by omnirefl.
// Possible limitation: structural checks may instantiate too much generated
// reflection/query surface before the generated header exists.
// Concepts intentionally check marker tags instead of exact specialization
// shapes. As of this writing, the entity parameter pollutes meta/binding
// interface types; the tags keep C++20 visitor syntax stable if internals
// change, and accept cv/ref-qualified arguments.
template <typename T>
concept meta = requires { typename compat::remove_cvref_t<T>::omni_meta_tag; };

template <typename T>
concept binding =
  requires { typename compat::remove_cvref_t<T>::omni_binding_tag; };

template <typename T>
concept field_meta =
  requires { typename compat::remove_cvref_t<T>::omni_field_meta_tag; };

template <typename T>
concept field_binding =
  requires { typename compat::remove_cvref_t<T>::omni_field_binding_tag; };
#endif

/// Experimental utilities backed by generated reflection metadata.
///
/// These utilities are callable only from within a reflected scope. Their
/// declarations remain in `reflection.hpp` while `aggregate_into` is the only
/// generated utility.
namespace refl {

/// Whether generated aggregate construction is available for `T`.
template <typename T, typename = void>
struct is_aggregatable: std::false_type {};

template <typename T>
struct is_aggregatable<T,
  compat::void_t<decltype(omni::detail::_meta<T>::is_aggregatable())>>:
    std::integral_constant<bool, omni::detail::_meta<T>::is_aggregatable()> {};

namespace detail {

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

template <std::size_t Index,
  typename TargetField,
  typename Fields,
  typename Target,
  bool End>
struct _get_t;

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

// REFACTORME: Decide whether this provisional tuple-only lookup remains the
// aggregate-construction customization point before stabilizing the interface.
/// Return the first same-named field constructed as `Target`.
///
/// The current `Fields` protocol is `std::tuple<Field...>`. Each field provides
/// a static `name()` and exposes its value through `.value()` as an rvalue.
/// The first name match is consumed and must construct `Target`. A missing or
/// incompatible field is a compile-time error.
template <typename TargetField, typename Target, typename... Field>
Target get(std::tuple<Field...> &fields) {
  using fields_t = std::tuple<Field...>;
  return detail::_get_t<0,
    TargetField,
    fields_t,
    Target,
    sizeof...(Field) == 0>::from(fields);
}

/// Shallowly construct `T` from the current tuple-based field protocol.
///
/// `Fields` currently follows the `std::tuple<Field...>` protocol documented
/// by `get`. The generated header supplies the implementation for each
/// supported destination type.
///
/// Generated specializations query each destination public field by name.
/// Every destination public field is required to have a same-named source field
/// whose consumed value constructs it; source-only fields are ignored.
/// Unions and aggregates with base classes are not currently supported.
///
/// Nested values are supported only when the destination field is directly
/// constructible from the same-named source value. Differently shaped nested
/// aggregates are not recursively converted.
template <typename T, typename Fields>
T aggregate_into(Fields &&fields) {
  return detail::aggregate_into_t<T>::from(std::forward<Fields>(fields));
}

} // namespace refl

struct reflected_call_t {
  private:
#if !defined(OMNI_TOOL_RUN) \
  && !defined(OMNI_INCLUDED_GENERATED_REFLECTION_HEADER)
  // Ad hoc for normal IDE/clangd parsing before the first tool run.
  //
  // Reflected source files force-include a generated header. During CMake
  // configure that header may exist only as an empty placeholder, so
  // `reflected_call(...)` must still be parseable even though `_reflected<T>`
  // specializations are unavailable.
  //
  // This is not used by the omnirefl tool run: `OMNI_TOOL_RUN` still uses
  // `_tool_arg(...)` return-type deduction so invalid visitors, such as lambdas
  // missing a required trailing return type, can be diagnosed by the tool.
  //
  // This placeholder is only for the pre-generation IDE parse, where real
  // `_reflect_arg(...)` deduction would instantiate `binding_t<T>`/`meta_t<T>`
  // and fail because the generated metadata is absent. After omnirefl generates
  // the header, `OMNI_INCLUDED_GENERATED_REFLECTION_HEADER` is defined and real
  // visitor invocation is compiled instead.
  struct _ungenerated_result {
    template <typename T>
    constexpr operator T() const
      noexcept(std::is_nothrow_default_constructible<T>::value) {
      return T{};
    }

    // Declaration only: this placeholder is parsed but never executed before
    // reflection generation.
    template <typename T>
    operator T &() const noexcept;
  };
#endif

#if defined(OMNI_TOOL_RUN)
  // During the tool run generated `_reflected<T>` metadata does not exist yet,
  // so real `_reflect_arg(value)` would instantiate `binding_t<T>` and fail
  // while trying to evaluate `_reflected<T>::entity()`.
  //
  // The reflected_call return type still has to look like
  // `impl(meta_t<T>...)` / `impl(binding_t<T>...)` so Clang can parse and match
  // user calls. These declaration-only functions are used exclusively in this
  // unevaluated return type; the visitor body remains uninstantiated until the
  // generated header exists.
  template <typename T>
  static constexpr meta_t<T> _tool_arg(type_t<T>) noexcept;

  template <typename T>
  static binding_t<T &&> _tool_arg(T &&) noexcept;
#endif

  template <typename T>
  static constexpr meta_t<T> _reflect_arg(type_t<T>) noexcept {
    return {};
  }

  template <typename T>
  static constexpr binding_t<T &&> _reflect_arg(T &&t) noexcept(
    noexcept(binding_t<T &&>{std::forward<T>(t)})) {
    return binding_t<T &&>{std::forward<T>(t)};
  }

  public:
  // todo: decide whether `reflected_call` should ever support constexpr
  // evaluation. It currently cannot be constexpr as a whole: during the tool
  // run the call is parsed and matched, but intentionally does not evaluate the
  // visitor before generated reflection exists.
  template <typename Impl, typename... Args>
  auto operator()(Impl &&impl, Args &&...args) const
#if defined(OMNI_TOOL_RUN)
    -> decltype(std::declval<Impl &&>()(_tool_arg(std::declval<Args &&>())...));
#elif !defined(OMNI_INCLUDED_GENERATED_REFLECTION_HEADER)
    -> _ungenerated_result;
#else
    -> decltype(std::declval<Impl &&>()(
      _reflect_arg(std::declval<Args &&>())...));
#endif
};

namespace detail {
namespace {

template <typename T>
struct reflected_arg_type {
  using type = T;
};

template <typename T>
struct reflected_arg_type<type_t<T>> {
  using type = T;
};

} // namespace
} // namespace detail

/// class to invoke a callable implementation object
#if defined(OMNI_TOOL_RUN)
// Tool-run operator definition must stay available in this translation unit:
// reflected_call can receive local/unnamed visitor types, and Clang rejects a
// used-but-undefined function template specialization whose type has no
// linkage. The body is still unevaluated for tool purposes, hence the return
// warning suppression below.
#  if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wreturn-type"
#  elif defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wreturn-type"
#  elif defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4715)
#  endif
#endif

template <typename Impl, typename... Args>
auto reflected_call_t::operator()(Impl &&impl, Args &&...args) const
#if defined(OMNI_TOOL_RUN)
  -> decltype(std::declval<Impl &&>()(_tool_arg(std::declval<Args &&>())...)) {
#elif !defined(OMNI_INCLUDED_GENERATED_REFLECTION_HEADER)
  -> reflected_call_t::_ungenerated_result {
#else
  -> decltype(std::declval<Impl &&>()(
    _reflect_arg(std::declval<Args &&>())...)) {
#endif
#if defined(OMNI_ENABLE_INDEX_MODE) && OMNI_ENABLE_INDEX_MODE
  int registered[] = {0,
    ((void)detail::_reflected_indexed_type<typename detail::reflected_arg_type<
        typename std::decay<Args>::type>::type>{},
      0)...};
  (void)registered;
#else
  (void)sizeof...(args);
#endif

  (void)impl;

  // Tool-run calls are parsed and matched, but never evaluated. The missing
  // return is intentional there: constructing an arbitrary visitor result would
  // instantiate exactly the user code reflected_call is meant to defer.
#if !defined(OMNI_TOOL_RUN) \
  && !defined(OMNI_INCLUDED_GENERATED_REFLECTION_HEADER)
  return {};
#elif !defined(OMNI_TOOL_RUN)
  return std::forward<Impl>(impl)(_reflect_arg(std::forward<Args>(args))...);
#endif
}

#if defined(OMNI_TOOL_RUN)
#  if defined(__clang__)
#    pragma clang diagnostic pop
#  elif defined(__GNUC__)
#    pragma GCC diagnostic pop
#  elif defined(_MSC_VER)
#    pragma warning(pop)
#  endif
#endif

// Field metadata is wrapped as functions instead of exposing member pointers.
// This keeps normal fields and bitfields on the same interface: bitfields can
// be read/written through generated accessors, but cannot be addressed by
// pointer-to-member.
template <typename Owner, typename FieldMeta>
struct field_meta_t {
  using omni_field_meta_tag = void;
  using owner_type = compat::decay_t<Owner>;
  using reflected = FieldMeta;
  using type = typename reflected::type;

  static constexpr const char *name() noexcept {
    return reflected::name();
  }

  /// Source-spelled field type without namespace prefixes, e.g. `outer::item`.
  static constexpr const char *type_name() noexcept {
    return reflected::type_name();
  }

  /// Namespace-qualified source spelling, e.g. `app::outer::item`.
  static constexpr const char *qualified_type_name() noexcept {
    return reflected::qualified_type_name();
  }

  /// Documentation text; empty when absent or generated with
  /// `--no-annotations`.
  static constexpr const char *documentation() noexcept {
    return reflected::documentation();
  }

  // Index is local to the field's declaring record.
  // Flattened inherited public field tuples may contain repeated indexes.
  static constexpr std::size_t index() noexcept {
    return reflected::index();
  }

  static constexpr bool is_const() noexcept {
    return reflected::is_const();
  }

  static constexpr bool is_mutable() noexcept {
    return reflected::is_mutable();
  }

  static constexpr bool is_volatile() noexcept {
    return reflected::is_volatile();
  }

  /// True when a type-preserving read equivalent to
  /// `const auto &value = object.field` is safe.
  static constexpr bool has_value_access() noexcept {
    return reflected::has_value_access();
  }

  /// True when `auto &value = object.field` can bind directly and safely.
  /// Bit-fields cannot bind directly; misaligned packed fields are unsafe.
  static constexpr bool has_reference_access() noexcept {
    return reflected::has_reference_access();
  }

  /// True when the field declaration has a deprecated attribute.
  static constexpr bool is_deprecated() noexcept {
    return reflected::is_deprecated();
  }

  /// Read the field through an lvalue owner.
  ///
  /// @details `R` keeps lookup dependent, so an unavailable generated accessor
  /// fails only when used rather than while forming `public_fields()`.
  template <typename T,
    typename R = reflected,
    typename std::enable_if<R::has_value_access(), int>::type = 0>
  static constexpr auto value(T &t) noexcept -> decltype(R::value(t)) {
    return R::value(t);
  }

  /// Return a field reference preserving owner cv-qualification.
  ///
  /// @details `R` keeps lookup dependent, so an unavailable generated accessor
  /// fails only when used rather than while forming `public_fields()`.
  template <typename T,
    typename R = reflected,
    typename std::enable_if<R::has_reference_access(), int>::type = 0>
  static constexpr auto ref(T &t) noexcept -> decltype(R::ref(t)) {
    return R::ref(t);
  }

  /// Assign fields that support assignment, including bitfields.
  ///
  /// @details `R` keeps generated assignment lookup and availability checks
  /// dependent until use.
  template <typename T,
    typename V,
    typename OwnerRef = T &&,
    typename R = reflected,
    typename std::enable_if<R::has_value_access()
        && detail::_is_writable_field<OwnerRef, R>::value,
      int>::type = 0>
  static void set_value(T &&t, V &&v) {
    R::set_value(std::forward<T>(t), std::forward<V>(v));
  }
};

// Field binding pairs field metadata with an object instance. It uses the same
// generated accessors as field_meta_t, so bitfields remain readable/writable
// even though pointer-to-member cannot represent them.
template <typename Record, typename FieldMeta>
struct field_binding_t {
  using omni_field_binding_tag = void;
  using type = typename FieldMeta::type;
  using meta = FieldMeta;

  Record &_owner;

  static constexpr const char *name() noexcept {
    return meta::name();
  }

  /// Source-spelled field type without namespace prefixes, e.g. `outer::item`.
  static constexpr const char *type_name() noexcept {
    return meta::type_name();
  }

  /// Namespace-qualified source spelling, e.g. `app::outer::item`.
  static constexpr const char *qualified_type_name() noexcept {
    return meta::qualified_type_name();
  }

  /// Documentation text; empty when absent or generated with
  /// `--no-annotations`.
  static constexpr const char *documentation() noexcept {
    return meta::documentation();
  }

  static constexpr std::size_t index() noexcept {
    return meta::index();
  }

  static constexpr bool is_const() noexcept {
    return meta::is_const();
  }

  static constexpr bool is_mutable() noexcept {
    return meta::is_mutable();
  }

  static constexpr bool is_volatile() noexcept {
    return meta::is_volatile();
  }

  /// True when a type-preserving read equivalent to
  /// `const auto &value = object.field` is safe.
  static constexpr bool has_value_access() noexcept {
    return meta::has_value_access();
  }

  /// True when `auto &value = object.field` can bind directly and safely.
  /// Bit-fields cannot bind directly; misaligned packed fields are unsafe.
  static constexpr bool has_reference_access() noexcept {
    return meta::has_reference_access();
  }

  /// True when the field declaration has a deprecated attribute.
  static constexpr bool is_deprecated() noexcept {
    return meta::is_deprecated();
  }

  /// Read the field without moving from the owner.
  ///
  /// The binding's value category selects access in the style of
  /// `std::optional::value()` and `std::expected::value()`. `field.value()` is
  /// deliberately read-only; `std::move(field).value()` requests consuming
  /// access. Move the binding, not the result of lvalue `value()`.
  ///
  /// @details `M` keeps lookup dependent until the accessor is used.
  template <typename M = meta,
    typename std::enable_if<M::has_value_access(), int>::type = 0>
  constexpr auto value() const & noexcept -> decltype(M::value(_owner)) {
    return M::value(_owner);
  }

  /// Consuming access for referenceable fields.
  ///
  /// Moving the trivial field-binding proxy selects this overload; it does not
  /// transfer proxy ownership. `std::move(field.ref())` is the explicit
  /// equivalent when reference access is available.
  ///
  /// @details `M` keeps lookup and the availability check dependent until use.
  template <typename M = meta,
    typename std::enable_if<M::has_value_access() && M::has_reference_access(),
      int>::type = 0>
  constexpr auto value() && noexcept -> decltype(std::move(M::ref(_owner))) {
    return std::move(M::ref(_owner));
  }

  /// Consuming syntax for fields without reference access.
  ///
  /// Bitfields and unsafe packed scalars cannot expose an rvalue reference, so
  /// this overload copies while preserving the uniform
  /// `std::move(field).value()` expression.
  ///
  /// @details `M` keeps lookup and the availability check dependent until use.
  template <typename M = meta,
    typename std::enable_if<M::has_value_access() && !M::has_reference_access(),
      int>::type = 0>
  constexpr auto value() && noexcept -> decltype(M::value(_owner)) {
    return M::value(_owner);
  }

  /// Return a field reference preserving owner cv-qualification.
  ///
  /// Use `std::move(field.ref())` to move explicitly from a referenceable
  /// field. Prefer expected-like `std::move(field).value()` in generic code
  /// because it also supports non-referenceable fields through copy fallback.
  ///
  /// @details `M` keeps lookup and the availability check dependent until use.
  template <typename M = meta,
    typename std::enable_if<M::has_reference_access(), int>::type = 0>
  constexpr auto ref() const noexcept -> decltype(M::ref(_owner)) {
    return M::ref(_owner);
  }

  /// Preserve the existing implicit value conversion.
  ///
  /// @details `M` keeps lookup dependent until the conversion is used.
  template <typename M = meta,
    typename std::enable_if<M::has_value_access(), int>::type = 0>
  constexpr operator decltype(M::value(_owner))() const noexcept {
    return value();
  }

  /// QoL dereference access for referenceable fields.
  ///
  /// @details `M` keeps lookup and the availability check dependent until use.
  template <typename M = meta,
    typename std::enable_if<M::has_reference_access(), int>::type = 0>
  constexpr auto operator*() const noexcept -> decltype(M::ref(_owner)) {
    return M::ref(_owner);
  }

  /// QoL member access returning the field's actual address even when its type
  /// overloads `operator&`.
  ///
  /// `std::addressof` is available since C++11 but is `constexpr` only since
  /// C++17, so this accessor follows the same constant-evaluation boundary.
  ///
  /// @details `M` keeps lookup and the availability check dependent until use.
  template <typename M = meta,
    typename std::enable_if<M::has_reference_access(), int>::type = 0>
#if OMNI_CPLUSPLUS >= 201703L
  constexpr
#endif
    auto operator->() const noexcept
    -> decltype(std::addressof(M::ref(_owner))) {
    return std::addressof(M::ref(_owner));
  }

  /// Assign fields that support assignment, including bitfields.
  ///
  /// @details `M` keeps generated assignment lookup and availability checks
  /// dependent until use.
  template <typename V,
    typename OwnerRef = Record &,
    typename M = meta,
    typename std::enable_if<M::has_value_access()
        && detail::_is_writable_field<OwnerRef, M>::value,
      int>::type = 0>
  void set_value(V &&v) {
    M::set_value(_owner, std::forward<V>(v));
  }

  constexpr explicit field_binding_t(Record &owner): _owner(owner) {}
};

#if defined(OMNI_TOOL_RUN)
template <typename T, reflected_entity Entity>
struct meta_t {
  using omni_meta_tag = void;
  using type = compat::decay_t<T>;

  static constexpr bool has_bases() noexcept;
  static constexpr bool is_aggregatable() noexcept;

  template <typename U>
  static constexpr binding_t<compat::decay_t<U>> bind(U &&) noexcept;
};

template <typename T, reflected_entity Entity>
struct binding_t {
  using omni_binding_tag = void;
  using type = compat::decay_t<T>;
  using owning = typename std::conditional<std::is_reference<T>::value,
    std::false_type,
    std::true_type>::type;
  using storage_t = typename std::conditional<owning::value,
    type,
    typename std::remove_reference<T>::type &>::type;

  // Entity inference is unavailable before generated metadata exists. Both
  // names preserve record/enum storage expressions in unevaluated visitor
  // return types; this declaration-only shape is never constructed.
  storage_t record;
  storage_t enum_value;

  template <typename U, reflected_entity E>
  constexpr operator binding_t<U, E>() const noexcept;
};

#else
template <typename T>
struct meta_t<T, reflected_entity::record> {
  using omni_meta_tag = void;
  using reflected = detail::_meta<T>;
  using type = typename reflected::type;

  /// Public fields visible through `type`; hidden inherited fields are omitted.
  using public_fields_t =
    detail::_all_visible_public_fields_t<reflected>; //< yields std::tuple<...>

  private:
  friend struct reflected_call_t;

  template <typename U>
  friend constexpr meta_t<compat::decay_t<U>> reflected() noexcept;

  template <typename, reflected_entity>
  friend struct binding_t;

  constexpr meta_t() noexcept = default;

  // C++11 ad hoc. `auto` lambda arguments only since C++14
  template <typename _T>
  struct _bind_fields_metadata {
    _T &t;

    template <typename... FieldMeta>
    constexpr auto operator()(FieldMeta...) const noexcept
      -> std::tuple<field_binding_t<_T, FieldMeta>...> {
      return std::make_tuple(field_binding_t<_T, FieldMeta>{t}...);
    }
  };

  template <typename _T>
  static constexpr auto _public_fields(_T &t) noexcept
    -> decltype(compat::apply(_bind_fields_metadata<_T>{t},
      public_fields_t{})) {
    return compat::apply(_bind_fields_metadata<_T>{t}, public_fields_t{});
  }

  public:
  static constexpr const char *type_name() noexcept {
    return reflected::type_name();
  }

  static constexpr const char *qualified_type_name() noexcept {
    return reflected::qualified_type_name();
  }

  static constexpr const char *documentation() noexcept {
    return reflected::documentation();
  }

  static constexpr reflected_entity entity() noexcept {
    return reflected::entity();
  }

  /// Whether the record directly declares any base class.
  static constexpr bool has_bases() noexcept {
    return reflected::has_bases();
  }

  /// Whether generated aggregate construction is available for the record.
  static constexpr bool is_aggregatable() noexcept {
    return refl::is_aggregatable<type>::value;
  }

  /// Metadata for public fields visible through the reflected record.
  static constexpr public_fields_t public_fields() noexcept {
    return {};
  }

  template <typename U,
    typename Binding =
      compat::conditional_t<std::is_lvalue_reference<U &&>::value,
        U &&,
        compat::decay_t<U>>>
  static constexpr auto bind(U &&t) noexcept(
    noexcept(binding_t<Binding>{std::forward<U>(t)}))
    -> decltype(binding_t<Binding>{std::forward<U>(t)}) {
    return binding_t<Binding>{std::forward<U>(t)};
  }

  template <typename U = type,
    typename std::enable_if<std::is_default_constructible<U>::value,
      int>::type = 0>
  static constexpr binding_t<type> bind() noexcept(
    std::is_nothrow_default_constructible<type>::value) {
    return binding_t<type>{type{}};
  }

  // todo: mutable_public_fields(T &t) and mutable_public_fields(const T &t)
};

template <typename T>
struct meta_t<T, reflected_entity::enumeration> {
  using omni_meta_tag = void;
  using reflected = detail::_meta<T>;
  using type = typename reflected::type;

  static_assert(std::is_enum<type>::value, "Type is not a enum");
  static_assert(is_reflected<type>::value,
    "Type was not reflected (Calling outside a reflected scope?)");

  static_assert(reflected_entity::enumeration == reflected::entity(),
    "Inconcistent reflection");

  private:
  friend struct reflected_call_t;

  template <typename U>
  friend constexpr meta_t<compat::decay_t<U>> reflected() noexcept;

  constexpr meta_t() noexcept = default;

  public:
  static constexpr const char *type_name() noexcept {
    return reflected::type_name();
  }

  static constexpr const char *qualified_type_name() noexcept {
    return reflected::qualified_type_name();
  }

  static constexpr const char *documentation() noexcept {
    return reflected::documentation();
  }

  static constexpr reflected_entity entity() noexcept {
    return reflected::entity();
  }

  // yields std::array of pair<type, const char*>
  static constexpr auto enumerators() noexcept
    -> decltype(reflected::enumerators()) {
    return reflected::enumerators();
  }
};

template <typename T>
struct binding_t<T, reflected_entity::record> {
  using omni_binding_tag = void;
  using type = compat::decay_t<T>;
  using reflected = typename meta_t<type>::reflected;

  using owning = typename std::conditional<std::is_reference<T>::value,
    std::false_type,
    std::true_type>::type;

  using storage_t = typename std::conditional<owning::value,
    type, //< own a value
    typename std::remove_reference<T>::type & //< hold a reference
    >::type;

  // todo: add an explicit interface to `std::move` an owned value out of a
  // binding_t<T> without exposing the storage member.
  storage_t record;

  static constexpr const char *type_name() noexcept {
    return reflected::type_name();
  }

  static constexpr const char *qualified_type_name() noexcept {
    return reflected::qualified_type_name();
  }

  static constexpr const char *documentation() noexcept {
    return reflected::documentation();
  }

  constexpr operator decltype((std::declval<const storage_t &>()))() const {
    return record;
  }

  // C++11 does not allow non-const constexpr member functions.
#  if OMNI_CPLUSPLUS >= 201402L
  constexpr
#  endif
    /// Bindings for public fields visible through the reflected record.
    auto public_fields() & -> decltype(meta_t<type>::_public_fields(
      std::declval<storage_t &>())) {
    return meta_t<type>::_public_fields(record);
  }

  constexpr auto
    public_fields() const & -> decltype(meta_t<type>::_public_fields(
      std::declval<const storage_t &>())) {
    return meta_t<type>::_public_fields(record);
  }

  auto public_fields() && -> decltype(meta_t<type>::_public_fields(
    std::declval<storage_t &>())) = delete;

  auto public_fields() const && -> decltype(meta_t<type>::_public_fields(
    std::declval<const storage_t &>())) = delete;

  private:
  friend struct reflected_call_t;
  friend struct meta_t<type>;

  // non-owning: T is a reference (U& / const U& / U&&)
  template <typename U,
    typename std::enable_if<!owning::value
        && std::is_convertible<U &&, T>::value,
      int>::type = 0>
  constexpr explicit binding_t(U &&u) noexcept: record(u) {}

  // owning: T is a value type (U / const U)
  template <typename U,
    typename std::enable_if<owning::value
        && std::is_constructible<type, U &&>::value,
      int>::type = 0>
  constexpr explicit binding_t(U &&u) noexcept(
    std::is_nothrow_move_constructible<type>::value)
      : record(std::forward<U>(u)) {}
};

template <typename T>
struct binding_t<T, reflected_entity::enumeration> {
  using omni_binding_tag = void;
  using reflected = typename meta_t<T>::reflected;
  using type = typename reflected::type;

  using owning = typename std::conditional<std::is_reference<T>::value,
    std::false_type,
    std::true_type>::type;

  using storage_t = typename std::conditional<owning::value,
    type, //< own a value
    typename std::remove_reference<T>::type & //< hold a reference
    >::type;

  // todo: add an explicit interface to `std::move` an owned value out of a
  // binding_t<T> without exposing the storage member.
  storage_t enum_value;

  static constexpr const char *type_name() noexcept {
    return reflected::type_name();
  }

  static constexpr const char *qualified_type_name() noexcept {
    return reflected::qualified_type_name();
  }

  static constexpr const char *documentation() noexcept {
    return reflected::documentation();
  }

  operator decltype((std::declval<const storage_t &>()))() const {
    return enum_value;
  }

  static constexpr auto enumerators() -> decltype(reflected::enumerators()) {
    return reflected::enumerators();
  }

  private:
  friend struct reflected_call_t;

  // non-owning: T is a reference (U& / const U& / U&&)
  template <typename U,
    typename std::enable_if<!owning::value
        && std::is_convertible<U &&, T>::value,
      int>::type = 0>
  constexpr explicit binding_t(U &&u) noexcept: enum_value(u) {}

  // owning: T is a value type (U / const U)
  template <typename U,
    typename std::enable_if<owning::value
        && std::is_constructible<type, U &&>::value,
      int>::type = 0>
  constexpr explicit binding_t(U &&u) noexcept(
    std::is_nothrow_move_constructible<type>::value)
      : enum_value(std::forward<U>(u)) {}
};
#endif

template <typename T>
constexpr meta_t<compat::decay_t<T>> reflected() noexcept {
  return {};
}

template <typename T>
constexpr auto reflected(T &&t) noexcept(
  noexcept(meta_t<compat::decay_t<T>>::bind(std::forward<T>(t))))
  -> decltype(meta_t<compat::decay_t<T>>::bind(std::forward<T>(t))) {
  return meta_t<compat::decay_t<T>>::bind(std::forward<T>(t));
}

constexpr reflected_call_t reflected_call{};

} // namespace omni
