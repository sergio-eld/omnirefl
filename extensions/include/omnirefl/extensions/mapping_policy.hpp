#pragma once

/** Record, enum, and range mapping policies shared by protocol extensions.
 * Policies contain field selection, aliases, and value-preserving validation;
 * they do not depend on generated metadata or a document backend.
 * See docs/CONVENTIONS.md, functional data-member updates. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/impl/tdm_mapping_policy.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/traits.hpp>

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace omni {
namespace mapping {

/** Record field selection, aliases, validation, and output null policy.
 * Predicates are stateless, default-constructible, and return Boolean traits.
 * Validators preserve successfully checked values; Error is interpreted by
 * the consuming extension. Type-carried keys such as OMNI_META_STRING("age")
 * let an extension select a field before instantiating a generic validator.
 *
 * Field and alias names refer to the original C++ declaration, independently
 * of a protocol's representation. Aliases must not overlap canonical names or
 * other aliases. Deserialization additionally excludes static members.
 */
template <typename FilterFields = decltype(
    omni::fn::is_non_static && omni::fn::is_non_functional),
  typename AliasesByName = std::tuple<>,
  typename ValidateFields = std::tuple<>,
  typename Finalize = omni::fn::identity,
  typename KeepNulls = compat::monostate>
struct record_policy_t:
    detail::record_policy_updates<FilterFields, AliasesByName,
      ValidateFields, Finalize, KeepNulls> {
  using tdm_provider = detail::record_policy_updates<FilterFields,
    AliasesByName, ValidateFields, Finalize, KeepNulls>;

  // filter_fields :: field_meta_like -> bool_constant
  FilterFields filter_fields OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_filter_fields;

  // aliases_by_name :: {field_name: [alias]}
  AliasesByName aliases_by_name OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_aliases_by_name;

  // validate_fields :: {field_name: [validator]}
  // validator :: T -> expected<T, Error> | with_diagnostics<T>
  ValidateFields validate_fields OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_validate_fields;

  // finalize :: Record -> expected<Record, Error> | with_diagnostics<Record>
  // The default identity performs no validation.
  Finalize finalize OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_finalize;

  /** Output policy for empty optional fields; ignored by input mappings.
   * `compat::monostate` inherits the output configuration. A `bool` or
   * `compat::bool_constant` overrides it: true keeps nulls, false omits. */
  KeepNulls keep_nulls OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_keep_nulls;

  static_assert(std::is_same<compat::monostate, KeepNulls>::value
      || std::is_same<bool, KeepNulls>::value
      || std::is_same<std::false_type, KeepNulls>::value
      || std::is_same<std::true_type, KeepNulls>::value,
    "record_policy_t keep_nulls must be compat::monostate, bool, "
    "or compat::bool_constant");

// Before C++17, aggregate bases need constructors for constexpr tdm_ updates.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr record_policy_t()
    : filter_fields{}, aliases_by_name{}, validate_fields{}, finalize{},
      keep_nulls{} {}

private:
  template <typename, typename, typename, typename, typename>
  friend struct detail::record_policy_updates;

  template <typename Filter, typename Aliases, typename Validators,
    typename Finish, typename Nulls>
  constexpr record_policy_t(Filter &&filter, Aliases &&aliases,
    Validators &&validators, Finish &&finish, Nulls &&nulls)
    : filter_fields(std::forward<Filter>(filter)),
      aliases_by_name(std::forward<Aliases>(aliases)),
      validate_fields(std::forward<Validators>(validators)),
      finalize(std::forward<Finish>(finish)),
      keep_nulls(std::forward<Nulls>(nulls)) {}
#endif
};

/** Enum names and their accepted aliases.
 * aliases_by_name :: {enumerator_name: [alias]} */
template <typename AliasesByName = std::tuple<>>
struct enum_policy_t:
    detail::tdm_for_enum_policy<AliasesByName> {
  using tdm_provider = detail::tdm_for_enum_policy<AliasesByName>;

  AliasesByName aliases_by_name OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_aliases_by_name;

// Ad hoc: classes with bases are not aggregates before C++17. These
// constructors preserve field initialization in the older standards.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr enum_policy_t() = default;

  OMNI_CONSTRAINED_TEMPLATE((typename Value = AliasesByName),
    (std::is_constructible<AliasesByName, Value &&>::value))
  constexpr enum_policy_t(Value &&value)
    : aliases_by_name(std::forward<Value>(value)) {}
#endif
};

/** Output policy for empty optional elements or mapped values.
 * Input mappings ignore this policy. */
template <typename KeepNulls = compat::monostate>
struct range_policy_t:
    detail::tdm_for_range_policy<KeepNulls> {
  using tdm_provider = detail::tdm_for_range_policy<KeepNulls>;

  /** `compat::monostate` inherits the output configuration. A `bool` or
   * `compat::bool_constant` overrides it: true keeps nulls, false omits. */
  KeepNulls keep_nulls OMNI_CPP14_DEFAULT_INIT();
  using tdm_provider::tdm_keep_nulls;

  static_assert(std::is_same<compat::monostate, KeepNulls>::value
      || std::is_same<bool, KeepNulls>::value
      || std::is_same<std::false_type, KeepNulls>::value
      || std::is_same<std::true_type, KeepNulls>::value,
    "range_policy_t keep_nulls must be compat::monostate, bool, "
    "or compat::bool_constant");

// Ad hoc: aggregate bases need constructors before C++17.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr range_policy_t() : keep_nulls{} {}

  OMNI_CONSTRAINED_TEMPLATE((typename Value = KeepNulls),
    (std::is_constructible<KeepNulls, Value &&>::value))
  constexpr range_policy_t(Value &&value)
    : keep_nulls(std::forward<Value>(value)) {}
#endif
};

/** Whether T specializes record_policy_t, ignoring cv/ref qualifiers. */
template <typename T>
struct is_record_policy:
    compat::bool_constant<
      traits::is_specialization_of<record_policy_t, T>()> {};

/** Whether T specializes enum_policy_t, ignoring cv/ref qualifiers. */
template <typename T>
struct is_enum_policy:
    compat::bool_constant<
      traits::is_specialization_of<enum_policy_t, T>()> {};

/** Whether T specializes range_policy_t, ignoring cv/ref qualifiers. */
template <typename T>
struct is_range_policy:
    compat::bool_constant<
      traits::is_specialization_of<range_policy_t, T>()> {};

// C++14 adds variable templates for trait values.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <typename T>
constexpr bool is_record_policy_v = is_record_policy<T>::value;

template <typename T>
constexpr bool is_enum_policy_v = is_enum_policy<T>::value;

template <typename T>
constexpr bool is_range_policy_v = is_range_policy<T>::value;
#endif

// C++20 concepts expose the same policy contracts.
#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename T>
concept record_policy_like = is_record_policy<T>::value;

template <typename T>
concept enum_policy_like = is_enum_policy<T>::value;

template <typename T>
concept range_policy_like = is_range_policy<T>::value;
#endif

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr record_policy_t<> record_policy{};
inline constexpr enum_policy_t<> enum_policy{};
inline constexpr range_policy_t<> range_policy{};
#else
static constexpr const record_policy_t<> &record_policy =
  compat::detail::shared_state<record_policy_t<>>::value;
static constexpr const enum_policy_t<> &enum_policy =
  compat::detail::shared_state<enum_policy_t<>>::value;
static constexpr const range_policy_t<> &range_policy =
  compat::detail::shared_state<range_policy_t<>>::value;
#endif

/** (QoL) Canonical-name groups, each followed by its aliases.
 * aliases_dict :: (group, ...) -> {name: [alias]}
 * Borrowed names must outlive uses of the policy. */
template <typename... Groups>
constexpr auto aliases_dict(Groups &&...groups)
  -> std::tuple<compat::decay_t<Groups>...> {
  return {std::forward<Groups>(groups)...};
}

namespace detail {

template <typename Group>
compat::optional<compat::string_view> group_alias(const Group &,
  compat::string_view, compat::index_sequence<>) {
  return compat::nullopt;
}

template <typename Group, std::size_t... Index>
compat::optional<compat::string_view> group_alias(const Group &group,
  compat::string_view alias, compat::index_sequence<Index...>) {
  const bool matches[] = {false,
    (compat::string_view{std::get<Index + 1>(group)} == alias)...};
  for (const bool match : matches)
    if (match)
      return compat::string_view{std::get<0>(group)};

  return compat::nullopt;
}

template <typename Aliases>
auto resolve_alias(const Aliases &aliases, compat::string_view alias, int)
  -> decltype(compat::invoke(aliases, alias)) {
  return compat::invoke(aliases, alias);
}

inline compat::optional<compat::string_view> resolve_groups(
  const std::tuple<> &, compat::string_view, compat::index_sequence<>) {
  return compat::nullopt;
}

template <typename Aliases, std::size_t... Index>
compat::optional<compat::string_view> resolve_groups(const Aliases &aliases,
  compat::string_view alias, compat::index_sequence<Index...>) {
  const compat::optional<compat::string_view> matches[] = {
    compat::nullopt,
    group_alias(std::get<Index>(aliases), alias,
      compat::make_index_sequence<std::tuple_size<
        compat::remove_cvref_t<decltype(std::get<Index>(aliases))>>::value
          - 1>{})...};
  for (const auto &match : matches)
    if (match)
      return match;

  return compat::nullopt;
}

template <typename Aliases>
compat::optional<compat::string_view> resolve_alias(const Aliases &aliases,
  compat::string_view alias, long) {
  return resolve_groups(aliases, alias,
    compat::make_index_sequence<std::tuple_size<Aliases>::value>{});
}

} // namespace detail

/** Alias-to-canonical-name lookup.
 * Opaque string_view -> optional<string_view> resolvers are also accepted;
 * their configuration cannot be inspected by schema generation. */
template <typename Aliases>
auto resolve_alias(const Aliases &aliases, compat::string_view alias)
  -> decltype(detail::resolve_alias(aliases, alias, 0)) {
  return detail::resolve_alias(aliases, alias, 0);
}

namespace detail {

struct policy_name {
  compat::string_view spelling;
  bool is_canonical;
};

struct alias_group_names {
  template <typename Name, typename... Aliases>
  OMNI_CPP14_CONSTEXPR auto operator()(const Name &name,
    const Aliases &...aliases) const
    -> std::array<policy_name, 1 + sizeof...(Aliases)> {
    return {{{compat::string_view{name}, true},
      {compat::string_view{aliases}, false}...}};
  }
};

template <typename Names>
OMNI_CPP14_CONSTEXPR bool contains_name(const Names &names,
  compat::string_view name) {
  for (const auto &candidate : names)
    if (compat::string_view{candidate} == name)
      return true;

  return false;
}

struct collect_policy_names {
  template <typename... Entries>
  constexpr auto operator()(const Entries &...entries) const
    -> std::array<policy_name, sizeof...(Entries)> {
    return {{entries...}};
  }
};

template <typename Names, typename Groups, std::size_t... Index>
OMNI_CPP14_CONSTEXPR bool valid_alias_groups(const Names &names,
  const Groups &groups, compat::index_sequence<Index...>) {
  const auto entries = compat::apply(collect_policy_names{}, std::tuple_cat(
    compat::apply(alias_group_names{}, std::get<Index>(groups))...));
  // Fixed-size arrays keep validation allocation-free.
  const auto names_valid = std::array<bool, sizeof...(Index)>{{
    contains_name(names, compat::string_view{
      std::get<0>(std::get<Index>(groups))})...}};
  for (const bool valid : names_valid)
    if (!valid)
      return false;

  for (std::size_t index = 0; index < entries.size(); ++index) {
    if (!entries[index].is_canonical
      && contains_name(names, entries[index].spelling))
      return false;
    for (std::size_t previous = 0; previous < index; ++previous)
      if (entries[index].spelling == entries[previous].spelling)
        return false;
  }
  return true;
}

template <typename Names, typename Groups, std::size_t... Index>
OMNI_CPP14_CONSTEXPR bool valid_field_groups(const Names &names,
  const Groups &groups, compat::index_sequence<Index...>) {
  const std::array<compat::string_view, sizeof...(Index)> keys{{
    compat::string_view{std::get<0>(std::get<Index>(groups))}...}};
  for (std::size_t index = 0; index < keys.size(); ++index) {
    if (!contains_name(names, keys[index]))
      return false;
    for (std::size_t previous = 0; previous < index; ++previous)
      if (keys[index] == keys[previous])
        return false;
  }
  return true;
}

} // namespace detail

/** Canonical-name existence and disjoint aliases against known names.
 * Opaque callable resolvers cannot be inspected and retain responsibility for
 * their returned names. Tuple-like dictionaries are fully checked. */
OMNI_CONSTRAINED_TEMPLATE((typename Names, typename Aliases),
  (!compat::is_invocable<const Aliases &, compat::string_view>::value))
OMNI_CPP14_CONSTEXPR bool valid_aliases(const Names &names,
  const Aliases &aliases) {
  return detail::valid_alias_groups(names, aliases,
    compat::make_index_sequence<std::tuple_size<Aliases>::value>{});
}

/** Opaque resolvers retain responsibility for valid, unambiguous aliases. */
OMNI_CONSTRAINED_TEMPLATE((typename Names, typename Aliases),
  (compat::is_invocable<const Aliases &, compat::string_view>::value))
constexpr bool valid_aliases(const Names &, const Aliases &) { return true; }

/** Field-name and alias validation for a record policy. */
template <typename Names, typename Policy>
OMNI_CPP14_CONSTEXPR bool valid_record_policy(const Names &names,
  const Policy &policy) {
  return valid_aliases(names, policy.aliases_by_name)
    && detail::valid_field_groups(names, policy.validate_fields,
      compat::make_index_sequence<std::tuple_size<
        decltype(policy.validate_fields)>::value>{});
}

} // namespace mapping
} // namespace omni

// TODO: [medium] Explore lazy dependent-field validation only if it offers
// practical benefits over record finalization; dependency ordering and sibling
// branch dependencies could substantially complicate the interface.

// TODO: [medium] Consider direct-value tdm_ overloads. Unary callable values
// can also be updaters: passing fn::identity{} to tdm_finalize could retain
// the old finalizer or replace it
// with the no-op finalizer. Resolve that interpretation before adding
// overloads.
