#include "json_match.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace diagnostic_inputs {

struct empty_record {};
struct embedded_document {};

} // namespace diagnostic_inputs

template <>
struct omni::ryml::cp::to_type_t<diagnostic_inputs::empty_record> {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node, omni::type_t<diagnostic_inputs::empty_record>)
    const -> omni::ryml::with_diagnostics_t<diagnostic_inputs::empty_record> {
    omni::ryml::diagnostics report{{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      state.config.error_policy, {}}, {}, {}};
    const bool warnings = state.config.error_policy.unknown_fields
      == (omni::json::unknown_policy::allow | omni::json::unknown_policy::warn);
    for (const auto child : node.children())
      report.issues.push_back(omni::ryml::core::make_issue(
        omni::json::issue_code::unknown_field,
        omni::json::value_kind::unknown, child, warnings));

    return {
      /*value=*/warnings
        ? omni::compat::make_optional(diagnostic_inputs::empty_record{})
        : omni::compat::nullopt,
      /*diagnostics=*/std::move(report),
    };
  }
};

template <>
struct omni::ryml::cp::to_type_t<diagnostic_inputs::embedded_document> {
  OMNI_CONSTRAINED_TEMPLATE((typename State),
    (omni::ryml::is_read_state<State>::value))
  auto operator()(State state,
    ::ryml::ConstNodeRef node,
    omni::type_t<diagnostic_inputs::embedded_document>) const
    -> omni::ryml::with_diagnostics_t<diagnostic_inputs::embedded_document> {
    const auto text = node.val();
    auto parsed = omni::ryml::core::parse(std::string{text.str, text.len});
    if (parsed)
      return {
        /*value=*/diagnostic_inputs::embedded_document{},
        /*diagnostics=*/{{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
          /*base=*/{},
#endif
          state.config.error_policy, {}}, {}, {}},
      };

    return {omni::compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      state.config.error_policy, {}},
      {omni::ryml::core::make_issue(omni::json::issue_code::parse_error,
        omni::json::value_kind::unknown, node)}, std::move(parsed.error())}};
  }
};

namespace {

TEST(read_state, identifies_read_state_specializations) {
  EXPECT_TRUE(omni::ryml::is_read_state<omni::ryml::read_state_t<>>::value);
  EXPECT_TRUE(omni::ryml::is_read_state<
    const omni::ryml::read_state_t<> &>::value);
  EXPECT_FALSE(omni::ryml::is_read_state<omni::ryml::write_state_t<>>::value);
  EXPECT_FALSE(omni::ryml::is_read_state<omni::ryml::config_t<>>::value);
}

TEST(write_state, identifies_write_state_specializations) {
  EXPECT_TRUE(omni::ryml::is_write_state<omni::ryml::write_state_t<>>::value);
  EXPECT_TRUE(omni::ryml::is_write_state<
    const omni::ryml::write_state_t<> &>::value);
  EXPECT_FALSE(omni::ryml::is_write_state<omni::ryml::read_state_t<>>::value);
  EXPECT_FALSE(omni::ryml::is_write_state<omni::ryml::config_t<>>::value);
}

TEST(mapping_diagnostics, retains_the_document_path_when_mapping_a_subtree) {
  const auto tree = omni::ryml::core::parse(
    R"({"outer": {"reading": "bad"}})");

  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto node = tree->crootref()["outer"]["reading"];
  const auto result = omni::ryml::map_by_schema(omni::type_t<int>{}, node);

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  for (const auto &entry : result.diagnostics.issues) {
    EXPECT_EQ(node.id(), entry.node_id);
    EXPECT_EQ("/outer/reading", omni::ryml::detail::render_path(entry.path));
  }
}

TEST(mapping_diagnostics, limits_errors_in_a_custom_report) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<std::vector<diagnostic_inputs::empty_record>>{},
    R"([{"first": 1, "second": 2, "third": 3}])");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/0/first: unknown field", result.diagnostics);
}

TEST(mapping_diagnostics, stops_after_the_warning_that_exceeds_the_limit) {
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_t<> options) {
      options.error_policy.unknown_fields =
        omni::json::unknown_policy::allow | omni::json::unknown_policy::warn;
      options.error_policy.warning_limit = 1;
      return options;
    });

  const auto result = load(
    omni::type_t<std::vector<diagnostic_inputs::empty_record>>{},
    R"([{"first": 1}, {"second": 2, "third": 3}])");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("/0/first: warning: unknown field\n"
            "/1/second: warning: unknown field\n"
            "/1/second: warning limit exceeded",

    result.diagnostics);
}

TEST(mapping_diagnostics, retains_a_nested_parse_error_message) {
  const auto parsed = omni::ryml::core::parse("[unterminated");
  EXPECT_FALSE(parsed);
  if (parsed)
    return;

  const auto result = omni::ryml::deserialize(
    omni::type_t<std::vector<diagnostic_inputs::embedded_document>>{},
    R"(["[unterminated"])");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(parsed.error(), result.diagnostics);
}

TEST(mapping_diagnostics, renders_each_nested_parse_error_once) {
  const auto first = omni::ryml::core::parse("[unterminated");
  const auto second = omni::ryml::core::parse("{unterminated");
  EXPECT_FALSE(first);
  EXPECT_FALSE(second);
  if (first || second)
    return;

  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_t<> options) {
      options.error_policy.error_limit = 1;
      return options;
    });

  const auto result = load(
    omni::type_t<std::vector<diagnostic_inputs::embedded_document>>{},
    R"(["[unterminated", "{unterminated"])");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(first.error() + '\n' + second.error(), result.diagnostics);
}

struct issue_view {
  omni::ryml::issue *first;
  omni::ryml::issue *last;

  omni::ryml::issue *begin() const { return this->first; }
  omni::ryml::issue *end() const { return this->last; }
};

TEST(mapping_diagnostics, reuses_relative_paths_from_a_borrowed_issue_range) {
  const auto tree = omni::ryml::core::parse(R"({"outer": {}})");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  auto missing = omni::ryml::core::make_issue(
    omni::json::issue_code::missing_field,
    omni::json::value_kind::unknown, ::ryml::ConstNodeRef{});
  missing.path.push_back({"child", omni::compat::nullopt});
  const issue_view errors{&missing, &missing + 1};
  const auto mapping = omni::ryml::compose::map_object_node(
    [errors](::ryml::ConstNodeRef) -> omni::compat::expected<int, issue_view> {
      return omni::compat::unexpected<issue_view>{errors};
    });

  const omni::ryml::config_t<> options{};
  for (int attempt = 0; attempt < 2; ++attempt) {
    SCOPED_TRACE(attempt);
    const auto result = serialization_test::map_using(options, mapping,
      omni::type_t<int>{}, tree->crootref()["outer"]);

    EXPECT_FALSE(result.value);
    EXPECT_EQ("/outer/child: missing field",
      omni::ryml::render_diagnostics(*tree, result.diagnostics));
  }
}

struct lvalue_renderer {
  std::string operator()(const ::ryml::Tree &,
    const omni::ryml::diagnostics &) & { return "lvalue"; }
};

struct rvalue_renderer {
  std::string operator()(const ::ryml::Tree &,
    const omni::ryml::diagnostics &) && { return "rvalue"; }
};

TEST(deserialize_binding, preserves_lvalue_config_invocability) {
  using config_type = omni::ryml::config_t<lvalue_renderer>;
  using binding_type = omni::ryml::deserialize_t<config_type>;

  EXPECT_TRUE((omni::compat::is_invocable<binding_type &,
    omni::type_t<int>, std::string>::value));

  EXPECT_FALSE((omni::compat::is_invocable<const binding_type &,
    omni::type_t<int>, std::string>::value));

  EXPECT_FALSE((omni::compat::is_invocable<binding_type &&,
    omni::type_t<int>, std::string>::value));
}

TEST(deserialize_binding, preserves_rvalue_config_invocability) {
  using config_type = omni::ryml::config_t<rvalue_renderer>;
  using binding_type = omni::ryml::deserialize_t<config_type>;

  EXPECT_FALSE((omni::compat::is_invocable<binding_type &,
    omni::type_t<int>, std::string>::value));

  EXPECT_FALSE((omni::compat::is_invocable<const binding_type &,
    omni::type_t<int>, std::string>::value));

  EXPECT_TRUE((omni::compat::is_invocable<binding_type &&,
    omni::type_t<int>, std::string>::value));
}

struct move_only_renderer {
  std::unique_ptr<std::string> prefix;

  std::string operator()(const ::ryml::Tree &tree,
    const omni::ryml::diagnostics &report) const {
    return *this->prefix + omni::ryml::render_diagnostics(tree, report);
  }
};

TEST(diagnostics_transform, moves_retained_state_through_chained_updates) {
  using config_type = omni::ryml::config_t<move_only_renderer>;
  const auto load = omni::ryml::deserialize_t<config_type>{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
    /*base=*/{},
#endif
    /*config=*/{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      /*error_policy=*/{},
      /*transform_diagnostics=*/{
        /*prefix=*/omni::compat::make_unique<std::string>("custom: "),
      },
    },
  }
    .tdm_config([](config_type options) {
      return std::move(options).tdm_transform_diagnostics(
        [](move_only_renderer renderer) {
          *renderer.prefix += "updated: ";
          return renderer;
        });
    })
    .tdm_config([](config_type options) { return options; });

  const auto result = load(omni::type_t<int>{}, "bad");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("custom: updated: <root>: expected integer, found scalar",
    result.diagnostics);
}

TEST(diagnostics_transform, decorates_the_current_callable) {
  const omni::ryml::config_t<> original{};
  const auto options = original.tdm_transform_diagnostics(
    [](omni::ryml::render_diagnostics_t previous) {
      return [previous](const ::ryml::Tree &tree,
        omni::ryml::diagnostics report) {
        return "updated: " + previous(tree, report);
      };
    });

  const auto load = omni::ryml::deserialize.tdm_config(
    [options](omni::ryml::config_t<>) { return options; });

  const auto result = load(omni::type_t<int>{}, "bad");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("updated: <root>: expected integer, found scalar",
    result.diagnostics);
  EXPECT_EQ("<root>: expected integer, found scalar",
    omni::ryml::deserialize(omni::type_t<int>{}, "bad").diagnostics);
}

TEST(diagnostics_transform, receives_the_tree_before_it_is_released) {
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_t<> options) {
      return options.tdm_transform_diagnostics(
        [](omni::ryml::render_diagnostics_t) {
          return [](const ::ryml::Tree &tree, omni::ryml::diagnostics report)
            -> std::vector<std::string> {
            std::vector<std::string> values;
            for (const auto &issue : report.issues) {
              const auto value = tree.val(issue.node_id);
              values.emplace_back(value.str, value.len);
            }

            return values;
          };
        });
    });

  const auto result = load(omni::type_t<int>{}, "bad");

  EXPECT_FALSE(result.value);
  EXPECT_EQ((std::vector<std::string>{"bad"}), result.diagnostics);
}

TEST(diagnostics_transform, reports_parse_errors_without_a_valid_tree) {
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_t<> options) {
      return options.tdm_transform_diagnostics(
        [](omni::ryml::render_diagnostics_t) {
          return [](const ::ryml::Tree &tree, omni::ryml::diagnostics report) {
            return tree.empty() ? report.parse_error_message : std::string{};
          };
        });
    });

  const auto result = load(omni::type_t<int>{}, "[unterminated");

  EXPECT_FALSE(result.value);
  EXPECT_FALSE(result.diagnostics.empty());
}

TEST(diagnostics_transform, retains_mutable_renderer_state) {
  auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_t<> options) {
      int calls = 0;
      return options.tdm_transform_diagnostics(
        [calls](omni::ryml::render_diagnostics_t) mutable {
          return [calls](const ::ryml::Tree &, omni::ryml::diagnostics) mutable {
            return ++calls;
          };
        });
    });

  const auto first = load(omni::type_t<int>{}, "7");
  const auto second = load(omni::type_t<int>{}, "bad");

  EXPECT_TRUE(first.value);
  EXPECT_EQ(1, first.diagnostics);
  EXPECT_FALSE(second.value);
  EXPECT_EQ(2, second.diagnostics);
}

TEST(diagnostics_transform, preserves_policies_and_the_original_binding) {
  const auto limited = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_t<> options) {
      options.error_policy.error_limit = 2;
      return options;
    });

  const auto configured = limited.tdm_config(
    [](omni::ryml::config_t<> options) {
      return options.tdm_transform_diagnostics(
        [](omni::ryml::render_diagnostics_t) {
          return [](const ::ryml::Tree &, omni::ryml::diagnostics report) {
            return report.issues.size();
          };
        });
    });

  EXPECT_EQ(2, configured.config.error_policy.error_limit);
  EXPECT_EQ(2, limited.config.error_policy.error_limit);
  EXPECT_EQ(0, omni::ryml::deserialize.config.error_policy.error_limit);
  EXPECT_EQ(1U, configured(omni::type_t<int>{}, "bad").diagnostics);
  EXPECT_EQ("<root>: expected integer, found scalar",
    limited(omni::type_t<int>{}, "bad").diagnostics);
}

TEST(diagnostics_transform, accepts_named_mutable_configurators) {
  int calls = 0;
  auto configure = [calls](omni::ryml::config_t<> options) mutable {
    options.error_policy.error_limit = ++calls;
    return options;
  };

  const auto first = omni::ryml::deserialize.tdm_config(configure);
  const auto second = omni::ryml::deserialize.tdm_config(configure);

  EXPECT_EQ(1, first.config.error_policy.error_limit);
  EXPECT_EQ(2, second.config.error_policy.error_limit);
}

TEST(diagnostics_transform, renders_a_caller_owned_tree) {
  const auto tree = omni::ryml::core::parse("bad");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto result = omni::ryml::map_by_schema(omni::type_t<int>{}, *tree);

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  EXPECT_EQ("<root>: expected integer, found scalar",
    omni::ryml::render_diagnostics(*tree, result.diagnostics));
}

TEST(diagnostics_transform, renders_a_scalar_kind_mismatch) {
  const auto tree = omni::ryml::core::parse("[1, 2]");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const omni::ryml::diagnostics report{
    /*config=*/{},
    /*issues=*/{
      omni::ryml::core::make_issue(omni::json::issue_code::unexpected_kind,
        omni::json::value_kind::scalar,
        tree->crootref()),
    },
    /*parse_error_message=*/{},
  };

  EXPECT_EQ("<root>: expected scalar, found array",
    omni::ryml::render_diagnostics(*tree, report));
}

TEST(diagnostics_transform, configures_a_diagnostics_result) {
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config_t<> options) {
      return options.tdm_transform_diagnostics(
        [](omni::ryml::render_diagnostics_t) {
          return [](const ::ryml::Tree &, omni::ryml::diagnostics report) {
            return report.issues.size();
          };
        });
    });

  EXPECT_EQ(0U, load(omni::type_t<int>{}, "7").diagnostics);
  EXPECT_EQ(1U, load(omni::type_t<int>{}, "bad").diagnostics);
}

// Designated config needs aggregate CTAD as well as designated initializers.
// Clang 17 added aggregate CTAD; its shared feature macro also covers
// alias CTAD.
#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers \
  && ((defined(__cpp_deduction_guides) && 201907L <= __cpp_deduction_guides) \
    || (defined(__clang__) && 17 <= __clang_major__))
TEST(diagnostics_transform, accepts_designated_callable_configuration) {
  const auto load = omni::ryml::deserialize_t{
    .config = omni::ryml::config_t{
      .transform_diagnostics =
        [](const ::ryml::Tree &, omni::ryml::diagnostics report) {
          return report.issues.size();
        },
    },
  };

  EXPECT_EQ(0U, load(omni::type_t<int>{}, "7").diagnostics);
  EXPECT_EQ(1U, load(omni::type_t<int>{}, "bad").diagnostics);
}
#endif

} // namespace
