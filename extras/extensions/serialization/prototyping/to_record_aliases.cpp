/** Experimental to_record compositor: alias-to-canonical-name lookup only.
 * with_alias_by_name(f) replaces the resolver and checks its invocation type.
 * alias_dict takes tuple-like groups: canonical name, followed by its aliases.
 * Configuration and lookup do not require a reflected scope.
 */

#include <omnirefl/extensions/compat.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <tuple>
#include <utility>

namespace {

using string_view = omni::compat::string_view;

namespace prototype {

namespace detail {

struct alias_entry {
  string_view alias;
  string_view name;
};

// Compare bounded character buffers directly; view comparison operators
// lack C++11 constant evaluation. Lengths also preserve embedded NULs.
constexpr bool alias_less(const char *left,
  std::size_t left_size,
  const char *right,
  std::size_t right_size,
  std::size_t index = 0) {
  return index == right_size ? false
    : index == left_size ? true
    : static_cast<unsigned char>(left[index])
        == static_cast<unsigned char>(right[index])
      ? alias_less(left, left_size, right, right_size, index + 1)
      : static_cast<unsigned char>(left[index])
        < static_cast<unsigned char>(right[index]);
}

constexpr bool alias_less(string_view left, string_view right) {
  return alias_less(left.data(), left.size(), right.data(), right.size());
}

// Views borrow the strings supplied to alias_dict; keep their storage alive.
template <std::size_t Size>
struct name_by_alias_lookup {
  // alias_dict sorts these entries; users must not modify them afterward.
  std::array<alias_entry, Size> _entries;

  omni::compat::optional<string_view> operator()(string_view alias) const {
    const auto found = std::lower_bound(this->_entries.begin(),
      this->_entries.end(),
      alias,
      [](const alias_entry &entry, string_view alias) {
        return entry.alias < alias;
      });

    return found != this->_entries.end() && found->alias == alias
      ? omni::compat::optional<string_view>{found->name}
      : omni::compat::nullopt;
  }
};

// C++11 lambda equivalent: unpack the canonical name and its aliases.
struct cpp11_invert_alias_group {
  template <typename... Aliases>
  OMNI_CPP14_CONSTEXPR auto operator()(string_view name,
    const Aliases &...aliases) const
    -> decltype(std::make_tuple(alias_entry{string_view{aliases}, name}...)) {
    return std::make_tuple(alias_entry{string_view{aliases}, name}...);
  }
};

// C++11 cannot mutate arrays in constant evaluation. Build a new index order
// at each insertion, then copy entries only when materializing the result.
// Built-in arrays allow C++11 constexpr access; one slot handles Size=0.
template <std::size_t Size>
struct alias_buffer {
  alias_entry values[Size ? Size : 1];
};

template <std::size_t Size>
struct alias_order {
  std::size_t values[Size ? Size : 1];
};

template <std::size_t... Index>
constexpr alias_order<sizeof...(Index)> alias_indices(
  omni::compat::index_sequence<Index...>) {
  return {{Index...}};
}

template <std::size_t Size>
constexpr std::size_t alias_position(const alias_buffer<Size> &entries,
  const alias_order<Size> &order,
  std::size_t next,
  std::size_t position) {
  return position == 0
      || !alias_less(entries.values[next].alias,
        entries.values[order.values[position - 1]].alias)
    ? position
    : alias_position(entries, order, next, position - 1);
}

template <std::size_t Size, std::size_t... Index>
constexpr alias_order<Size> insert_alias_index(const alias_order<Size> &order,
  std::size_t next,
  std::size_t position,
  omni::compat::index_sequence<Index...>) {
  return {{(Index == position ? next
    : Index > position && Index <= next ? order.values[Index - 1]
    : order.values[Index])...}};
}

template <std::size_t Size>
constexpr alias_order<Size> insertion_sort(const alias_buffer<Size> &entries,
  const alias_order<Size> &order,
  std::size_t next = 1) {
  return next >= Size ? order
    : insertion_sort(entries,
        insert_alias_index(order,
          next,
          alias_position(entries, order, next, next),
          omni::compat::make_index_sequence<Size>{}),
        next + 1);
}

template <std::size_t Size, std::size_t... Index>
constexpr std::array<alias_entry, Size> materialize_alias_array(
  const alias_buffer<Size> &entries,
  const alias_order<Size> &order,
  omni::compat::index_sequence<Index...>) {
  return {{entries.values[order.values[Index]]...}};
}

template <std::size_t Size>
constexpr std::array<alias_entry, Size> sort_alias_entries(
  const alias_buffer<Size> &entries) {
  return materialize_alias_array(entries,
    insertion_sort(entries,
      alias_indices(omni::compat::make_index_sequence<Size>{})),
    omni::compat::make_index_sequence<Size>{});
}

// C++11 lambda equivalent: materialize and sort the flattened fixed-size table.
struct cpp11_sorted_aliases {
  template <typename... Entries>
  constexpr name_by_alias_lookup<sizeof...(Entries)> operator()(
    const Entries &...entries) const {
    return {
      /*_entries=*/sort_alias_entries(
        alias_buffer<sizeof...(Entries)>{{entries...}}),
    };
  }
};

} // namespace detail

/** Build an alias-to-name resolver from tuple-like string-view groups.
 * Each group starts with the canonical name; remaining entries are aliases.
 * Canonical names are not implicitly added as aliases. Returned views borrow
 * the supplied strings, whose storage must outlive use of the resolver.
 * Full constexpr factory evaluation requires C++14 tuple helpers; the sorting
 * implementation itself supports constant evaluation in C++11.
 */
template <typename... Groups>
OMNI_CPP14_CONSTEXPR auto alias_dict(const Groups &...groups)
  -> decltype(omni::compat::apply(detail::cpp11_sorted_aliases{},
    std::tuple_cat(omni::compat::apply(
      detail::cpp11_invert_alias_group{}, groups)...))) {
  return omni::compat::apply(detail::cpp11_sorted_aliases{},
    std::tuple_cat(omni::compat::apply(
      detail::cpp11_invert_alias_group{}, groups)...));
}

// Prototype of a generic fn::default_value compositor.
template <typename Result>
struct default_value {
  static_assert(std::is_default_constructible<Result>::value,
    "default_value requires a default-constructible result");

  template <typename... Args>
  constexpr Result operator()(const Args &...) const { return {}; }
};

namespace detail {

template <typename F, typename = void>
struct is_alias_by_name: std::false_type {};

template <typename F>
struct is_alias_by_name<F,
  omni::compat::void_t<omni::compat::invoke_result_t<const F &, string_view>>>:
    std::is_same<omni::compat::optional<string_view>,
      omni::compat::invoke_result_t<const F &, string_view>> {};

} // namespace detail

template <typename AliasByName = default_value<
  omni::compat::optional<string_view>>>
struct to_record {
  // Returned views must remain valid while the caller uses the resolved name.
  AliasByName alias_by_name;

  template <typename F>
  OMNI_CPP14_CONSTEXPR to_record<F> with_alias_by_name(F f) const {
    static_assert(std::is_copy_constructible<F>::value,
      "with_alias_by_name requires a copy-constructible callable");
    static_assert(detail::is_alias_by_name<F>::value,
      "Invalid alias_by_name callable; expected: "
      "alias_by_name :: string_view -> optional<string_view>");
    return {/*alias_by_name=*/std::move_if_noexcept(f)};
  }
};

} // namespace prototype

namespace examples {

struct runtime_aliases {
  bool legacy;

  omni::compat::optional<string_view> operator()(string_view alias) const {
    return this->legacy && alias == "hostname"
      ? omni::compat::optional<string_view>{"host"}
      : omni::compat::nullopt;
  }
};

struct copy_only_alias_by_name {
  copy_only_alias_by_name() = default;
  copy_only_alias_by_name(const copy_only_alias_by_name &) = default;
  copy_only_alias_by_name(copy_only_alias_by_name &&) = delete;

  omni::compat::optional<string_view> operator()(string_view) const {
    return string_view{"host"};
  }
};

} // namespace examples

namespace tests {

TEST(alias_lookup, resolves_aliases_from_unsorted_groups_and_a_copy) {
  const auto names = prototype::alias_dict(
    std::make_tuple("port", "service_port", "listen_port"),
    std::make_tuple("host", "hostname"));
  const auto copied = names;

  EXPECT_EQ("host", names("hostname").value());
  EXPECT_EQ("port", copied("service_port").value());
  EXPECT_EQ("port", copied("listen_port").value());
  EXPECT_FALSE(names("missing"));
  EXPECT_FALSE(names("host"));
}

TEST(alias_lookup, accepts_tuple_like_groups_and_canonical_only_groups) {
  const std::array<string_view, 3> host{{"host", "hostname", "server"}};
  const std::pair<string_view, string_view> port{"port", "service_port"};
  const auto names = prototype::alias_dict(host, port, std::make_tuple("tls"));

  EXPECT_EQ("host", names("hostname").value());
  EXPECT_EQ("host", names("server").value());
  EXPECT_EQ("port", names("service_port").value());
  EXPECT_FALSE(names("tls"));
}

TEST(alias_lookup, returns_no_name_for_an_empty_dictionary) {
  const auto empty = prototype::alias_dict();
  const auto canonical_only = prototype::alias_dict(std::make_tuple("host"));

  EXPECT_FALSE(empty("hostname"));
  EXPECT_FALSE(canonical_only("hostname"));
}

TEST(alias_lookup, sorts_entries_in_constant_evaluation) {
  constexpr auto sorted = prototype::detail::cpp11_sorted_aliases{}(
    prototype::detail::alias_entry{"tls", "secure"},
    prototype::detail::alias_entry{"host_name", "host"},
    prototype::detail::alias_entry{"host", "host"},
    prototype::detail::alias_entry{"hostname", "host"},
    prototype::detail::alias_entry{"port", "port"});

  EXPECT_EQ("secure", sorted("tls").value());
  EXPECT_EQ("host", sorted("host_name").value());
  EXPECT_EQ("host", sorted("host").value());
  EXPECT_EQ("host", sorted("hostname").value());
  EXPECT_EQ("port", sorted("port").value());
}

TEST(alias_lookup, prepares_empty_and_single_entry_constexpr_tables) {
  constexpr auto empty = prototype::detail::cpp11_sorted_aliases{}();
  constexpr auto single = prototype::detail::cpp11_sorted_aliases{}(
    prototype::detail::alias_entry{"hostname", "host"});

  EXPECT_FALSE(empty("hostname"));
  EXPECT_EQ("host", single("hostname").value());
}

TEST(alias_lookup, orders_empty_prefix_and_non_ascii_keys) {
  constexpr auto sorted = prototype::detail::cpp11_sorted_aliases{}(
    prototype::detail::alias_entry{"\x80", "extended"},
    prototype::detail::alias_entry{"host_", "suffix"},
    prototype::detail::alias_entry{"", "empty"},
    prototype::detail::alias_entry{"host", "plain"},
    prototype::detail::alias_entry{"hostname", "long"});

  EXPECT_EQ("extended", sorted("\x80").value());
  EXPECT_EQ("suffix", sorted("host_").value());
  EXPECT_EQ("empty", sorted("").value());
  EXPECT_EQ("plain", sorted("host").value());
  EXPECT_EQ("long", sorted("hostname").value());
}

// C++11 does not require make_tuple/tuple_cat to be constexpr. Sort preparation
// is tested independently above; full factory evaluation starts with C++14.
#if 201402L <= OMNI_CPLUSPLUS
TEST(alias_lookup, prepares_an_inverse_dictionary_in_constant_evaluation) {
  constexpr auto names = prototype::alias_dict(
    std::make_tuple("port", "service_port", "port_number"),
    std::make_tuple("host", "hostname", "server", "host_name"),
    std::make_tuple("tls", "secure", "ssl"));

  EXPECT_EQ("host", names("hostname").value());
  EXPECT_EQ("host", names("host_name").value());
  EXPECT_EQ("host", names("server").value());
  EXPECT_EQ("port", names("port_number").value());
  EXPECT_EQ("port", names("service_port").value());
  EXPECT_EQ("tls", names("secure").value());
  EXPECT_EQ("tls", names("ssl").value());
}
#endif

// C++14 tuple helpers allow retaining the prepared table in a constexpr object.
#if 201402L <= OMNI_CPLUSPLUS
TEST(alias_lookup, prepares_and_queries_a_persistent_constexpr_dictionary) {
  constexpr auto names = prototype::alias_dict(
    std::make_tuple("port", "service_port"),
    std::make_tuple("host", "hostname", "server"));

  EXPECT_EQ("host", names("hostname").value());
  EXPECT_EQ("host", names("server").value());
  EXPECT_EQ("port", names("service_port").value());
  EXPECT_FALSE(names("missing"));
}
#endif

TEST(to_record_aliases, default_has_no_alias_mapping) {
  const prototype::to_record<> compositor{};

  EXPECT_FALSE(compositor.alias_by_name("hostname"));
}

TEST(to_record_aliases, dictionary_replaces_the_previous_resolver) {
  const auto original = prototype::to_record<>{}.with_alias_by_name(
    prototype::alias_dict(std::make_tuple("host", "hostname")));
  const auto replacement = original.with_alias_by_name(
    prototype::alias_dict(std::make_tuple("port", "service_port")));

  EXPECT_EQ("host", original.alias_by_name("hostname").value());
  EXPECT_FALSE(original.alias_by_name("service_port"));
  EXPECT_EQ("port", replacement.alias_by_name("service_port").value());
  EXPECT_FALSE(replacement.alias_by_name("hostname"));
}

TEST(to_record_aliases, supports_runtime_configuration_per_instance) {
  const auto legacy = prototype::to_record<>{}.with_alias_by_name(
    examples::runtime_aliases{/*legacy=*/true});
  const auto current = prototype::to_record<>{}.with_alias_by_name(
    examples::runtime_aliases{/*legacy=*/false});

  EXPECT_EQ("host", legacy.alias_by_name("hostname").value());
  EXPECT_FALSE(current.alias_by_name("hostname"));
}

TEST(to_record_aliases, accepts_a_callable_with_a_deleted_move_constructor) {
  const examples::copy_only_alias_by_name resolver{};
  const auto compositor = prototype::to_record<>{}.with_alias_by_name(resolver);

  EXPECT_EQ("host", compositor.alias_by_name("hostname").value());
}

TEST(to_record_aliases, checks_the_callable_when_configured) {
  const auto compositor = prototype::to_record<>{}.with_alias_by_name(
    [](string_view alias) -> omni::compat::optional<string_view> {
      return alias == "hostname"
        ? omni::compat::optional<string_view>{"host"}
        : omni::compat::nullopt;
    });

  EXPECT_EQ("host", compositor.alias_by_name("hostname").value());

  // Enable independently to inspect an invalid argument or return type.
#if 0
  (void)prototype::to_record<>{}.with_alias_by_name(
    [](int) -> omni::compat::optional<string_view> {
      return omni::compat::nullopt;
    });
#endif

#if 0
  (void)prototype::to_record<>{}.with_alias_by_name(
    [](string_view) -> string_view { return "host"; });
#endif
}

} // namespace tests
} // namespace
