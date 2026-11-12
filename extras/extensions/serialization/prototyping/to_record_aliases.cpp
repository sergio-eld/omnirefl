/** Experimental to_record compositor: aliases_for customization only.
 * AliasesFor maps actual member metadata to an alias range. aliases_for(f)
 * returns a new specialization; f receives metadata and the previous callable.
 * Signature errors are exposed only when actual member metadata is supplied.
 */

#include <omnirefl/extensions/compat.hpp>
#include <omnirefl/extensions/traits.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/traits.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// C++20 permits vector storage and sorting within constant evaluation.
#if defined(__cpp_lib_constexpr_vector) \
  && 201907L <= __cpp_lib_constexpr_vector \
  && defined(__cpp_lib_constexpr_algorithms) \
  && 201806L <= __cpp_lib_constexpr_algorithms
#  define CPP20_CONSTEXPR_VECTOR constexpr
#else
#  define CPP20_CONSTEXPR_VECTOR
#endif

namespace {

using string_view = omni::compat::string_view;

namespace prototype {

// Alias views borrow the character storage supplied to make_lookup.
struct lookup {
  CPP20_CONSTEXPR_VECTOR explicit lookup(
    std::vector<std::pair<string_view, std::vector<string_view>>> entries)
    : _entries(std::move(entries)) {
    std::sort(this->_entries.begin(), this->_entries.end());
  }

  // Declaring copy operations suppresses implicit moves; rvalues copy too.
  lookup(const lookup &) = default;
  lookup &operator=(const lookup &) = default;

  CPP20_CONSTEXPR_VECTOR std::vector<string_view> operator()(
    string_view name) const {
    const auto found = std::lower_bound(this->_entries.begin(),
      this->_entries.end(),
      name,
      [](const decltype(_entries)::value_type &entry, string_view name) {
        return entry.first < name;
      });

    return found != this->_entries.end() && found->first == name
      ? found->second
      : std::vector<string_view>{};
  }

private:
  std::vector<std::pair<string_view, std::vector<string_view>>> _entries;
};

CPP20_CONSTEXPR_VECTOR lookup make_lookup(
  std::vector<std::pair<string_view, std::vector<string_view>>> entries) {
  return lookup{std::move(entries)};
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

// Bind the previous callable to a two-argument override.
template <typename Previous, typename Override = Previous>
struct alias_override {
  static_assert(std::is_copy_constructible<Override>::value,
    "alias_override requires a copy-constructible override");
  static_assert(std::is_copy_constructible<Previous>::value,
    "alias_override requires a copy-constructible previous callable");

  Override custom;
  Previous previous;

  // C++11 needs the trailing expression to deduce the composed return type.
  template <typename _F>
  constexpr auto operator()(omni::data_member_meta_t<_F> m) const
    -> decltype(omni::compat::invoke(this->custom, m, this->previous)) {
    return omni::compat::invoke(this->custom, m, this->previous);
  }
};

// C++20 names the range requirement as a concept for constrained interfaces.
#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename Range>
concept alias_range = omni::compat::is_input_range<Range>::value
  && omni::traits::is_string<omni::compat::remove_cvref_t<
    omni::compat::range_reference_t<Range>>>::value;

template <typename Range>
using is_alias_range = std::bool_constant<alias_range<Range>>;
#else
template <typename Range, typename = void>
struct is_alias_range: std::false_type {};

template <typename Range>
struct is_alias_range<Range,
  omni::compat::enable_if_t<omni::compat::is_input_range<Range>::value>>:
    omni::traits::is_string<omni::compat::remove_cvref_t<
      omni::compat::range_reference_t<Range>>> {};
#endif

template <typename AliasesFor>
struct alias_for_type_sig;

template <typename Previous, typename Current>
struct alias_for_type_sig<alias_override<Previous, Current>> {
  // Supply the concrete metadata type when applying the compositor.
  template <typename Member, typename = void>
  struct validate: std::false_type {};

  template <typename Member>
  struct validate<Member,
    omni::compat::void_t<omni::compat::invoke_result_t<const Current &,
      Member,
      const Previous &>>>:
      is_alias_range<omni::compat::invoke_result_t<const Current &,
        Member,
        const Previous &>> {};
};

struct no_error: std::true_type {};

template <typename Callable>
struct invalid_aliases: std::false_type {};

// Preserve an earlier failure without inspecting a later override's body.
template <typename PreviousError,
  typename Previous,
  typename Current,
  typename Member>
struct alias_override_error: omni::compat::type_identity<PreviousError> {};

template <typename Previous, typename Current, typename Member>
struct alias_override_error<no_error, Previous, Current, Member> {
  using type = omni::compat::conditional_t<alias_for_type_sig<
    alias_override<Previous, Current>>::template validate<Member>::value,
    no_error,
    invalid_aliases<Current>>;
};

template <typename AliasesFor, typename Member>
struct alias_chain_error;

template <typename Previous, typename Current, typename Member>
struct alias_chain_error<alias_override<Previous, Current>, Member>:
    alias_override_error<typename alias_chain_error<Previous, Member>::type,
      Previous,
      Current,
      Member> {};

template <typename Current, typename Member>
struct alias_chain_error<alias_override<Current, Current>, Member>:
    alias_override_error<no_error, Current, Current, Member> {};

template <typename AliasesFor, typename Members>
struct first_alias_error;

template <typename AliasesFor>
struct first_alias_error<AliasesFor, std::tuple<>>:
    omni::compat::type_identity<no_error> {};

template <typename AliasesFor, typename Member, typename... Rest>
struct first_alias_error<AliasesFor, std::tuple<Member, Rest...>>:
    omni::compat::conditional_t<std::is_same<no_error,
      typename alias_chain_error<AliasesFor, Member>::type>::value,
      first_alias_error<AliasesFor, std::tuple<Rest...>>,
      alias_chain_error<AliasesFor, Member>> {};

// C++11 lambda equivalent: compute a compact result without asserting here.
template <typename AliasesFor>
struct cpp11_check_record {
  template <typename _M>
  constexpr auto operator()(omni::record_meta_t<_M>) const
  // Metadata does not exist before generation. Defer checking while keeping
  // reflected_call visible during instrumentation and IDE startup.
#if defined(OMNI_TOOL_RUN) \
  || !defined(OMNI_INCLUDED_GENERATED_REFLECTION_HEADER)
    -> no_error
#else
    -> typename first_alias_error<AliasesFor,
      decltype(std::declval<omni::record_meta_t<_M>>()
        .accessible_data_members())>::type
#endif
  { return {}; }
};

// MSVC prints the computed error tag without expanding the override chain.
#if defined(_MSC_VER) && !defined(__clang__)
template <typename Result>
struct assert_alias_result {
  static_assert(Result::value,
    "Invalid aliases_for override; expected: "
    "aliases_for :: (data_member_meta, previous) "
    "-> input_range<traits::is_string>");
  using type = void;
};
// GCC names Callable explicitly when it is deduced by this error overload.
#elif !defined(__clang__)
OMNI_CPP14_CONSTEXPR inline void assert_valid_aliases(no_error) {}

template <typename Callable>
OMNI_CPP14_CONSTEXPR void assert_valid_aliases(invalid_aliases<Callable>) {
  static_assert(invalid_aliases<Callable>::value,
    "Invalid aliases_for override; expected: "
    "aliases_for :: (data_member_meta, previous) "
    "-> input_range<traits::is_string>");
}
#endif

} // namespace detail

template <typename AliasesFor = detail::alias_override<
  default_value<std::array<string_view, 0>>>>
struct to_record {
  static_assert(omni::traits::is<detail::alias_override, AliasesFor>(),
    "to_record stores aliases through alias_override");

  AliasesFor _aliases_for;

  // No callability check is made here without concrete member metadata.
  template <typename Override>
  constexpr auto aliases_for(Override f) const
    -> to_record<detail::alias_override<AliasesFor, Override>> {
    return {
      /*_aliases_for=*/{
        /*custom=*/std::move(f),
        /*previous=*/this->_aliases_for,
      },
    };
  }
};

/** Validate every alias-override layer for every accessible data member.
 * Establishes a reflected scope and reports invalid overrides by static_assert.
 * Stops at the first failing member and its earliest invalid override.
 * Call outside a reflected scope; nested reflected_call is unsupported.
 */
// These standard C++ alternatives change diagnostic presentation, not checking.
// MSVC's return-type assertion avoids its verbose function-body backtrace.
#if defined(_MSC_VER) && !defined(__clang__)
template <typename AliasesFor, typename Record>
auto static_assert_alias_is_invocable(const to_record<AliasesFor> &,
  omni::type_t<Record> type)
  -> typename detail::assert_alias_result<decltype(omni::reflected_call(
    detail::cpp11_check_record<AliasesFor>{}, type))>::type {}
#else
template <typename AliasesFor, typename Record>
void static_assert_alias_is_invocable(const to_record<AliasesFor> &,
  omni::type_t<Record> type) {
  const auto result = omni::reflected_call(
    detail::cpp11_check_record<AliasesFor>{}, type);
  // Clang names the computed error tag in a direct assertion's diagnostic.
#if defined(__clang__)
  static_assert(decltype(result)::value,
    "Invalid aliases_for override; expected: "
    "aliases_for :: (data_member_meta, previous) "
    "-> input_range<traits::is_string>");
#else
  detail::assert_valid_aliases(result);
#endif
}
#endif

// TODO: [medium] Document static_assert_alias_is_invocable diagnostics on GCC,
// Clang, and MSVC: locate the signature assertion, identify the invalid
// callable in its template context, and find the user call site.

} // namespace prototype

namespace examples {

struct endpoint {
  std::string host;
  unsigned port;
};

// Only static_assert_alias_is_invocable discovers this record.
// Other examples use endpoint.
struct validation_record {
  std::string host;
  unsigned port;
};

// C++11 lambda equivalents also serve the newer-standard examples.
struct cpp11_host_aliases {
  template <typename _F, typename Previous>
  CPP20_CONSTEXPR_VECTOR std::vector<string_view> operator()(
    omni::data_member_meta_t<_F> m,
    const Previous &previous) const {
    // C++20 constant evaluation needs transient storage; C++11 caches it.
#if !defined(__cpp_lib_constexpr_vector) \
  || 201907L > __cpp_lib_constexpr_vector \
  || !defined(__cpp_lib_constexpr_algorithms) \
  || 201806L > __cpp_lib_constexpr_algorithms
    static
#endif
    const auto names = prototype::make_lookup({
      {"host", {"hostname"}},
    });
    const auto selected = names(m.name());
    const auto aliases = previous(m);
    std::vector<string_view> result(aliases.begin(), aliases.end());
    result.insert(result.end(), selected.begin(), selected.end());
    return result;
  }
};

struct cpp11_extend_host_aliases {
  template <typename _F, typename Previous>
  CPP20_CONSTEXPR_VECTOR std::vector<string_view> operator()(
    omni::data_member_meta_t<_F> m,
    const Previous &previous) const {
    const auto aliases = previous(m);
    std::vector<string_view> result(aliases.begin(), aliases.end());
    if (string_view{m.name()} == "host")
      result.emplace_back("server");
    return result;
  }
};

constexpr cpp11_host_aliases host_aliases{};
constexpr cpp11_extend_host_aliases extend_host_aliases{};

struct invalid_signature {
  std::vector<string_view> operator()(int) const { return {}; }
};

} // namespace examples

namespace tests {

TEST(alias_lookup, finds_aliases_from_unsorted_entries_and_a_copy) {
  // C++20 can prepare and consume a temporary lookup in constant evaluation.
#if defined(__cpp_lib_constexpr_vector) \
  && 201907L <= __cpp_lib_constexpr_vector \
  && defined(__cpp_lib_constexpr_algorithms) \
  && 201806L <= __cpp_lib_constexpr_algorithms
  static_assert(prototype::make_lookup({
    {"port", {"service_port"}},
    {"host", {"hostname"}},
  })("host").front() == "hostname");
#endif

  const auto names = prototype::make_lookup({
    {"port", {"service_port", "listen_port"}},
    {"host", {"hostname"}},
  });
  const auto copied = names;

  EXPECT_EQ((std::vector<string_view>{"hostname"}), names("host"));
  EXPECT_EQ((std::vector<string_view>{"service_port", "listen_port"}),
    copied("port"));
  EXPECT_EQ(std::vector<string_view>{}, copied("missing"));
}

TEST(alias_lookup, returns_no_aliases_for_empty_entries_or_lookup) {
  const auto empty = prototype::make_lookup({});
  const auto names = prototype::make_lookup({
    {"host", {}},
  });

  EXPECT_EQ(std::vector<string_view>{}, empty("host"));
  EXPECT_EQ(std::vector<string_view>{}, names("host"));
}

// C++11 lambda equivalent: normalize the returned range for readable checks.
template <typename Compositor>
struct cpp11_collect_member {
  const Compositor &compositor;

  template <typename _F>
  std::map<string_view, std::vector<string_view>> operator()(
    std::map<string_view, std::vector<string_view>> result,
    omni::data_member_meta_t<_F> m) const {
    static_assert(prototype::detail::alias_for_type_sig<
      decltype(this->compositor._aliases_for)>::template validate<
      omni::data_member_meta_t<_F>>::value,
      "Invalid aliases_for override; expected: "
      "aliases_for :: (data_member_meta, previous) "
      "-> input_range<traits::is_string>");

    const auto aliases = this->compositor._aliases_for(m);
    result.emplace(m.name(),
      std::vector<string_view>{aliases.begin(), aliases.end()});
    return result;
  }
};

template <typename Compositor>
struct inspect_aliases {
  Compositor compositor;

  // Explicit return type defers the visitor body until metadata is generated.
  template <typename _M>
  std::map<string_view, std::vector<string_view>> operator()(
    omni::record_meta_t<_M> m) const {
    return omni::fn::foldl(cpp11_collect_member<Compositor>{this->compositor},
      std::map<string_view, std::vector<string_view>>{},
      m.accessible_data_members());
  }
};

TEST(to_record_aliases, default_has_no_aliases) {
  static_assert(prototype::default_value<unsigned>{}("ignored") == 0,
    "Default values remain usable in constant expressions");

  static_assert(prototype::detail::is_alias_range<
    std::vector<std::string>>::value,
    "Alias ranges may own their strings");

  const prototype::to_record<> compositor{};
  const auto result = omni::reflected_call(
    inspect_aliases<decltype(compositor)>{compositor},
    omni::type_t<examples::endpoint>{});

  const std::map<string_view, std::vector<string_view>> expected{
    {"host", {}},
    {"port", {}},
  };
  EXPECT_EQ(expected, result);
}

TEST(to_record_aliases, override_selects_a_member_and_delegates_the_rest) {
  const auto compositor =
    prototype::to_record<>{}.aliases_for(examples::host_aliases);
  const auto result = omni::reflected_call(
    inspect_aliases<decltype(compositor)>{compositor},
    omni::type_t<examples::endpoint>{});

  const std::map<string_view, std::vector<string_view>> expected{
    {"host", {"hostname"}},
    {"port", {}},
  };
  EXPECT_EQ(expected, result);
}

TEST(to_record_aliases, override_composes_without_changing_the_previous_value) {
  const auto original =
    prototype::to_record<>{}.aliases_for(examples::host_aliases);
  const auto extended = original.aliases_for(examples::extend_host_aliases);
  const auto result = omni::reflected_call(
    inspect_aliases<decltype(extended)>{extended},
    omni::type_t<examples::endpoint>{});

  const std::map<string_view, std::vector<string_view>> expected{
    {"host", {"hostname", "server"}},
    {"port", {}},
  };
  EXPECT_EQ(expected, result);

  const auto unchanged = omni::reflected_call(
    inspect_aliases<decltype(original)>{original},
    omni::type_t<examples::endpoint>{});

  const std::map<string_view, std::vector<string_view>> previous{
    {"host", {"hostname"}},
    {"port", {}},
  };
  EXPECT_EQ(previous, unchanged);
}

TEST(to_record_aliases, lookup_appends_to_previous_aliases) {
  const auto compositor = prototype::to_record<>{}
    .aliases_for(examples::extend_host_aliases)
    .aliases_for(examples::host_aliases);
  const auto result = omni::reflected_call(
    inspect_aliases<decltype(compositor)>{compositor},
    omni::type_t<examples::endpoint>{});

  const std::map<string_view, std::vector<string_view>> expected{
    {"host", {"server", "hostname"}},
    {"port", {}},
  };
  EXPECT_EQ(expected, result);
}

TEST(to_record_aliases, validates_every_override_for_record_members) {
  constexpr auto valid = prototype::to_record<>{}
    .aliases_for(examples::host_aliases)
    .aliases_for(examples::extend_host_aliases);
  prototype::static_assert_alias_is_invocable(valid,
    omni::type_t<examples::validation_record>{});

  // Enable one block at a time to inspect its diagnostic independently.
  // Invalid second override.
#if 0
  prototype::static_assert_alias_is_invocable(prototype::to_record<>{}
      .aliases_for(examples::host_aliases)
      .aliases_for(examples::invalid_signature{}),
    omni::type_t<examples::validation_record>{});
#endif

  // Invalid first override; the valid replacement deliberately ignores it.
#if 0
  prototype::static_assert_alias_is_invocable(prototype::to_record<>{}
      .aliases_for(examples::invalid_signature{})
      .aliases_for(prototype::default_value<std::array<string_view, 0>>{}),
    omni::type_t<examples::validation_record>{});
#endif

  // Both overrides are invalid.
#if 0
  prototype::static_assert_alias_is_invocable(prototype::to_record<>{}
      .aliases_for(examples::invalid_signature{})
      .aliases_for(examples::invalid_signature{}),
    omni::type_t<examples::validation_record>{});
#endif
}

} // namespace tests
} // namespace

#undef CPP20_CONSTEXPR_VECTOR
