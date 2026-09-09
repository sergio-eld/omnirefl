// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

// Public reflection interface and generated metadata contract.

#pragma once

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include <omnirefl/compat.hpp>
#include <omnirefl/traits.hpp>

#if defined(OMNI_ENABLE_INDEX_MODE) && OMNI_ENABLE_INDEX_MODE
#  include <omnirefl/unique_id.hpp>
#endif

namespace omni {

enum class reflected_entity {
  /// `struct`, `class`, or named `union`.
  record,
  /// `enum` or `enum class`.
  enumeration,
};

/**
 * Reflection availability within a reflected scope.
 *
 * **Users must not specialize this trait.**
 *
 * Types become reflected only through instrumentation and its dependency
 * protocol. Queries ignore cv/ref qualification.
 */
template <typename T, typename = void>
struct is_reflected;

} // namespace omni

// Ad hoc: detail helpers depend on `reflected_entity`, so this include follows
// its declaration instead of the other includes at the top of the file.
#include <omnirefl/detail_reflected_scope.hpp>

namespace omni {

/// Reflected-scope-only field metadata; `_M` is opaque.
template <typename _M>
struct field_meta_t {
  /// Declared field type.
  using type = typename _M::type;

  /// Source-spelled field name.
  static constexpr const char *name() noexcept {
    return _M::name();
  }

  /**
   * Source-spelled field type with namespace qualifiers omitted.
   *
   * ```cpp
   * namespace app {
   * using account_id = int;
   * }
   * struct account {
   *   app::account_id id;
   * };
   * ```
   *
   * For `account::id`, returns the source-spelled alias `"account_id"`.
   *
   * @details Omnirefl's seamless integration relies on generated forward
   * declarations. A C++ type alias cannot be forward-declared or have metadata
   * distinct from its underlying type. Field declarations have no such
   * limitation, so their source spelling can be preserved as text for uses
   * such as schema generation.
   *
   * @note The `type` alias names the actual C++ field type. Within a reflected
   * scope, access its metadata by value or by type with:
   * ```cpp
   * template <omni::field_meta FieldMeta>
   * void inspect(FieldMeta) {
   *   const auto field_type_metadata =
   *     omni::reflected(omni::type_t<typename FieldMeta::type>{});
   *   using field_type_meta = omni::meta_for<typename FieldMeta::type>;
   * }
   * ```
   * Before C++20, use `omni::field_meta_t<_FieldMeta>` as the parameter type.
   */
  static constexpr const char *spelled_type_name() noexcept {
    return _M::type_name();
  }

  /**
   * Source-spelled field type including enclosing namespaces and records.
   *
   * ```cpp
   * namespace app {
   * using account_id = int;
   * }
   * struct account {
   *   app::account_id id;
   * };
   * ```
   *
   * For `account::id`, returns the source spelling `"app::account_id"`.
   *
   * @details Omnirefl's seamless integration relies on generated forward
   * declarations. A C++ type alias cannot be forward-declared or have metadata
   * distinct from its underlying type. Field declarations have no such
   * limitation, so their source spelling can be preserved as text for uses
   * such as schema generation.
   *
   * @note The `type` alias names the actual C++ field type. Within a reflected
   * scope, access its metadata by value or by type with:
   * ```cpp
   * template <omni::field_meta FieldMeta>
   * void inspect(FieldMeta) {
   *   const auto field_type_metadata =
   *     omni::reflected(omni::type_t<typename FieldMeta::type>{});
   *   using field_type_meta = omni::meta_for<typename FieldMeta::type>;
   * }
   * ```
   * Before C++20, use `omni::field_meta_t<_FieldMeta>` as the parameter type.
   */
  static constexpr const char *spelled_qualified_type_name() noexcept {
    return _M::qualified_type_name();
  }

  /**
   * Documentation comment attached to the field declaration.
   *
   * ```cpp
   * struct account {
   *   /// Stable identifier used to reference this account
   *   /// across requests and persisted records.
   *   ///
   *   /// Assigned by the storage layer when the account is created.
   *   int id;
   * };
   * ```
   *
   * Returns:
   *
   * ```text
   * Stable identifier used to reference this account
   * across requests and persisted records.
   *
   * Assigned by the storage layer when the account is created.
   * ```
   *
   * Comment markers and common indentation are removed. Line breaks and later
   * paragraphs are preserved. Returns an empty string when no documentation
   * comment is attached or generation uses `--no-annotations`.
   */
  static constexpr const char *documentation() noexcept {
    return _M::documentation();
  }

  /**
   * Zero-based index among reflected fields declared by the field's record.
   *
   * ```cpp
   * struct item {
   *   int first;  // index 0
   * private:
   *   int hidden; // not reflected; no index
   * public:
   *   int second; // index 1
   *   int third;  // index 2
   * };
   * ```
   *
   * Inherited fields retain their declaring-record index, so indexes can repeat
   * in a flattened `public_fields()` tuple.
   */
  static constexpr std::size_t index() noexcept {
    return _M::index();
  }

  /// Whether the declared field type has top-level `const` qualification.
  static constexpr bool is_const() noexcept {
    return _M::is_const();
  }

  /// Whether the field declaration uses the `mutable` specifier.
  static constexpr bool is_mutable() noexcept {
    return _M::is_mutable();
  }

  /// Whether the declared field type has top-level `volatile` qualification.
  static constexpr bool is_volatile() noexcept {
    return _M::is_volatile();
  }

  /**
   * Whether `value()` is available.
   *
   * Safely aligned fields are returned by reference; bit-fields and misaligned
   * packed scalars are returned by value.
   * Misaligned packed arrays have no value access.
   */
  static constexpr bool has_value_access() noexcept {
    return _M::has_value_access();
  }

  /**
   * Whether `ref()` can expose a direct field reference.
   *
   * Bit-fields and packed fields without safe reference alignment cannot.
   */
  static constexpr bool has_reference_access() noexcept {
    return _M::has_reference_access();
  }

  /// True when the field declaration has a deprecated attribute.
  static constexpr bool is_deprecated() noexcept {
    return _M::is_deprecated();
  }

  /**
   * Return an lvalue reference to the field, or a copy when direct reference
   * access is unavailable.
   *
   * Available only when `has_value_access()` is true. Referenceable fields
   * return an lvalue reference; bit-fields and misaligned packed scalars are
   * returned by value.
   *
   * @details `R` keeps lookup dependent, so an unavailable generated accessor
   * fails only when used rather than while forming `public_fields()`.
   */
  template <typename T,
    typename R = _M,
    typename std::enable_if<R::has_value_access(), int>::type = 0>
  static constexpr auto value(T &t) noexcept -> decltype(R::value(t)) {
    return R::value(t);
  }

  /**
   * Return a direct field reference with ordinary member-access cv semantics.
   *
   * Available only when `has_reference_access()` is true; the check is false
   * for bit-fields and packed fields without safe reference alignment.
   *
   * @details `R` keeps lookup dependent, so an unavailable generated accessor
   * fails only when used rather than while forming `public_fields()`.
   */
  template <typename T,
    typename R = _M,
    typename std::enable_if<R::has_reference_access(), int>::type = 0>
  static constexpr auto ref(T &t) noexcept -> decltype(R::ref(t)) {
    return R::ref(t);
  }

  /**
   * Assign through member access, including bit-fields.
   *
   * The generated accessor effectively performs
   * `t.field_name = std::forward<V>(v)`.
   *
   * Requires `has_value_access()` and a writable field for the supplied record.
   * Declared `const` fields and non-`mutable` fields of const records are not
   * writable.
   *
   * @details `R` keeps generated assignment lookup and availability checks
   * dependent until use.
   */
  template <typename T,
    typename V,
    typename R = _M,
    typename std::enable_if<R::has_value_access()
        && detail::_is_writable_field<T &&, R>::value,
      int>::type = 0>
  static void set_value(T &&t, V &&v) {
    R::set_value(std::forward<T>(t), std::forward<V>(v));
  }
};

/**
 * Reflected-scope-only field binding.
 *
 * `Record` is the cv-qualified bound record type, including the final record
 * through which inherited fields are accessed. `_M` is opaque.
 *
 * TODO(high): Decide whether this should inherit `field_meta_t<_M>`.
 *   This requires consistent pre-C++20 overload and C++20 concept semantics.
 *   The same static predicate should accept metadata and bindings:
 *   ```cpp
 *   template <omni::field_meta Field>
 *   using mutable_field = std::bool_constant<Field::is_mutable()>;
 *
 *   metadata.public_fields() | omni::fn::filter<mutable_field>();
 *   binding.public_fields() | omni::fn::filter<mutable_field>();
 *   ```
 */
template <typename Record, typename _M>
struct field_binding_t {
  /// Cv-qualified type of the bound record object.
  using record = Record;

  /// Metadata for the bound field.
  using meta = field_meta_t<_M>;

  /// Declared field type.
  using type = typename meta::type;

  record &_record;

  /// Source-spelled field name.
  static constexpr const char *name() noexcept {
    return meta::name();
  }

  /**
   * Source-spelled field type with namespace qualifiers omitted.
   *
   * ```cpp
   * namespace app {
   * using account_id = int;
   * }
   * struct account {
   *   app::account_id id;
   * };
   * ```
   *
   * For `account::id`, returns the source-spelled alias `"account_id"`.
   *
   * @details The field spelling can be preserved as generated text even though
   * an alias cannot be forward-declared or have metadata distinct from its
   * underlying type. Preserved spellings can be used when deriving schemas.
   *
   * @note The `type` alias names the actual C++ field type. Within a reflected
   * scope, access its metadata by value or by type with:
   * ```cpp
   * template <omni::field_binding FieldBinding>
   * void inspect(FieldBinding) {
   *   const auto field_type_metadata =
   *     omni::reflected(omni::type_t<typename FieldBinding::type>{});
   *   using field_type_meta = omni::meta_for<typename FieldBinding::type>;
   * }
   * ```
   * Before C++20, use `omni::field_binding_t<Record, _FieldMeta>` as the
   * parameter type.
   */
  static constexpr const char *spelled_type_name() noexcept {
    return meta::spelled_type_name();
  }

  /**
   * Source-spelled field type including enclosing namespaces and records.
   *
   * ```cpp
   * namespace app {
   * using account_id = int;
   * }
   * struct account {
   *   app::account_id id;
   * };
   * ```
   *
   * For `account::id`, returns the source spelling `"app::account_id"`.
   *
   * @details The field spelling can be preserved as generated text even though
   * an alias cannot be forward-declared or have metadata distinct from its
   * underlying type. Preserved spellings can be used when deriving schemas.
   *
   * @note The `type` alias names the actual C++ field type. Within a reflected
   * scope, access its metadata by value or by type with:
   * ```cpp
   * template <omni::field_binding FieldBinding>
   * void inspect(FieldBinding) {
   *   const auto field_type_metadata =
   *     omni::reflected(omni::type_t<typename FieldBinding::type>{});
   *   using field_type_meta = omni::meta_for<typename FieldBinding::type>;
   * }
   * ```
   * Before C++20, use `omni::field_binding_t<Record, _FieldMeta>` as the
   * parameter type.
   */
  static constexpr const char *spelled_qualified_type_name() noexcept {
    return meta::spelled_qualified_type_name();
  }

  /**
   * Documentation comment attached to the field declaration.
   *
   * ```cpp
   * struct account {
   *   /// Stable identifier used to reference this account
   *   /// across requests and persisted records.
   *   ///
   *   /// Assigned by the storage layer when the account is created.
   *   int id;
   * };
   * ```
   *
   * Returns:
   *
   * ```text
   * Stable identifier used to reference this account
   * across requests and persisted records.
   *
   * Assigned by the storage layer when the account is created.
   * ```
   *
   * Comment markers and common indentation are removed. Line breaks and later
   * paragraphs are preserved. Returns an empty string when no documentation
   * comment is attached or generation uses `--no-annotations`.
   */
  static constexpr const char *documentation() noexcept {
    return meta::documentation();
  }

  /**
   * Zero-based index among reflected fields declared by the field's record.
   *
   * ```cpp
   * struct item {
   *   int first;  // index 0
   * private:
   *   int hidden; // not reflected; no index
   * public:
   *   int second; // index 1
   *   int third;  // index 2
   * };
   * ```
   *
   * Inherited fields retain their declaring-record index, so indexes can repeat
   * in a flattened `public_fields()` tuple.
   */
  static constexpr std::size_t index() noexcept {
    return meta::index();
  }

  /**
   * Whether the declared field type has top-level `const` qualification.
   *
   * This does not describe the bound record's cv-qualification.
   */
  static constexpr bool is_const() noexcept {
    return meta::is_const();
  }

  /// Whether the field declaration uses the `mutable` specifier.
  static constexpr bool is_mutable() noexcept {
    return meta::is_mutable();
  }

  /// Whether the declared field type has top-level `volatile` qualification.
  static constexpr bool is_volatile() noexcept {
    return meta::is_volatile();
  }

  /**
   * Whether `value()` is available.
   *
   * Safely aligned fields are returned by reference; bit-fields and misaligned
   * packed scalars are returned by value.
   * Misaligned packed arrays have no value access.
   */
  static constexpr bool has_value_access() noexcept {
    return meta::has_value_access();
  }

  /**
   * Whether `ref()` can expose a direct field reference.
   *
   * Bit-fields and packed fields without safe reference alignment cannot.
   */
  static constexpr bool has_reference_access() noexcept {
    return meta::has_reference_access();
  }

  /// True when the field declaration has a deprecated attribute.
  static constexpr bool is_deprecated() noexcept {
    return meta::is_deprecated();
  }

  /**
   * Return an lvalue reference to the field, or a copy when direct reference
   * access is unavailable.
   *
   * Available only when `has_value_access()` is true. Referenceable fields
   * return an lvalue reference; bit-fields and misaligned packed scalars are
   * returned by value. Move the binding to select its rvalue-qualified access
   * path.
   *
   * @details `M` keeps lookup dependent until the accessor is used.
   */
  template <typename M = meta,
    typename std::enable_if<M::has_value_access(), int>::type = 0>
  constexpr auto value() const & noexcept -> decltype(M::value(_record)) {
    return M::value(_record);
  }

  /**
   * Return an rvalue reference when both `has_value_access()` and
   * `has_reference_access()` are true.
   *
   * `std::move(field).value()` selects this overload and returns an rvalue
   * reference to the field.
   *
   * @details `M` keeps lookup and the availability check dependent until use.
   */
  template <typename M = meta,
    typename std::enable_if<M::has_value_access() && M::has_reference_access(),
      int>::type = 0>
#if defined(__cpp_constexpr) && 201304L <= __cpp_constexpr
  constexpr
#endif
    auto value() && noexcept -> decltype(std::move(M::ref(_record))) {
    return std::move(M::ref(_record));
  }

  /**
   * Return a copy when the field has value access but cannot expose a
   * reference.
   *
   * Bit-fields and packed scalars cannot expose an rvalue reference, so this
   * overload copies.
   *
   * @details `M` keeps lookup and the availability check dependent until use.
   */
  template <typename M = meta,
    typename std::enable_if<M::has_value_access() && !M::has_reference_access(),
      int>::type = 0>
#if defined(__cpp_constexpr) && 201304L <= __cpp_constexpr
  constexpr
#endif
    auto value() && noexcept -> decltype(M::value(_record)) {
    return M::value(_record);
  }

  /**
   * Return a direct field reference with ordinary member-access cv semantics.
   *
   * Available only when `has_reference_access()` is true. Use
   * `std::move(field).value()` when the field should be moved; copy-only fields
   * still return a copy.
   *
   * @details `M` keeps lookup and the availability check dependent until use.
   */
  template <typename M = meta,
    typename std::enable_if<M::has_reference_access(), int>::type = 0>
  constexpr auto ref() const noexcept -> decltype(M::ref(_record)) {
    return M::ref(_record);
  }

  /**
   * Return the same lvalue reference or copy as `value()` through implicit
   * conversion.
   *
   * Available only when `has_value_access()` is true.
   *
   * @details `M` keeps lookup dependent until the conversion is used.
   */
  template <typename M = meta,
    typename std::enable_if<M::has_value_access(), int>::type = 0>
  constexpr operator decltype(M::value(_record))() const noexcept {
    return value();
  }

  /**
   * Return the direct field reference exposed by `ref()`.
   *
   * Available only when `has_reference_access()` is true.
   *
   * @details `M` keeps lookup and the availability check dependent until use.
   */
  template <typename M = meta,
    typename std::enable_if<M::has_reference_access(), int>::type = 0>
  constexpr auto operator*() const noexcept -> decltype(M::ref(_record)) {
    return M::ref(_record);
  }

  /**
   * Return a pointer to the field reference exposed by `ref()`.
   *
   * Available only when `has_reference_access()` is true. `std::addressof` is
   * available since C++11 but is `constexpr` only since C++17, so this accessor
   * follows the same constant-evaluation boundary.
   *
   * @details `M` keeps lookup and the availability check dependent until use.
  */
  template <typename M = meta,
    typename std::enable_if<M::has_reference_access(), int>::type = 0>
  auto
#if defined(__cpp_lib_addressof_constexpr)
    constexpr
#endif
    operator->() const noexcept
    -> decltype(std::addressof(M::ref(_record))) {
    return std::addressof(M::ref(_record));
  }

  /**
   * Assign through member access, including bit-fields.
   *
   * For the bound record `t`, this effectively performs
   * `t.field_name = std::forward<V>(v)`.
   *
   * Requires `has_value_access()` and a writable field for the bound record.
   * Declared `const` fields and non-`mutable` fields of const records are not
   * writable.
   *
   * @details `M` keeps generated assignment lookup and availability checks
   * dependent until use.
   */
  template <typename V,
    typename M = meta,
    typename std::enable_if<M::has_value_access()
        && detail::_is_writable_field<record &, M>::value,
      int>::type = 0>
  void set_value(V &&v) {
    M::set_value(_record, std::forward<V>(v));
  }

  constexpr explicit field_binding_t(record &value): _record(value) {}
};

/// Reflected-scope-only metadata wrapper; `_M` is opaque.
template <typename _M,
#if defined(OMNI_TOOL_RUN)
  // Generated entity metadata is unavailable during instrumentation.
  reflected_entity = std::is_enum<compat::decay_t<_M>>::value
    ? reflected_entity::enumeration
    : reflected_entity::record>
#else
  reflected_entity = _M::entity()>
#endif
struct meta_t;

/// Reflected-scope-only value binding; `T` preserves cv/ref qualification.
template <typename T,
#if defined(OMNI_TOOL_RUN)
  // Generated entity metadata is unavailable during instrumentation.
  reflected_entity = std::is_enum<compat::decay_t<T>>::value
    ? reflected_entity::enumeration
    : reflected_entity::record>
#else
  reflected_entity = detail::_meta<T>::entity()>
#endif
struct binding_t;

/// Reflected-scope-only record metadata; `_M` is opaque.
template <typename _M>
struct meta_t<_M, reflected_entity::record> {
  /// Domain type recovered from generated metadata.
#if defined(OMNI_TOOL_RUN)
  using reflected_type = compat::decay_t<_M>;
#else
  using reflected_type = typename _M::type;
#endif

  /// Identify this wrapper as record metadata.
  static constexpr reflected_entity entity() noexcept {
    return reflected_entity::record;
  }

#if !defined(OMNI_TOOL_RUN)
  static_assert(!std::is_enum<reflected_type>::value, "Type is not a record");
  static_assert(is_reflected<reflected_type>::value,
    "Type was not reflected through reflected_call or its dependency "
    "protocol");
  static_assert(reflected_entity::record == _M::entity(),
    "Inconsistent reflection");

  /**
   * Metadata tuple for visible public fields:
   * `std::tuple<field_meta_t<_FieldMeta>...>`.
   *
   * Hidden and ambiguous inherited fields are omitted.
   */
  using public_fields_t = detail::_all_visible_public_fields_t<_M>;

  /**
   * Reflected record name without namespace qualification.
   *
   * ```cpp
   * namespace app {
   * struct outer { struct item {}; };
   * }
   * omni::meta_for<app::outer::item>::type_name(); // "outer::item"
   * ```
   *
   * Class-template specializations report the primary template name.
   */
  static constexpr const char *type_name() noexcept {
    return _M::type_name();
  }

  /**
   * Reflected record name including enclosing namespaces and records.
   *
   * ```cpp
   * namespace app {
   * struct outer { struct item {}; };
   * }
   * omni::meta_for<app::outer::item>::qualified_type_name();
   * // "app::outer::item"
   * ```
   *
   * Class-template specializations report the primary template name.
   */
  static constexpr const char *qualified_type_name() noexcept {
    return _M::qualified_type_name();
  }

  /**
   * Documentation comment attached to the record declaration.
   *
   * ```cpp
   * /// Account returned to API clients.
   * ///
   * /// The identifier remains stable across requests.
   * struct account {
   *   int id;
   * };
   * ```
   *
   * Returns:
   *
   * ```text
   * Account returned to API clients.
   *
   * The identifier remains stable across requests.
   * ```
   *
   * Comment markers and common indentation are removed. Line breaks and later
   * paragraphs are preserved. Returns an empty string when no documentation
   * comment is attached or generation uses `--no-annotations`.
   */
  static constexpr const char *documentation() noexcept {
    return _M::documentation();
  }

  /// Whether the record directly declares any base class.
  static constexpr bool has_bases() noexcept {
    return _M::has_bases();
  }

  /**
   * Whether `refl::aggregate_into` has a generated implementation.
   *
   * This requires a non-union C++ aggregate with no bases; every data member
   * except unnamed bit-fields must have a name.
   */
  static constexpr bool is_aggregatable() noexcept {
    return _M::is_aggregatable();
  }

  /**
   * Return visible public fields as
   * `std::tuple<field_meta_t<_FieldMeta>...>`.
   *
   * Hidden and ambiguous inherited fields are omitted.
   */
  static constexpr public_fields_t public_fields() noexcept {
    return {};
  }
#endif

  /**
   * Bind a record object to this metadata.
   *
   * The object's unqualified type must match `reflected_type`.
   * Lvalues remain referenced; rvalues are owned by the returned binding.
   *
   * Inside a reflected scope:
   * ```cpp
   * using account_meta = omni::meta_for<account>;
   * account value{};
   * auto referenced = account_meta::bind(value); // record_binding_t<account &>
   * auto owned = account_meta::bind(account{});   // record_binding_t<account>
   * ```
   */
  template <typename U,
    typename std::enable_if<
      std::is_same<compat::decay_t<U>, reflected_type>::value,
      int>::type = 0,
    typename Binding =
      compat::conditional_t<std::is_lvalue_reference<U &&>::value,
        U &&,
        compat::decay_t<U>>>
  static constexpr auto bind(U &&t) noexcept(
    noexcept(binding_t<Binding>{std::forward<U>(t)}))
    -> decltype(binding_t<Binding>{std::forward<U>(t)}) {
    return binding_t<Binding>{std::forward<U>(t)};
  }

  /**
   * Create an owning binding around a default-constructed record.
   *
   * Available when `reflected_type` can be default-constructed and stored by
   * value:
   * ```cpp
   * using account_meta = omni::meta_for<account>;
   * auto owned = account_meta::bind(); // record_binding_t<account>
   * ```
   */
  template <
    // Keep the constraint dependent until `bind()` participates in SFINAE.
    typename U = reflected_type,
    typename std::enable_if<std::is_same<U, reflected_type>::value
        && std::is_default_constructible<U>::value
        && std::is_constructible<U, U &&>::value,
      int>::type = 0>
  static constexpr binding_t<reflected_type> bind() noexcept(
    std::is_nothrow_default_constructible<reflected_type>::value
      && std::is_nothrow_constructible<reflected_type,
        reflected_type &&>::value) {
    return binding_t<reflected_type>{reflected_type {}};
  }

  private:
  // Only reflected entry points may construct metadata wrappers.
  friend struct reflected_call_t;

#if !defined(OMNI_TOOL_RUN)
  template <typename U>
  friend constexpr meta_t<detail::_meta<U>> reflected(type_t<U>) noexcept;
#endif

  // Record bindings use `_public_fields` to bind generated field metadata.
  template <typename, reflected_entity>
  friend struct binding_t;

  constexpr meta_t() noexcept = default;

  // Deduce the field pack from its tuple without a C++14 generic lambda.
  template <typename _T, typename... FieldMeta>
  static constexpr std::tuple<field_binding_t<_T, FieldMeta>...>
    _public_fields(_T &t, std::tuple<field_meta_t<FieldMeta>...>) {
    return std::tuple<field_binding_t<_T, FieldMeta>...>{
      field_binding_t<_T, FieldMeta>{t}...};
  }
};

/// Reflected-scope-only enum metadata; `_M` is opaque.
template <typename _M>
struct meta_t<_M, reflected_entity::enumeration> {
  /// Domain type recovered from generated metadata.
#if defined(OMNI_TOOL_RUN)
  using reflected_type = compat::decay_t<_M>;
#else
  using reflected_type = typename _M::type;
#endif

  /// Identify this wrapper as enum metadata.
  static constexpr reflected_entity entity() noexcept {
    return reflected_entity::enumeration;
  }

#if !defined(OMNI_TOOL_RUN)
  static_assert(std::is_enum<reflected_type>::value, "Type is not an enum");
  static_assert(is_reflected<reflected_type>::value,
    "Type was not reflected through reflected_call or its dependency "
    "protocol");

  static_assert(reflected_entity::enumeration == _M::entity(),
    "Inconsistent reflection");

  /**
   * Reflected enum name without namespace qualification.
   *
   * ```cpp
   * namespace app {
   * struct outer { enum class state { ready }; };
   * }
   * omni::meta_for<app::outer::state>::type_name(); // "outer::state"
   * ```
   */
  static constexpr const char *type_name() noexcept {
    return _M::type_name();
  }

  /**
   * Reflected enum name including enclosing namespaces and records.
   *
   * ```cpp
   * namespace app {
   * struct outer { enum class state { ready }; };
   * }
   * omni::meta_for<app::outer::state>::qualified_type_name();
   * // "app::outer::state"
   * ```
   */
  static constexpr const char *qualified_type_name() noexcept {
    return _M::qualified_type_name();
  }

  /**
   * Documentation comment attached to the enum declaration.
   *
   * ```cpp
   * /// Current account state.
   * ///
   * /// Stored values remain stable across releases.
   * enum class status {
   *   active,
   *   suspended,
   * };
   * ```
   *
   * Returns:
   *
   * ```text
   * Current account state.
   *
   * Stored values remain stable across releases.
   * ```
   *
   * Comment markers and common indentation are removed. Line breaks and later
   * paragraphs are preserved. Returns an empty string when no documentation
   * comment is attached or generation uses `--no-annotations`.
   */
  static constexpr const char *documentation() noexcept {
    return _M::documentation();
  }

  /**
   * Return enumerators in declaration order as `{value, name}` pairs.
   *
   * Given this enum:
   * ```cpp
   * enum class status { draft, active };
   * ```
   * Inside a reflected scope:
   * ```cpp
   * using status_meta = omni::meta_for<status>;
   *
   * constexpr std::array pairs = status_meta::enumerators();
   * for (const std::pair entry : pairs) {
   *   const auto &[value, name] = entry;
   *   std::cout << name << ": " << static_cast<int>(value) << '\n';
   * }
   * // draft: 0
   * // active: 1
   * ```
   */
  static constexpr auto enumerators() noexcept -> decltype(_M::enumerators()) {
    return _M::enumerators();
  }
#endif

  /**
   * Bind an enum value to this metadata.
   *
   * The value's unqualified type must match `reflected_type`.
   * Lvalues remain referenced; rvalues are owned by the returned binding.
   *
   * Inside a reflected scope:
   * ```cpp
   * using status_meta = omni::meta_for<status>;
   * status value = status::active;
   * auto referenced = status_meta::bind(value); // enum_binding_t<status &>
   * auto owned = status_meta::bind(status::active); // enum_binding_t<status>
   * ```
   */
  template <typename U,
    typename std::enable_if<
      std::is_same<compat::decay_t<U>, reflected_type>::value,
      int>::type = 0,
    typename Binding =
      compat::conditional_t<std::is_lvalue_reference<U &&>::value,
        U &&,
        compat::decay_t<U>>>
  static constexpr auto bind(U &&value) noexcept(
    noexcept(binding_t<Binding>{std::forward<U>(value)}))
    -> decltype(binding_t<Binding>{std::forward<U>(value)}) {
    return binding_t<Binding>{std::forward<U>(value)};
  }

  private:
  // Only reflected entry points may construct metadata wrappers.
  friend struct reflected_call_t;

#if !defined(OMNI_TOOL_RUN)
  template <typename U>
  friend constexpr meta_t<detail::_meta<U>> reflected(type_t<U>) noexcept;
#endif

  constexpr meta_t() noexcept = default;
};

/// Reflected-scope-only record metadata; `_M` is opaque.
template <typename _M>
using record_meta_t = meta_t<_M, reflected_entity::record>;

/// Reflected-scope-only enum metadata; `_M` is opaque.
template <typename _M>
using enum_meta_t = meta_t<_M, reflected_entity::enumeration>;

/// Instrumentation type tag; usable without generated metadata.
template <typename T>
struct type_t {};

// Variable templates require C++14.
#if defined(__cpp_variable_templates)
template <typename T>
constexpr type_t<T> type{};
#endif

/**
 * Reflected-scope-only QoL alias for metadata in dependent code.
 *
 * Names the metadata type returned by `reflected(type_t<T>{})`.
 *
 * @warning Do not add reflection specializations. This accessor names only
 * metadata emitted by Omnirefl instrumentation.
 */
template <typename T>
using meta_for =
#if defined(OMNI_TOOL_RUN)
  // Ad hoc: generated metadata does not exist during instrumentation.
  // TODO(high): Handle `meta_for` queries in the tool so this alias has one
  // definition in both passes.
  meta_t<T>;
#else
  meta_t<detail::_meta<T>>;
#endif

/// Reflected-scope-only record binding.
///
/// TODO(high): Decide whether bindings should inherit their metadata wrappers.
///   This requires coordinated pre-C++20 overload and C++20 concept semantics.
template <typename T>
struct binding_t<T, reflected_entity::record> {
  /// Bound record type with cv/ref qualification removed.
  using type = compat::decay_t<T>;

  /// Generated metadata for the bound record.
  using meta = meta_for<type>;

  /// Whether the binding stores its own record instead of a reference.
  using owning = std::integral_constant<bool, !std::is_reference<T>::value>;

  /// Stored record type for owning and non-owning bindings.
  using storage_t = compat::conditional_t<owning::value,
    type, //< own a value
    typename std::remove_reference<T>::type &>; //< hold a reference

  storage_t _record;

  /// Identify this wrapper as a record binding.
  static constexpr reflected_entity entity() noexcept {
    return reflected_entity::record;
  }

#if !defined(OMNI_TOOL_RUN)
  /**
   * Reflected record name without namespace qualification.
   *
   * ```cpp
   * namespace app {
   * struct outer { struct item {}; };
   * }
   * auto item = app::outer::item{};
   * omni::reflected(item).type_name(); // "outer::item"
   * ```
   *
   * Class-template specializations report the primary template name.
   */
  static constexpr const char *type_name() noexcept {
    return meta::type_name();
  }

  /**
   * Reflected record name including enclosing namespaces and records.
   *
   * ```cpp
   * namespace app {
   * struct outer { struct item {}; };
   * }
   * auto item = app::outer::item{};
   * omni::reflected(item).qualified_type_name(); // "app::outer::item"
   * ```
   *
   * Class-template specializations report the primary template name.
   */
  static constexpr const char *qualified_type_name() noexcept {
    return meta::qualified_type_name();
  }

  /**
   * Documentation comment attached to the bound record's declaration.
   *
   * ```cpp
   * /// Account returned to API clients.
   * ///
   * /// The identifier remains stable across requests.
   * struct account {
   *   int id;
   * };
   * ```
   *
   * Returns:
   *
   * ```text
   * Account returned to API clients.
   *
   * The identifier remains stable across requests.
   * ```
   *
   * Comment markers and common indentation are removed. Line breaks and later
   * paragraphs are preserved. Returns an empty string when no documentation
   * comment is attached or generation uses `--no-annotations`.
   */
  static constexpr const char *documentation() noexcept {
    return meta::documentation();
  }
#endif

  /// Return an lvalue reference to the bound record.
  constexpr const storage_t &value() const & noexcept {
    return _record;
  }

#  if defined(__cpp_constexpr) && 201304L <= __cpp_constexpr
  constexpr
#  endif
  /// Return an rvalue reference to the bound record.
  auto value() && noexcept -> decltype(std::move(_record)) {
    return std::move(_record);
  }

  /// Return an rvalue reference through a const record binding.
  constexpr auto value() const && noexcept -> decltype(std::move(_record)) {
    return std::move(_record);
  }

#  if defined(__cpp_constexpr) && 201304L <= __cpp_constexpr
  constexpr
#  endif
  /// Return a direct reference to the bound record.
  storage_t &ref() & noexcept {
    return _record;
  }

  /// Return a direct reference through a const record binding.
  constexpr const storage_t &ref() const & noexcept {
    return _record;
  }

  /**
   * Access the bound record.
   *
   * Non-owning bindings preserve the referenced object's cv-qualification;
   * owning bindings expose their stored value as const.
   */
  constexpr operator const storage_t &() const noexcept {
    return value();
  }

#if !defined(OMNI_TOOL_RUN)
#  if defined(__cpp_constexpr) && 201304L <= __cpp_constexpr
  constexpr
#  endif
  /**
   * Bind visible public fields from a non-const lvalue record binding as
   * `std::tuple<field_binding_t<Record, _FieldMeta>...>`.
   *
   * Hidden and ambiguous inherited fields are omitted.
   *
   * @note This non-const overload is not `constexpr` in C++11 because C++11
   * requires `constexpr` member functions to be const.
   */
  auto public_fields() & -> decltype(meta::_public_fields(
    std::declval<storage_t &>(),
    typename meta::public_fields_t{})) {
    return meta::_public_fields(_record, typename meta::public_fields_t{});
  }

  /**
   * Bind visible public fields from a const lvalue record binding as
   * `std::tuple<field_binding_t<Record, _FieldMeta>...>`.
   *
   * Referenced records keep their own cv-qualification. An owned record is
   * const through this overload. Hidden and ambiguous inherited fields are
   * omitted.
   */
  constexpr auto public_fields() const & -> decltype(meta::_public_fields(
    std::declval<const storage_t &>(),
    typename meta::public_fields_t{})) {
    return meta::_public_fields(_record, typename meta::public_fields_t{});
  }

  /**
   * Bind visible public fields from an rvalue record binding.
   *
   * Deleted to prevent dangling bindings from an owning temporary:
   *
   * ```cpp
   * auto dangling_fields =
   *   omni::reflected(account{}).public_fields(); // rejected
   *
   * auto account_binding = omni::reflected(account{});
   * auto fields = account_binding.public_fields(); // valid
   * ```
   */
  auto public_fields() && -> decltype(meta::_public_fields(
    std::declval<storage_t &>(),
    typename meta::public_fields_t{})) = delete;

  /**
   * Bind visible public fields from a const rvalue record binding.
   *
   * Deleted to prevent dangling bindings from an owning temporary:
   *
   * ```cpp
   * const auto account_binding = omni::reflected(account{});
   * auto dangling_fields =
   *   std::move(account_binding).public_fields(); // rejected
   *
   * auto fields = account_binding.public_fields(); // valid
   * ```
   */
  auto public_fields() const && -> decltype(meta::_public_fields(
    std::declval<const storage_t &>(),
    typename meta::public_fields_t{})) = delete;
#endif

  private:
  // Bindings are constructed only by reflected entry points and their matching
  // metadata factory.
  friend struct reflected_call_t;
  friend meta;

  // non-owning: T is a reference (U& / const U& / U&&)
  template <typename U,
    typename std::enable_if<!owning::value
        && std::is_convertible<U &&, T>::value,
      int>::type = 0>
  constexpr explicit binding_t(U &&u) noexcept: _record(u) {}

  // owning: T is a value type (U / const U)
  template <typename U,
    typename std::enable_if<owning::value
        && std::is_constructible<type, U &&>::value,
      int>::type = 0>
  constexpr explicit binding_t(U &&u) noexcept(
    std::is_nothrow_constructible<type, U &&>::value)
      : _record(std::forward<U>(u)) {}
};

/// Reflected-scope-only enum binding.
///
/// TODO(high): Decide whether bindings should inherit their metadata wrappers.
///   This requires coordinated pre-C++20 overload and C++20 concept semantics.
template <typename T>
struct binding_t<T, reflected_entity::enumeration> {
  /// Bound enum type with cv/ref qualification removed.
  using type = compat::decay_t<T>;

  /// Generated metadata for the bound enum.
  using meta = meta_for<type>;

  /// Whether the binding stores its own enum value instead of a reference.
  using owning = std::integral_constant<bool, !std::is_reference<T>::value>;

  /// Stored enum type for owning and non-owning bindings.
  using storage_t = compat::conditional_t<owning::value,
    type, //< own a value
    typename std::remove_reference<T>::type &>; //< hold a reference

  storage_t _enum_value;

  /// Identify this wrapper as an enum binding.
  static constexpr reflected_entity entity() noexcept {
    return reflected_entity::enumeration;
  }

#if !defined(OMNI_TOOL_RUN)
  /**
   * Reflected enum name without namespace qualification.
   *
   * ```cpp
   * namespace app {
   * struct outer { enum class state { ready }; };
   * }
   * auto state = app::outer::state::ready;
   * omni::reflected(state).type_name(); // "outer::state"
   * ```
   */
  static constexpr const char *type_name() noexcept {
    return meta::type_name();
  }

  /**
   * Reflected enum name including enclosing namespaces and records.
   *
   * ```cpp
   * namespace app {
   * struct outer { enum class state { ready }; };
   * }
   * auto state = app::outer::state::ready;
   * omni::reflected(state).qualified_type_name(); // "app::outer::state"
   * ```
   */
  static constexpr const char *qualified_type_name() noexcept {
    return meta::qualified_type_name();
  }

  /**
   * Documentation comment attached to the bound enum's declaration.
   *
   * ```cpp
   * /// Current account state.
   * ///
   * /// Stored values remain stable across releases.
   * enum class status {
   *   active,
   *   suspended,
   * };
   * ```
   *
   * Returns:
   *
   * ```text
   * Current account state.
   *
   * Stored values remain stable across releases.
   * ```
   *
   * Comment markers and common indentation are removed. Line breaks and later
   * paragraphs are preserved. Returns an empty string when no documentation
   * comment is attached or generation uses `--no-annotations`.
   */
  static constexpr const char *documentation() noexcept {
    return meta::documentation();
  }
#endif

  /// Return an lvalue reference to the bound enum value.
  constexpr const storage_t &value() const & noexcept {
    return _enum_value;
  }

#  if defined(__cpp_constexpr) && 201304L <= __cpp_constexpr
  constexpr
#  endif
  /// Return an rvalue reference to the bound enum value.
  auto value() && noexcept -> decltype(std::move(_enum_value)) {
    return std::move(_enum_value);
  }

  /// Return an rvalue reference through a const enum binding.
  constexpr auto value() const && noexcept -> decltype(std::move(_enum_value)) {
    return std::move(_enum_value);
  }

#  if defined(__cpp_constexpr) && 201304L <= __cpp_constexpr
  constexpr
#  endif
  /// Return a direct reference to the bound enum value.
  storage_t &ref() & noexcept {
    return _enum_value;
  }

  /// Return a direct reference through a const enum binding.
  constexpr const storage_t &ref() const & noexcept {
    return _enum_value;
  }

  /**
   * Access the bound enum value.
   *
   * Non-owning bindings preserve the referenced value's cv-qualification;
   * owning bindings expose their stored value as const.
   */
  constexpr operator const storage_t &() const noexcept {
    return value();
  }

#if !defined(OMNI_TOOL_RUN)
  /**
   * Return enumerators in declaration order as `{value, name}` pairs.
   *
   * Given this enum:
   * ```cpp
   * enum class status { draft, active };
   * ```
   * Inside a reflected scope:
   * ```cpp
   * constexpr auto binding = omni::reflected(status::active);
   *
   * constexpr std::array pairs = binding.enumerators();
   * for (const std::pair entry : pairs) {
   *   const auto &[value, name] = entry;
   *   std::cout << name << ": " << static_cast<int>(value) << '\n';
   * }
   * // draft: 0
   * // active: 1
   * ```
   */
  static constexpr auto enumerators() noexcept
    -> decltype(meta::enumerators()) {
    return meta::enumerators();
  }
#endif

  private:
  // Bindings are constructed only by reflected entry points and their matching
  // metadata factory.
  friend struct reflected_call_t;
  friend meta;

  // non-owning: T is a reference (U& / const U& / U&&)
  template <typename U,
    typename std::enable_if<!owning::value
        && std::is_convertible<U &&, T>::value,
      int>::type = 0>
  constexpr explicit binding_t(U &&u) noexcept: _enum_value(u) {}

  // owning: T is a value type (U / const U)
  template <typename U,
    typename std::enable_if<owning::value
        && std::is_constructible<type, U &&>::value,
      int>::type = 0>
  constexpr explicit binding_t(U &&u) noexcept(
    std::is_nothrow_constructible<type, U &&>::value)
      : _enum_value(std::forward<U>(u)) {}
};

/// Reflected-scope-only record binding.
template <typename T>
using record_binding_t = binding_t<T, reflected_entity::record>;

/// Reflected-scope-only enum binding.
template <typename T>
using enum_binding_t = binding_t<T, reflected_entity::enumeration>;

#if defined(__cpp_concepts)
/// Whether `T` is a record or enum metadata wrapper.
template <typename T>
concept meta = traits::is<meta_t, T>();

/// Whether `T` is a record or enum value binding.
template <typename T>
concept binding = traits::is<binding_t, T>();

/// Whether `T` is field metadata.
template <typename T>
concept field_meta = traits::is<field_meta_t, T>();

/// Whether `T` binds field metadata to a record object.
template <typename T>
concept field_binding = traits::is<field_binding_t, T>();

/// Whether `T` is record metadata.
template <typename T>
concept record_meta =
  meta<T> && compat::remove_cvref_t<T>::entity() == reflected_entity::record;

/// Whether `T` is enum metadata.
template <typename T>
concept enum_meta = meta<T>
  && compat::remove_cvref_t<T>::entity() == reflected_entity::enumeration;

/// Whether `T` binds a reflected record object.
template <typename T>
concept record_binding =
  binding<T> && compat::remove_cvref_t<T>::entity() == reflected_entity::record;

/// Whether `T` binds a reflected enum value.
template <typename T>
concept enum_binding = binding<T>
  && compat::remove_cvref_t<T>::entity() == reflected_entity::enumeration;
#endif

#if defined(OMNI_TOOL_RUN)
// Dependent reflected queries must be declared while the visitor body is
// parsed. Their definitions require generated metadata and remain unavailable.
template <typename T>
constexpr meta_for<T> reflected(type_t<T>) noexcept;

template <typename T,
  typename std::enable_if<!traits::is<type_t, compat::decay_t<T>>(),
    int>::type = 0>
constexpr auto reflected(T &&) noexcept
  -> binding_t<compat::conditional_t<
    std::is_lvalue_reference<T &&>::value, T &&, compat::decay_t<T>>>;
#else
/**
 * Access generated metadata for a reflected type tag.
 *
 * Query `is_reflected<T>` first when metadata availability is conditional.
 *
 * TODO(high): Consider a direct unavailable-metadata diagnostic only if it
 *   can avoid cascading errors from the incomplete generated specialization.
 */
template <typename T>
constexpr meta_for<T> reflected(type_t<T>) noexcept {
  return {};
}

/**
 * Bind a value to its generated metadata.
 *
 * Type tags select the metadata overload above.
 *
 * Lvalues remain referenced; rvalues are owned by the returned binding.
 *
 * Query `is_reflected<T>` first when metadata availability is conditional.
 */
template <typename T,
  typename std::enable_if<!traits::is<type_t, compat::decay_t<T>>(),
    int>::type = 0>
constexpr auto reflected(T &&t) noexcept(
  noexcept(meta_for<T>::bind(std::forward<T>(t))))
  -> decltype(meta_for<T>::bind(std::forward<T>(t))) {
  return meta_for<T>::bind(std::forward<T>(t));
}
#endif

#if defined(OMNI_TOOL_RUN)
// Ad hoc: this definition must remain visible when a callable has no linkage,
// but instrumentation never evaluates it and therefore intentionally returns
// nothing. The callable's declared result is still required by surrounding
// source expressions.
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

// TODO: Replace this callable implementation type with overloaded
// `reflected_call` functions.
struct reflected_call_t {
  private:
  // These factories need this class's friendship to construct public wrappers.
  template <typename T>
  static constexpr meta_for<T> _reflect_arg(type_t<T>) noexcept {
    return {};
  }

  template <typename T>
  static constexpr binding_t<T &&> _reflect_arg(T &&t) noexcept(
    noexcept(binding_t<T &&>{std::forward<T>(t)})) {
    return binding_t<T &&>{std::forward<T>(t)};
  }

  public:
  // TODO: Decide whether `reflected_call` should support constexpr
  // evaluation. It currently cannot be constexpr as a whole: during the tool
  // run the call is parsed and matched, but intentionally does not evaluate the
  // callable before generated reflection exists.
  /// Invoke the callable with reflected wrappers for every argument.
  template <typename Impl, typename... Args>
  auto operator()(Impl &&impl, Args &&...args) const
#if defined(OMNI_TOOL_RUN)
    // Resolve only the callable declaration; its body is the reflected scope.
    -> decltype(std::declval<Impl &&>()(
      _reflect_arg(std::declval<Args &&>())...)) {
#elif !defined(OMNI_INCLUDED_GENERATED_REFLECTION_HEADER)
    // IDE parsing cannot name the result before generation.
    -> detail::_ungenerated_result {
#else
    // Normal compilation invokes the callable with generated wrappers.
    -> decltype(std::declval<Impl &&>()(
      _reflect_arg(std::declval<Args &&>())...)) {
#endif
#if defined(OMNI_ENABLE_INDEX_MODE) && OMNI_ENABLE_INDEX_MODE
    // C++11 pack expansion instantiates registrations in argument order.
    int registered[] = {0,
      ((void)detail::_reflected_indexed_type<
         typename detail::reflected_arg_type<compat::decay_t<Args>>::type>{},
        0)...};
    (void)registered;
#else
    (void)sizeof...(args);
#endif

    (void)impl;

    // Instrumentation parses and matches calls but does not evaluate them.
#if defined(OMNI_TOOL_RUN)
#elif !defined(OMNI_INCLUDED_GENERATED_REFLECTION_HEADER)
    return {};
#else
    return std::forward<Impl>(impl)(_reflect_arg(std::forward<Args>(args))...);
#endif
  }
};

#if defined(OMNI_TOOL_RUN)
#  if defined(__clang__)
#    pragma clang diagnostic pop
#  elif defined(__GNUC__)
#    pragma GCC diagnostic pop
#  elif defined(_MSC_VER)
#    pragma warning(pop)
#  endif
#endif

/**
 * Invoke a callable with reflection metadata or bindings for the supplied
 * arguments.
 *
 * The callable itself is not reflected. Each argument must identify a named
 * record (struct, class, or union) or a named enum. The type must be visible
 * from global scope, and its full definition must be visible at the call. Pass
 * a value when the callable needs an object, or `type_t<T>` when it needs
 * metadata only. The caller is responsible for sanitizing other inputs: invoke
 * `reflected_call` in a loop for arrays and containers, dereference pointers,
 * visit the active variant alternative, and handle fundamental values without
 * reflection. Checks for unsupported arguments are best effort.
 *
 * Arguments keep their order and are passed to the callable as:
 * ```cpp
 * type_t<T>          -> meta_for<T>
 * struct/class/union -> record_binding_t<T &&>
 * enum               -> enum_binding_t<T &&>
 * ```
 * Value bindings preserve the argument's cv-qualification and value category.
 *
 * The callable must be a generic lambda or a type with a templated
 * `operator()`. Its return type must be known from the declaration without
 * instantiating the function body. A generic lambda therefore needs an explicit
 * trailing return type, including `-> void`. Reflection wrappers and concepts
 * may constrain the declaration. Before C++20, use `record_meta_t<_Meta>` and
 * `record_binding_t<Record>` as the parameter types of a templated
 * `operator()`.
 *
 * The reflected scope starts in the function body and includes calls made from
 * it. Metadata for the argument roots and their declared dependencies is
 * available throughout that scope. `reflected_call` returns the callable's
 * result.
 *
 * ```cpp
 * struct account {
 *   int id;
 * };
 *
 * struct description {
 *   std::string name;
 *   std::size_t fields;
 * };
 *
 * // Reflection wrapper types and concepts are valid in this declaration.
 * const auto describe =
 *   []<omni::record_meta RecordMeta, omni::record_binding RecordBinding>(
 *     RecordMeta, RecordBinding record)
 *     // Explicit return avoids body instantiation for deduction.
 *     -> description {
 *     // Reflected scope: metadata is available here and in calls made from
 *     // this body.
 *     return {
 *       RecordMeta::qualified_type_name(),
 *       std::tuple_size<decltype(record.public_fields())>::value,
 *     };
 *   };
 *
 * account value{};
 * const auto result = omni::reflected_call(describe,
 *   omni::type_t<account>{}, // `meta_for<account>`
 *   value);                  // `record_binding_t<account &>`
 * // result.name == "account"
 * // result.fields == 1
 * ```
 *
 * @warning Calling `reflected_call` again from the callable's `operator()` is
 * not supported.
 */
constexpr reflected_call_t reflected_call{};

/// Experimental utilities callable only within a reflected scope.
namespace refl {

/// Reflected-scope-only query for generated aggregate construction.
template <typename T, typename = void>
struct is_aggregatable: std::false_type {};

template <typename T>
struct is_aggregatable<T,
  compat::void_t<decltype(omni::meta_for<T>::is_aggregatable())>>:
    std::integral_constant<bool, omni::meta_for<T>::is_aggregatable()> {};

/**
 * Field-lookup customization point used by `aggregate_into`.
 *
 * The tuple overload finds the first same-named field, moves from its
 * `.value()`, and constructs `Target`. Missing or incompatible fields are
 * compile-time errors.
 *
 * REFACTORME: Decide whether this provisional tuple-only lookup remains the
 *   aggregate-construction customization point before stabilizing the
 *   interface.
 */
template <typename TargetField, typename... Field>
typename TargetField::type get(std::tuple<Field...> &fields) {
  using target = typename TargetField::type;

  return detail::_get_t<0,
    TargetField,
    std::tuple<Field...>,
    target,
    sizeof...(Field) == 0>::from(fields);
}

/**
 * For each supported `Record`, the generator emits an implementation
 * equivalent to:
 *
 * ```cpp
 * return Record{
 *   .first_field = omni::refl::get<omni::meta_for<Record>::first_field>(fields),
 *   .second_field = omni::refl::get<omni::meta_for<Record>::second_field>(fields),
 * };
 * ```
 *
 * `get` is the field-lookup customization point. Conversion is shallow; before
 * C++20 the same calls are emitted as positional initializers. Source-only
 * fields are ignored; missing or incompatible destination fields fail during
 * compilation. Unions and aggregates with bases are unsupported.
 *
 * @warning The generated `aggregate_into`/`get` protocol is experimental and
 * may change.
 */
template <typename T, typename Fields>
T aggregate_into(Fields &&fields) {
  return detail::aggregate_into_t<T>::from(std::forward<Fields>(fields));
}

} // namespace refl
} // namespace omni
