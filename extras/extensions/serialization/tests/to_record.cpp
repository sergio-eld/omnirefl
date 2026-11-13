#include <omnirefl/serialization/ryml.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <tuple>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {

using string_view = omni::compat::string_view;

namespace examples {

struct runtime_endpoint { std::string host; };

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

  omni::compat::optional<string_view> operator()(string_view) const {
    return string_view{"host"};
  }
};

} // namespace examples

namespace tests {

TEST(alias_lookup, resolves_aliases_from_unsorted_groups_and_a_copy) {
  const auto names = omni::ryml::alias_dict(
    std::make_tuple("port", "service_port", "listen_port"),
    std::make_tuple("host", "hostname"));

  const auto copied = names;

  const auto hostname = names("hostname");
  EXPECT_TRUE(hostname);
  if (hostname)
    EXPECT_EQ("host", *hostname);

  const auto service_port = copied("service_port");
  EXPECT_TRUE(service_port);
  if (service_port)
    EXPECT_EQ("port", *service_port);

  const auto listen_port = copied("listen_port");
  EXPECT_TRUE(listen_port);
  if (listen_port)
    EXPECT_EQ("port", *listen_port);

  EXPECT_FALSE(names("missing"));
  EXPECT_FALSE(names("host"));
}

TEST(alias_lookup, accepts_tuple_like_groups_and_canonical_only_groups) {
  const std::array<string_view, 3> host{{"host", "hostname", "server"}};
  const std::pair<string_view, string_view> port{"port", "service_port"};
  const auto names = omni::ryml::alias_dict(host, port, std::make_tuple("tls"));

  const auto hostname = names("hostname");
  EXPECT_TRUE(hostname);
  if (hostname)
    EXPECT_EQ("host", *hostname);

  const auto server = names("server");
  EXPECT_TRUE(server);
  if (server)
    EXPECT_EQ("host", *server);

  const auto service_port = names("service_port");
  EXPECT_TRUE(service_port);
  if (service_port)
    EXPECT_EQ("port", *service_port);

  EXPECT_FALSE(names("tls"));
}

TEST(alias_lookup, returns_no_name_for_an_empty_dictionary) {
  const auto empty = omni::ryml::alias_dict();
  const auto canonical_only = omni::ryml::alias_dict(std::make_tuple("host"));

  EXPECT_FALSE(empty("hostname"));
  EXPECT_FALSE(canonical_only("hostname"));
}

TEST(alias_lookup, sorts_entries_in_constant_evaluation) {
  constexpr auto sorted = omni::ryml::detail::cpp11_sorted_aliases{}(
    omni::ryml::detail::alias_entry{"tls", "secure"},
    omni::ryml::detail::alias_entry{"host_name", "host"},
    omni::ryml::detail::alias_entry{"host", "host"},
    omni::ryml::detail::alias_entry{"hostname", "host"},
    omni::ryml::detail::alias_entry{"port", "port"});

  const auto tls = sorted("tls");
  EXPECT_TRUE(tls);
  if (tls)
    EXPECT_EQ("secure", *tls);

  const auto host_name = sorted("host_name");
  EXPECT_TRUE(host_name);
  if (host_name)
    EXPECT_EQ("host", *host_name);

  const auto host = sorted("host");
  EXPECT_TRUE(host);
  if (host)
    EXPECT_EQ("host", *host);

  const auto hostname = sorted("hostname");
  EXPECT_TRUE(hostname);
  if (hostname)
    EXPECT_EQ("host", *hostname);

  const auto port = sorted("port");
  EXPECT_TRUE(port);
  if (port)
    EXPECT_EQ("port", *port);
}

TEST(alias_lookup, prepares_empty_and_single_entry_constexpr_tables) {
  constexpr auto empty = omni::ryml::detail::cpp11_sorted_aliases{}();
  constexpr auto single = omni::ryml::detail::cpp11_sorted_aliases{}(
    omni::ryml::detail::alias_entry{"hostname", "host"});

  EXPECT_FALSE(empty("hostname"));
  const auto hostname = single("hostname");
  EXPECT_TRUE(hostname);
  if (hostname)
    EXPECT_EQ("host", *hostname);
}

TEST(alias_lookup, orders_empty_prefix_and_non_ascii_keys) {
  constexpr auto sorted = omni::ryml::detail::cpp11_sorted_aliases{}(
    omni::ryml::detail::alias_entry{"\x80", "extended"},
    omni::ryml::detail::alias_entry{"host_", "suffix"},
    omni::ryml::detail::alias_entry{"", "empty"},
    omni::ryml::detail::alias_entry{"host", "plain"},
    omni::ryml::detail::alias_entry{"hostname", "long"});

  const auto _x80 = sorted("\x80");
  EXPECT_TRUE(_x80);
  if (_x80)
    EXPECT_EQ("extended", *_x80);

  const auto host_ = sorted("host_");
  EXPECT_TRUE(host_);
  if (host_)
    EXPECT_EQ("suffix", *host_);

  const auto resolved_name = sorted("");
  EXPECT_TRUE(resolved_name);
  if (resolved_name)
    EXPECT_EQ("empty", *resolved_name);

  const auto host = sorted("host");
  EXPECT_TRUE(host);
  if (host)
    EXPECT_EQ("plain", *host);

  const auto hostname = sorted("hostname");
  EXPECT_TRUE(hostname);
  if (hostname)
    EXPECT_EQ("long", *hostname);
}

// C++11 does not require make_tuple/tuple_cat to be constexpr. Sort preparation
// is tested independently above; full factory evaluation starts with C++14.
#if 201402L <= OMNI_CPLUSPLUS
TEST(alias_lookup, prepares_an_inverse_dictionary_in_constant_evaluation) {
  constexpr auto names = omni::ryml::alias_dict(
    std::make_tuple("port", "service_port", "port_number"),
    std::make_tuple("host", "hostname", "server", "host_name"),
    std::make_tuple("tls", "secure", "ssl"));

  const auto hostname = names("hostname");
  EXPECT_TRUE(hostname);
  if (hostname)
    EXPECT_EQ("host", *hostname);

  const auto host_name = names("host_name");
  EXPECT_TRUE(host_name);
  if (host_name)
    EXPECT_EQ("host", *host_name);

  const auto server = names("server");
  EXPECT_TRUE(server);
  if (server)
    EXPECT_EQ("host", *server);

  const auto port_number = names("port_number");
  EXPECT_TRUE(port_number);
  if (port_number)
    EXPECT_EQ("port", *port_number);

  const auto service_port = names("service_port");
  EXPECT_TRUE(service_port);
  if (service_port)
    EXPECT_EQ("port", *service_port);

  const auto secure = names("secure");
  EXPECT_TRUE(secure);
  if (secure)
    EXPECT_EQ("tls", *secure);

  const auto ssl = names("ssl");
  EXPECT_TRUE(ssl);
  if (ssl)
    EXPECT_EQ("tls", *ssl);
}
#endif

// C++14 tuple helpers allow retaining the prepared table in a constexpr object.
#if 201402L <= OMNI_CPLUSPLUS
TEST(alias_lookup, prepares_and_queries_a_persistent_constexpr_dictionary) {
  constexpr auto names = omni::ryml::alias_dict(
    std::make_tuple("port", "service_port"),
    std::make_tuple("host", "hostname", "server"));

  const auto hostname = names("hostname");
  EXPECT_TRUE(hostname);
  if (hostname)
    EXPECT_EQ("host", *hostname);

  const auto server = names("server");
  EXPECT_TRUE(server);
  if (server)
    EXPECT_EQ("host", *server);

  const auto service_port = names("service_port");
  EXPECT_TRUE(service_port);
  if (service_port)
    EXPECT_EQ("port", *service_port);

  EXPECT_FALSE(names("missing"));
}
#endif

TEST(to_record_aliases, default_has_no_alias_mapping) {
  const auto compositor = omni::ryml::reflected_scope::to_record;

  EXPECT_FALSE(compositor.alias_by_name);
}

TEST(to_record_aliases, dictionary_replaces_the_previous_resolver) {
  const auto original =
    omni::ryml::reflected_scope::to_record.with_alias_by_name(
      omni::ryml::alias_dict(std::make_tuple("host", "hostname")));

  const auto replacement = original.with_alias_by_name(
    omni::ryml::alias_dict(std::make_tuple("port", "service_port")));

  const auto hostname = original.alias_by_name("hostname");
  EXPECT_TRUE(hostname);
  if (hostname)
    EXPECT_EQ("host", *hostname);

  EXPECT_FALSE(original.alias_by_name("service_port"));
  const auto service_port = replacement.alias_by_name("service_port");
  EXPECT_TRUE(service_port);
  if (service_port)
    EXPECT_EQ("port", *service_port);

  EXPECT_FALSE(replacement.alias_by_name("hostname"));
}

TEST(to_record_aliases, supports_runtime_configuration_per_instance) {
  const auto legacy =
    omni::ryml::reflected_scope::to_record.with_alias_by_name(
      examples::runtime_aliases{/*legacy=*/true});

  const auto current =
    omni::ryml::reflected_scope::to_record.with_alias_by_name(
      examples::runtime_aliases{/*legacy=*/false});

  const auto hostname = legacy.alias_by_name("hostname");

  EXPECT_TRUE(hostname);
  if (hostname)
    EXPECT_EQ("host", *hostname);

  EXPECT_FALSE(current.alias_by_name("hostname"));
}

// C++11 lambda equivalent: apply this compositor inside the reflected scope.
struct cpp11_map_with_aliases {
  omni::ryml::reflected_scope::to_record_t compositor;
  ::ryml::ConstNodeRef node;
  const omni::ryml::detail::mapping_context &context;

  template <typename Record>
  auto operator()(omni::record_binding_t<Record> record) const
    -> omni::ryml::with_diagnostics<omni::compat::monostate,
      omni::ryml::diagnostics<false>> {
    return this->compositor(this->node, &record.value(), this->context);
  }
};

class runtime_record_aliases: public testing::TestWithParam<bool> {};

TEST_P(runtime_record_aliases, maps_using_the_resolvers_runtime_state) {
  // The same resolver type accepts the alias only in legacy mode.
  const bool legacy = GetParam();
  const auto compositor =
    omni::ryml::reflected_scope::to_record.with_alias_by_name(
      examples::runtime_aliases{/*legacy=*/legacy});

  for (const std::string document : {R"({"hostname": "localhost"})",
         "hostname: localhost\n"}) {
    SCOPED_TRACE(document);

    auto tree = omni::ryml::core::parse(document);
    EXPECT_TRUE(tree);
    if (!tree)
      continue;

    const omni::ryml::config options{};
    const omni::ryml::detail::mapping_context context{
      /*options=*/options,
      /*path=*/{},
      /*error_count=*/0,
      /*warning_count=*/0,
      /*borrow_strings=*/false,
    };

    examples::runtime_endpoint into{/*host=*/"unchanged"};
    const auto mapped = omni::reflected_call(
      cpp11_map_with_aliases{
        /*compositor=*/compositor,
        /*node=*/tree->crootref(),
        /*context=*/context,
      },

      into);

    EXPECT_EQ(legacy, bool(mapped.value));
    EXPECT_EQ(legacy ? "localhost" : "unchanged", into.host);
    EXPECT_EQ(legacy ? 0u : 1u, mapped.diagnostics.issues.size());
    for (const auto &issue : mapped.diagnostics.issues) {
      EXPECT_EQ(omni::json::issue_code::unknown_field, issue.reason);
      EXPECT_EQ("/hostname", omni::ryml::detail::render_path(issue.path));
      EXPECT_FALSE(issue.warning);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(configuration,
  runtime_record_aliases,
  testing::Bool(),
  [](const testing::TestParamInfo<bool> &info) {
    return info.param ? "legacy" : "current";
  });

TEST(to_record_aliases, accepts_a_callable_with_only_a_copy_constructor) {
  const examples::copy_only_alias_by_name resolver{};
  const auto compositor =
    omni::ryml::reflected_scope::to_record.with_alias_by_name(resolver);

  const auto hostname = compositor.alias_by_name("hostname");
  EXPECT_TRUE(hostname);
  if (hostname)
    EXPECT_EQ("host", *hostname);
}

TEST(to_record_aliases, accepts_a_mutable_callable) {
  const auto compositor =
    omni::ryml::reflected_scope::to_record.with_alias_by_name(
      [](string_view) mutable -> omni::compat::optional<string_view> {
        return omni::compat::nullopt;
      });

  EXPECT_EQ(omni::compat::nullopt, compositor.alias_by_name("hostname"));
}

TEST(to_record_aliases, checks_the_callable_when_configured) {
  const auto compositor =
    omni::ryml::reflected_scope::to_record.with_alias_by_name(
      [](string_view alias) -> omni::compat::optional<string_view> {
        return alias == "hostname"
          ? omni::compat::optional<string_view>{"host"}
          : omni::compat::nullopt;
      });

  const auto hostname = compositor.alias_by_name("hostname");

  EXPECT_TRUE(hostname);
  if (hostname)
    EXPECT_EQ("host", *hostname);

  // Enable independently to inspect an invalid argument or return type.
#if 0
  (void)omni::ryml::reflected_scope::to_record.with_alias_by_name(
    [](int) -> omni::compat::optional<string_view> {
      return omni::compat::nullopt;
    });
#endif

#if 0
  (void)omni::ryml::reflected_scope::to_record.with_alias_by_name(
    [](string_view) -> string_view { return "host"; });
#endif
}

} // namespace tests
} // namespace

namespace {
namespace alias_models {

struct endpoint { int port; };
struct endpoint_with_retries {
  int port;
  int retries;
};

struct optional_endpoint { omni::compat::optional<int> port; };
struct default_endpoint { int port = 42; };
struct cluster {
  endpoint primary;
  std::vector<endpoint> peers;
};

struct endpoint_names {
  omni::compat::optional<string_view> operator()(string_view alias) const {
    static const auto names = omni::ryml::alias_dict(
      std::make_tuple("port", "service_port", "listen_port", "legacy/port"),
      std::make_tuple("absent", "not_a_member"));

    return names(alias);
  }
};

} // namespace alias_models
} // namespace

namespace omni {
namespace ryml {
namespace cp {

template <>
struct to_type<alias_models::endpoint>:
    omni::ryml::reflected_scope::to_record_t {
  to_type(): to_record_t{/*alias_by_name=*/alias_models::endpoint_names{}} {}
};

template <>
struct to_type<alias_models::endpoint_with_retries>:
    omni::ryml::reflected_scope::to_record_t {
  to_type(): to_record_t{/*alias_by_name=*/alias_models::endpoint_names{}} {}
};

template <>
struct to_type<alias_models::optional_endpoint>:
    omni::ryml::reflected_scope::to_record_t {
  to_type(): to_record_t{/*alias_by_name=*/alias_models::endpoint_names{}} {}
};

template <>
struct to_type<alias_models::default_endpoint>:
    omni::ryml::reflected_scope::to_record_t {
  to_type(): to_record_t{/*alias_by_name=*/alias_models::endpoint_names{}} {}
};

} // namespace cp
} // namespace ryml
} // namespace omni

namespace {

using omni::json::duplicate_policy;
using omni::json::unknown_policy;
using omni::json::issue_code;

struct alias_issue {
  issue_code reason;
  std::string path;
  bool warning;
};

// Policies apply to resolved members; diagnostic paths keep the input spelling.
struct alias_case {
  std::string name;
  std::string document;
  duplicate_policy duplicates;
  unknown_policy unknown_fields;
  omni::compat::optional<int> port;
  std::vector<alias_issue> issues;
};

const alias_case alias_cases[]{
  {"canonical", R"({"port": 7})", duplicate_policy::reject,
    unknown_policy::reject, 7, {}},
  {"alias", R"({"service_port": 7})", duplicate_policy::reject,
    unknown_policy::reject, 7, {}},
  {"yaml_alias", "listen_port: 7\n", duplicate_policy::reject,
    unknown_policy::reject, 7, {}},
  {"first_alias", R"({"service_port": 7, "listen_port": 8})",
    duplicate_policy::allow, unknown_policy::reject, 7, {}},
  {"last_alias", R"({"service_port": 7, "listen_port": 8})",
    duplicate_policy::last, unknown_policy::reject, 8, {}},
  {"last_canonical", R"({"service_port": 7, "port": 8})",
    duplicate_policy::last, unknown_policy::reject, 8, {}},
  {"first_canonical", R"({"port": 7, "service_port": 8})",
    duplicate_policy::allow, unknown_policy::reject, 7, {}},
  {"warn_duplicate", R"({"service_port": 7, "listen_port": 8})",
    duplicate_policy::allow | duplicate_policy::warn, unknown_policy::reject,
    7, {{issue_code::duplicate_field, "/listen_port", true}}},
  {"warn_last_duplicate", R"({"service_port": 7, "listen_port": 8})",
    duplicate_policy::last | duplicate_policy::warn, unknown_policy::reject,
    8, {{issue_code::duplicate_field, "/listen_port", true}}},
  {"warn_without_allow", R"({"service_port": 7, "listen_port": 8})",
    duplicate_policy::warn, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::duplicate_field, "/listen_port", false}}},
  {"repeated_alias", R"({"service_port": 7, "service_port": 8})",
    duplicate_policy::last, unknown_policy::reject, 8, {}},
  {"last_skips_invalid_first", R"({"service_port": "bad", "port": 8})",
    duplicate_policy::last, unknown_policy::reject, 8, {}},
  {"first_skips_invalid_last", R"({"port": 7, "service_port": "bad"})",
    duplicate_policy::allow, unknown_policy::reject, 7, {}},
  {"unknown", R"({"service_port": 7, "other": 8})",
    duplicate_policy::reject, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::unknown_field, "/other", false}}},
  {"allow_unknown", R"({"service_port": 7, "other": 8})",
    duplicate_policy::reject, unknown_policy::allow, 7, {}},
  {"warn_unknown", R"({"service_port": 7, "other": 8})",
    duplicate_policy::reject, unknown_policy::allow | unknown_policy::warn,
    7, {{issue_code::unknown_field, "/other", true}}},
  {"unmatched_target", R"({"not_a_member": 8, "port": 7})",
    duplicate_policy::reject, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::unknown_field, "/not_a_member", false}}},
  {"invalid_alias_value", R"({"service_port": "wrong"})",
    duplicate_policy::reject, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::invalid_scalar, "/service_port", false}}},
  {"escaped_alias_path", R"({"legacy/port": "wrong"})",
    duplicate_policy::reject, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::invalid_scalar, "/legacy~1port", false}}},
  {"missing_member", "{}", duplicate_policy::reject, unknown_policy::reject,
    omni::compat::nullopt, {{issue_code::missing_field, "/port", false}}},
  // Canonical presence suppresses aliases even when its value is invalid.
  {"invalid_canonical_selected_first", R"({"port": "bad", "service_port": 8})",
    duplicate_policy::allow, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::invalid_scalar, "/port", false}}},
  {"invalid_canonical_over_last_alias", R"({"port": "bad", "service_port": 8})",
    duplicate_policy::last, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::invalid_scalar, "/port", false}}},
  {"invalid_canonical_over_first", R"({"service_port": 7, "port": "bad"})",
    duplicate_policy::allow, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::invalid_scalar, "/port", false}}},
  {"invalid_canonical_selected_last", R"({"service_port": 7, "port": "bad"})",
    duplicate_policy::last, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::invalid_scalar, "/port", false}}},
};

const alias_case canonical_priority_cases[]{
  {"canonical_before_alias", R"({"port": 7, "service_port": 8})",
    duplicate_policy::reject, unknown_policy::reject, 7, {}},
  {"canonical_after_alias", R"({"service_port": 7, "port": 8})",
    duplicate_policy::reject, unknown_policy::reject, 8, {}},
  {"canonical_over_last_alias", R"({"port": 7, "service_port": 8})",
    duplicate_policy::last, unknown_policy::reject, 7, {}},
  {"canonical_over_first_alias", R"({"service_port": 7, "port": 8})",
    duplicate_policy::allow, unknown_policy::reject, 8, {}},
  {"canonical_suppresses_alias_warning", R"({"port": 7, "service_port": 8})",
    duplicate_policy::allow | duplicate_policy::warn, unknown_policy::reject,
    7, {}},
  {"canonical_with_warn_without_allow", R"({"port": 7, "service_port": 8})",
    duplicate_policy::warn, unknown_policy::reject, 7, {}},
  {"canonical_over_invalid_alias", R"({"port": 7, "service_port": "bad"})",
    duplicate_policy::last, unknown_policy::reject, 7, {}},
  {"canonical_after_invalid_alias", R"({"service_port": "bad", "port": 8})",
    duplicate_policy::allow, unknown_policy::reject, 8, {}},
  {"invalid_canonical_with_reject", R"({"port":"bad","service_port":8})",
    duplicate_policy::reject, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::invalid_scalar, "/port", false}}},
  {"repeated_canonical_rejected", R"({"port":7,"service_port":99,"port":8})",
    duplicate_policy::reject, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::duplicate_field, "/port", false}}},
  {"repeated_canonical_first", R"({"port":7,"service_port":99,"port":8})",
    duplicate_policy::allow, unknown_policy::reject, 7, {}},
  {"repeated_canonical_last", R"({"port":7,"service_port":99,"port":8})",
    duplicate_policy::last, unknown_policy::reject, 8, {}},
  {"repeated_canonical_warn", R"({"port":7,"service_port":99,"port":8})",
    duplicate_policy::last | duplicate_policy::warn, unknown_policy::reject,
    8, {{issue_code::duplicate_field, "/port", true}}},
  {"repeated_canonical_invalid_first",
    R"({"port":"bad","service_port":99,"port":8})",
    duplicate_policy::allow, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::invalid_scalar, "/port", false}}},
  {"repeated_canonical_valid_last",
    R"({"port":"bad","service_port":99,"port":8})",
    duplicate_policy::last, unknown_policy::reject, 8, {}},
  {"repeated_canonical_valid_first",
    R"({"port":7,"service_port":99,"port":"bad"})",
    duplicate_policy::allow, unknown_policy::reject, 7, {}},
  {"repeated_canonical_invalid_last",
    R"({"port":7,"service_port":99,"port":"bad"})",
    duplicate_policy::last, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::invalid_scalar, "/port", false}}},
  {"repeated_canonical_all_invalid",
    R"({"port":"bad","service_port":99,"port":"wrong"})",
    duplicate_policy::last, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::invalid_scalar, "/port", false}}},
  {"yaml_canonical_after_alias", "service_port: 7\nport: 8\n",
    duplicate_policy::reject, unknown_policy::reject, 8, {}},
  {"yaml_canonical_before_alias", "port: 7\nservice_port: 8\n",
    duplicate_policy::last, unknown_policy::reject, 7, {}},
  {"yaml_invalid_canonical", "service_port: 7\nport: bad\n",
    duplicate_policy::allow, unknown_policy::reject, omni::compat::nullopt,
    {{issue_code::invalid_scalar, "/port", false}}},
  {"yaml_repeated_canonical", "port: 7\nservice_port: 99\nport: 8\n",
    duplicate_policy::last | duplicate_policy::warn, unknown_policy::reject,
    8, {{issue_code::duplicate_field, "/port", true}}},

};

class record_aliases: public testing::TestWithParam<alias_case> {};

TEST_P(record_aliases, maps_with_configured_policies_and_original_paths) {
  const auto &c = GetParam();
  const auto load = omni::ryml::deserialize.transform_config(
    [&c](omni::ryml::config options) {
      options.error_policy.duplicates = c.duplicates;
      options.error_policy.unknown_fields = c.unknown_fields;
      return options;
    });

  const auto result = load(omni::type_t<alias_models::endpoint>{}, c.document);

  EXPECT_EQ(bool(c.port), bool(result.value));
  if (c.port && result.value)
    EXPECT_EQ(*c.port, result.value->port);

  EXPECT_EQ(c.issues.size(), result.diagnostics.issues.size());
  auto actual = result.diagnostics.issues.begin();
  for (const auto &expected : c.issues) {
    if (actual == result.diagnostics.issues.end())
      break;

    EXPECT_EQ(expected.reason, actual->reason);
    EXPECT_EQ(expected.path, omni::ryml::detail::render_path(actual->path));
    EXPECT_EQ(expected.warning, actual->warning);
    ++actual;
  }
}

INSTANTIATE_TEST_SUITE_P(policies,
  record_aliases,
  testing::ValuesIn(alias_cases),
  [](const testing::TestParamInfo<alias_case> &info) {
    return info.param.name;
  });

INSTANTIATE_TEST_SUITE_P(canonical_priority,
  record_aliases,
  testing::ValuesIn(canonical_priority_cases),
  [](const testing::TestParamInfo<alias_case> &info) {
    return info.param.name;
  });

TEST(record_aliases, maps_nested_records_and_sequence_elements) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<alias_models::cluster>{},
    R"({"primary": {"service_port": 7}, "peers": [{"listen_port": 8}]})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_EQ(7, result.value->primary.port);
    EXPECT_EQ(1u, result.value->peers.size());
    if (!result.value->peers.empty())
      EXPECT_EQ(8, result.value->peers.front().port);
  }
}

TEST(record_aliases, preserves_nested_input_paths) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<alias_models::cluster>{},
    R"({"primary": {"service_port": 7}, "peers": [{"listen_port": "bad"}]})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1u, result.diagnostics.issues.size());
  for (const auto &issue : result.diagnostics.issues) {
    EXPECT_EQ(issue_code::invalid_scalar, issue.reason);
    EXPECT_EQ("/peers/0/listen_port",
      omni::ryml::detail::render_path(issue.path));
  }
}

TEST(record_aliases, observes_warning_limit_for_alias_duplicates) {
  const auto load = omni::ryml::deserialize.transform_config(
    [](omni::ryml::config options) {
      options.error_policy.duplicates =
        duplicate_policy::allow | duplicate_policy::warn;

      options.error_policy.warning_limit = 0;
      return options;
    });

  const auto result = load(omni::type_t<alias_models::endpoint>{},
    R"({"service_port": 7, "listen_port": 8, "legacy/port": 9})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(2u, result.diagnostics.issues.size());
  for (const auto &issue : result.diagnostics.issues)
    EXPECT_EQ("/listen_port", omni::ryml::detail::render_path(issue.path));

  if (!result.diagnostics.issues.empty())
    EXPECT_EQ(issue_code::warning_limit_exceeded,
      result.diagnostics.issues.back().reason);
}

TEST(record_aliases, does_not_recover_an_invalid_present_alias) {
  const auto load = omni::ryml::deserialize.transform_config(
    [](omni::ryml::config options) {
      options.recovery = omni::json::recovery_policy::default_initializable;
      return options;
    });

  const auto missing = load(omni::type_t<alias_models::endpoint>{}, "{}");
  const auto invalid = load(omni::type_t<alias_models::endpoint>{},
    R"({"service_port": "bad"})");

  EXPECT_TRUE(missing.value);
  if (missing.value)
    EXPECT_EQ(0, missing.value->port);

  EXPECT_FALSE(invalid.value);
  EXPECT_EQ(1u, invalid.diagnostics.issues.size());
  for (const auto &issue : invalid.diagnostics.issues)
    EXPECT_EQ(issue_code::invalid_scalar, issue.reason);
}

TEST(record_aliases, maps_optional_aliases_and_absence) {
  const auto present = omni::ryml::deserialize(
    omni::type_t<alias_models::optional_endpoint>{},
    R"({"service_port": 7})");

  const auto absent = omni::ryml::deserialize(
    omni::type_t<alias_models::optional_endpoint>{}, "{}");

  const auto null = omni::ryml::deserialize(
    omni::type_t<alias_models::optional_endpoint>{},
    R"({"service_port": null})");

  EXPECT_TRUE(present.value);
  if (present.value) {
    EXPECT_TRUE(present.value->port);
    if (present.value->port)
      EXPECT_EQ(7, *present.value->port);
  }

  EXPECT_TRUE(absent.value);
  if (absent.value)
    EXPECT_FALSE(absent.value->port);

  EXPECT_TRUE(null.value);
  if (null.value)
    EXPECT_FALSE(null.value->port);
}

TEST(record_aliases, alias_overrides_recoverable_member_default) {
  const auto load = omni::ryml::deserialize.transform_config(
    [](omni::ryml::config options) {
      options.recovery = omni::json::recovery_policy::field_defaults;
      return options;
    });

  const auto absent = load(
    omni::type_t<alias_models::default_endpoint>{}, "{}");

  const auto present = load(omni::type_t<alias_models::default_endpoint>{},
    R"({"service_port": 7})");

  EXPECT_TRUE(absent.value);
  if (absent.value)
    EXPECT_EQ(42, absent.value->port);

  EXPECT_TRUE(present.value);
  if (present.value)
    EXPECT_EQ(7, present.value->port);
}

TEST(record_aliases, continues_reporting_up_to_the_configured_error_limit) {
  const auto load = omni::ryml::deserialize.transform_config(
    [](omni::ryml::config options) {
      options.error_policy.error_limit = 1;
      return options;
    });

  const auto result = load(omni::type_t<alias_models::endpoint>{},
    R"({"service_port": 7, "listen_port": 8, "other": 9, "legacy/port": 10})");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(2u, result.diagnostics.issues.size());
  if (!result.diagnostics.issues.empty()) {
    EXPECT_EQ(issue_code::duplicate_field,
      result.diagnostics.issues.front().reason);

    EXPECT_EQ("/listen_port",
      omni::ryml::detail::render_path(result.diagnostics.issues.front().path));

    EXPECT_EQ(issue_code::unknown_field,
      result.diagnostics.issues.back().reason);

    EXPECT_EQ("/other",
      omni::ryml::detail::render_path(result.diagnostics.issues.back().path));
  }
}

class interleaved_aliases: public testing::TestWithParam<duplicate_policy> {};

TEST_P(interleaved_aliases, selects_each_member_and_preserves_issue_order) {
  const auto policy = GetParam();
  const bool last = (policy & duplicate_policy::last) == duplicate_policy::last;
  const bool warn = (policy & duplicate_policy::warn) == duplicate_policy::warn;
  const auto load = omni::ryml::deserialize.transform_config(
    [policy](omni::ryml::config options) {
      options.error_policy.duplicates = policy;
      return options;
    });

  for (const std::string document : {
         R"({"service_port":7,"retries":3,"listen_port":8,)"
         R"("retries":4,"legacy/port":9})",
         "service_port: 7\nretries: 3\nlisten_port: 8\n"
         "retries: 4\nlegacy/port: 9\n",
       }) {
    SCOPED_TRACE(document);
    const auto result = load(
      omni::type_t<alias_models::endpoint_with_retries>{}, document);

    EXPECT_TRUE(result.value);
    if (result.value) {
      EXPECT_EQ(last ? 9 : 7, result.value->port);
      EXPECT_EQ(last ? 4 : 3, result.value->retries);
    }

    std::vector<std::string> paths;
    for (const auto &issue : result.diagnostics.issues) {
      EXPECT_EQ(issue_code::duplicate_field, issue.reason);
      EXPECT_TRUE(issue.warning);
      paths.push_back(omni::ryml::detail::render_path(issue.path));
    }

    const std::vector<std::string> expected = warn
      ? std::vector<std::string>{"/listen_port", "/retries", "/legacy~1port"}
      : std::vector<std::string>{};

    EXPECT_EQ(expected, paths);
  }
}

INSTANTIATE_TEST_SUITE_P(policies,
  interleaved_aliases,
  testing::Values(duplicate_policy::allow,
    duplicate_policy::last,
    duplicate_policy::allow | duplicate_policy::warn,
    duplicate_policy::last | duplicate_policy::warn),

  [](const testing::TestParamInfo<duplicate_policy> &info) {
    const bool last = (info.param & duplicate_policy::last)
      == duplicate_policy::last;

    const bool warn = (info.param & duplicate_policy::warn)
      == duplicate_policy::warn;

    return std::string{last ? "last" : "first"} + (warn ? "_warn" : "");
  });

TEST(record_aliases, ignored_aliases_do_not_exhaust_warning_limit) {
  const auto load = omni::ryml::deserialize.transform_config(
    [](omni::ryml::config options) {
      options.error_policy.duplicates =
        duplicate_policy::allow | duplicate_policy::warn;

      options.error_policy.warning_limit = 0;
      return options;
    });

  const auto result = load(omni::type_t<alias_models::endpoint>{},
    R"({"service_port":"bad","port":8,"listen_port":9})");

  EXPECT_TRUE(result.value);
  if (result.value)
    EXPECT_EQ(8, result.value->port);

  EXPECT_TRUE(result.diagnostics.issues.empty());
}

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
