#pragma once

/** Record and enum mapping properties shared by protocol extensions.
 * Strategies contain field selection, aliases, and value-preserving validation;
 * they do not depend on generated metadata or a document backend.
 * See docs/CONVENTIONS.md, functional data-member updates. */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/functional.hpp>

#include <array>
#include <cstddef>
#include <tuple>
#include <utility>

namespace omni {
namespace mapping {
namespace detail {

template <typename FilterFields, typename AliasesByName,
  typename ValidateFields, typename Finalize>
struct record_strategy_updates;

} // namespace detail

/** Record field selection, aliases, and validation.
 * Predicates are stateless, default-constructible, and return Boolean traits.
 * Validators preserve successfully checked values; Error is interpreted by
 * the consuming extension.
 *
 * Field and alias names refer to the original C++ declaration, independently
 * of a protocol's representation. Aliases must not overlap canonical names or
 * other aliases. Deserialization additionally excludes static members.
 */
template <typename FilterFields = decltype(
    omni::fn::is_non_static && omni::fn::is_non_functional),
  typename AliasesByName = std::tuple<>,
  typename ValidateFields = std::tuple<>,
  typename Finalize = omni::fn::identity>
struct record_strategy_t:
    detail::record_strategy_updates<FilterFields, AliasesByName,
      ValidateFields, Finalize> {
  // filter_fields :: field_meta_like -> bool_constant
  FilterFields filter_fields OMNI_CPP14_DEFAULT_INIT();

  // aliases_by_name :: {field_name: [alias]}
  AliasesByName aliases_by_name OMNI_CPP14_DEFAULT_INIT();

  // validate_fields :: {field_name: [validator]}
  // validator :: T -> expected<T, Error> | with_diagnostics<T>
  ValidateFields validate_fields OMNI_CPP14_DEFAULT_INIT();

  // finalize :: Record -> expected<Record, Error> | with_diagnostics<Record>
  // The default identity performs no validation.
  Finalize finalize OMNI_CPP14_DEFAULT_INIT();

// Before C++17, aggregate bases need constructors for constexpr tdm_ updates.
#if !defined(__cpp_aggregate_bases) || 201603L > __cpp_aggregate_bases
  constexpr record_strategy_t()
    : filter_fields{}, aliases_by_name{}, validate_fields{}, finalize{} {}

private:
  template <typename, typename, typename, typename>
  friend struct detail::record_strategy_updates;

  constexpr record_strategy_t(FilterFields filter, AliasesByName aliases,
    ValidateFields validators, Finalize finish)
    : filter_fields(std::move(filter)), aliases_by_name(std::move(aliases)),
      validate_fields(std::move(validators)), finalize(std::move(finish)) {}
#endif
};

namespace detail {

template <typename FilterFields, typename AliasesByName,
  typename ValidateFields, typename Finalize>
struct record_strategy_updates {
  /** (QoL) Functional update of .filter_fields; other members are
   * copied from the const strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const FilterFields &>::value))
  constexpr auto tdm_filter_fields(F &&f) const &
    -> record_strategy_t<compat::decay_t<
      compat::invoke_result_t<F &&, const FilterFields &>>,
      AliasesByName,
      ValidateFields,
      Finalize> {
    return make_strategy(
      compat::invoke(std::forward<F>(f), this->derived().filter_fields),
      this->derived().aliases_by_name,
      this->derived().validate_fields,
      this->derived().finalize);
  }

  /** (QoL) Functional update of .filter_fields; other members are
   * moved from the consumed strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, FilterFields &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_filter_fields(F &&f) &&
    -> record_strategy_t<compat::decay_t<
      compat::invoke_result_t<F &&, FilterFields &&>>,
      AliasesByName,
      ValidateFields,
      Finalize> {
    return make_strategy(
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().filter_fields)),
      std::move(this->derived().aliases_by_name),
      std::move(this->derived().validate_fields),
      std::move(this->derived().finalize));
  }

  /** (QoL) Functional update of .aliases_by_name; other members are
   * copied from the const strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const AliasesByName &>::value))
  constexpr auto tdm_aliases_by_name(F &&f) const &
    -> record_strategy_t<FilterFields,
      compat::decay_t<compat::invoke_result_t<F &&, const AliasesByName &>>,
      ValidateFields,
      Finalize> {
    return make_strategy(
      this->derived().filter_fields,
      compat::invoke(std::forward<F>(f), this->derived().aliases_by_name),
      this->derived().validate_fields,
      this->derived().finalize);
  }

  /** (QoL) Functional update of .aliases_by_name; other members are
   * moved from the consumed strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, AliasesByName &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_aliases_by_name(F &&f) &&
    -> record_strategy_t<FilterFields,
      compat::decay_t<compat::invoke_result_t<F &&, AliasesByName &&>>,
      ValidateFields,
      Finalize> {
    return make_strategy(
      std::move(this->derived().filter_fields),
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().aliases_by_name)),
      std::move(this->derived().validate_fields),
      std::move(this->derived().finalize));
  }

  /** (QoL) Functional update of .validate_fields; other members are
   * copied from the const strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const ValidateFields &>::value))
  constexpr auto tdm_validate_fields(F &&f) const &
    -> record_strategy_t<FilterFields,
      AliasesByName,
      compat::decay_t<compat::invoke_result_t<F &&, const ValidateFields &>>,
      Finalize> {
    return make_strategy(
      this->derived().filter_fields,
      this->derived().aliases_by_name,
      compat::invoke(std::forward<F>(f), this->derived().validate_fields),
      this->derived().finalize);
  }

  /** (QoL) Functional update of .validate_fields; other members are
   * moved from the consumed strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, ValidateFields &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_validate_fields(F &&f) &&
    -> record_strategy_t<FilterFields,
      AliasesByName,
      compat::decay_t<compat::invoke_result_t<F &&, ValidateFields &&>>,
      Finalize> {
    return make_strategy(
      std::move(this->derived().filter_fields),
      std::move(this->derived().aliases_by_name),
      compat::invoke(std::forward<F>(f),
        std::move(this->derived().validate_fields)),
      std::move(this->derived().finalize));
  }

  /** (QoL) Functional update of .finalize; other members are
   * copied from the const strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const Finalize &>::value))
  constexpr auto tdm_finalize(F &&f) const &
    -> record_strategy_t<FilterFields,
      AliasesByName,
      ValidateFields,
      compat::decay_t<compat::invoke_result_t<F &&, const Finalize &>>> {
    return make_strategy(
      this->derived().filter_fields,
      this->derived().aliases_by_name,
      this->derived().validate_fields,
      compat::invoke(std::forward<F>(f), this->derived().finalize));
  }

  /** (QoL) Functional update of .finalize; other members are
   * moved from the consumed strategy. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, Finalize &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_finalize(F &&f) &&
    -> record_strategy_t<FilterFields,
      AliasesByName,
      ValidateFields,
      compat::decay_t<compat::invoke_result_t<F &&, Finalize &&>>> {
    return make_strategy(
      std::move(this->derived().filter_fields),
      std::move(this->derived().aliases_by_name),
      std::move(this->derived().validate_fields),
      compat::invoke(std::forward<F>(f), std::move(this->derived().finalize)));
  }

private:
  constexpr const record_strategy_t<FilterFields, AliasesByName,
    ValidateFields, Finalize> &derived() const & {
    return static_cast<const record_strategy_t<FilterFields, AliasesByName,
      ValidateFields, Finalize> &>(*this);
  }

  OMNI_CPP14_CONSTEXPR record_strategy_t<FilterFields, AliasesByName,
    ValidateFields, Finalize> &derived() & {
    return static_cast<record_strategy_t<FilterFields, AliasesByName,
      ValidateFields, Finalize> &>(*this);
  }

  template <typename Filter, typename Aliases, typename Validators,
    typename Finish>
  static constexpr auto make_strategy(Filter &&filter, Aliases &&aliases,
    Validators &&validators, Finish &&finish)
    -> record_strategy_t<compat::decay_t<Filter>, compat::decay_t<Aliases>,
      compat::decay_t<Validators>, compat::decay_t<Finish>> {
    return {
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*filter_fields=*/std::forward<Filter>(filter),
      /*aliases_by_name=*/std::forward<Aliases>(aliases),
      /*validate_fields=*/std::forward<Validators>(validators),
      /*finalize=*/std::forward<Finish>(finish),
    };
  }
};

} // namespace detail

/** Enum names and their accepted aliases.
 * aliases_by_name :: {enumerator_name: [alias]} */
template <typename AliasesByName = std::tuple<>>
struct enum_strategy_t {
  AliasesByName aliases_by_name OMNI_CPP14_DEFAULT_INIT();

  /** (QoL) Functional update of .aliases_by_name. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, const AliasesByName &>::value))
  constexpr auto tdm_aliases_by_name(F &&f) const &
    -> enum_strategy_t<compat::decay_t<
      compat::invoke_result_t<F &&, const AliasesByName &>>> {
    return {compat::invoke(std::forward<F>(f), this->aliases_by_name)};
  }

  /** (QoL) Functional update consuming .aliases_by_name. */
  OMNI_CONSTRAINED_TEMPLATE((typename F),
    (compat::is_invocable<F &&, AliasesByName &&>::value))
  OMNI_CPP14_CONSTEXPR auto tdm_aliases_by_name(F &&f) &&
    -> enum_strategy_t<compat::decay_t<
      compat::invoke_result_t<F &&, AliasesByName &&>>> {
    return {compat::invoke(std::forward<F>(f),
      std::move(this->aliases_by_name))};
  }
};

#if defined(__cpp_inline_variables) && 201606L <= __cpp_inline_variables
inline constexpr record_strategy_t<> record_strategy{};
inline constexpr enum_strategy_t<> enum_strategy{};
#else
static constexpr const record_strategy_t<> &record_strategy =
  compat::detail::shared_state<record_strategy_t<>>::value;
static constexpr const enum_strategy_t<> &enum_strategy =
  compat::detail::shared_state<enum_strategy_t<>>::value;
#endif

/** (QoL) Canonical-name groups, each followed by its aliases.
 * aliases_dict :: (group, ...) -> {name: [alias]}
 * Borrowed names must outlive uses of the strategy. */
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

struct strategy_name {
  compat::string_view spelling;
  bool is_canonical;
};

struct alias_group_names {
  template <typename Name, typename... Aliases>
  OMNI_CPP14_CONSTEXPR auto operator()(const Name &name,
    const Aliases &...aliases) const
    -> std::array<strategy_name, 1 + sizeof...(Aliases)> {
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

struct collect_strategy_names {
  template <typename... Entries>
  constexpr auto operator()(const Entries &...entries) const
    -> std::array<strategy_name, sizeof...(Entries)> {
    return {{entries...}};
  }
};

template <typename Names, typename Groups, std::size_t... Index>
OMNI_CPP14_CONSTEXPR bool valid_alias_groups(const Names &names,
  const Groups &groups, compat::index_sequence<Index...>) {
  const auto entries = compat::apply(collect_strategy_names{}, std::tuple_cat(
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

/** Field-name and alias validation for a record strategy. */
template <typename Names, typename Strategy>
OMNI_CPP14_CONSTEXPR bool valid_record_strategy(const Names &names,
  const Strategy &strategy) {
  return valid_aliases(names, strategy.aliases_by_name)
    && detail::valid_field_groups(names, strategy.validate_fields,
      compat::make_index_sequence<std::tuple_size<
        decltype(strategy.validate_fields)>::value>{});
}

} // namespace mapping
} // namespace omni

// TODO: [medium] Explore lazy dependent-field validation only if it offers
// practical benefits over record finalization; dependency ordering and sibling
// branch dependencies could substantially complicate the interface.
