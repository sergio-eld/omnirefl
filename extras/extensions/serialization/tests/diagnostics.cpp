#include <omnirefl/serialization/ryml.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

TEST(mapping_state, copies_borrow_config_and_keep_independent_paths) {
  const omni::ryml::config<> options{};
  using state_type = omni::ryml::mapping_state<>;
  const state_type root{
    /*config=*/options,
    /*path=*/{},
    /*error_count=*/2,
    /*warning_count=*/3,
    /*borrow_strings=*/false,
  };

  const auto first = root.child({
    /*field=*/c4::to_csubstr("first"),
    /*index=*/omni::compat::nullopt,
  });

  const auto indexed = first.child({/*field=*/{}, /*index=*/3});
  const auto sibling = root.child({
    /*field=*/c4::to_csubstr("second"),
    /*index=*/omni::compat::nullopt,
  });

  auto copied = first;
  ++copied.error_count;

  EXPECT_TRUE(std::is_trivially_copyable<state_type>::value);
  EXPECT_EQ(&options, &copied.config);
  EXPECT_EQ(&root.path, copied.path.parent);
  EXPECT_EQ(2U, first.error_count);
  EXPECT_EQ(3U, copied.error_count);
  EXPECT_EQ(3U, copied.warning_count);
  EXPECT_TRUE(root.path.materialize().empty());
  EXPECT_EQ("/first/3",
    omni::ryml::detail::render_path(indexed.path.materialize()));

  EXPECT_EQ("/second",
    omni::ryml::detail::render_path(sibling.path.materialize()));
}

TEST(diagnostics_transform, renders_by_default) {
  const auto valid = omni::ryml::deserialize(omni::type_t<int>{}, "7");
  const auto invalid = omni::ryml::deserialize(omni::type_t<int>{}, "bad");

  EXPECT_TRUE(valid.value);
  EXPECT_EQ("", valid.diagnostics);
  EXPECT_FALSE(invalid.value);
  EXPECT_EQ("<root>: \"bad\" is not an integer", invalid.diagnostics);
}

TEST(diagnostics_transform, decorates_the_current_callable) {
  const omni::ryml::config<> original{};
  const auto options = original.tdm_transform_diagnostics(
    [](omni::ryml::render_diagnostics_t previous) {
      return [previous](const ::ryml::Tree &tree,
        omni::ryml::diagnostics report) {
        return "updated: " + previous(tree, report);
      };
    });

  const auto load = omni::ryml::deserialize.tdm_config(
    [options](omni::ryml::config<>) { return options; });

  const auto result = load(omni::type_t<int>{}, "bad");

  EXPECT_FALSE(result.value);
  EXPECT_EQ("updated: <root>: \"bad\" is not an integer", result.diagnostics);
  EXPECT_EQ("<root>: \"bad\" is not an integer",
    omni::ryml::deserialize(omni::type_t<int>{}, "bad").diagnostics);
}

TEST(diagnostics_transform, receives_the_tree_before_it_is_released) {
  const auto load = omni::ryml::deserialize.tdm_config(
    [](omni::ryml::config<> options) {
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
    [](omni::ryml::config<> options) {
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
    [](omni::ryml::config<> options) {
      int calls = 0;
      return options.tdm_transform_diagnostics(
        [calls](omni::ryml::render_diagnostics_t) {
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
    [](omni::ryml::config<> options) {
      options.error_policy.error_limit = 2;
      return options;
    });

  const auto configured = limited.tdm_config(
    [](omni::ryml::config<> options) {
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
  EXPECT_EQ("<root>: \"bad\" is not an integer",
    limited(omni::type_t<int>{}, "bad").diagnostics);
}

TEST(diagnostics_transform, accepts_named_mutable_configurators) {
  int calls = 0;
  auto configure = [calls](omni::ryml::config<> options) mutable {
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

  const auto result = omni::ryml::map_tree(omni::type_t<int>{}, *tree);

  EXPECT_FALSE(result.value);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  EXPECT_EQ("<root>: \"bad\" is not an integer",
    omni::ryml::render_diagnostics(*tree, result.diagnostics));
}

TEST(diagnostics_transform, renders_a_collection_conversion_failure) {
  const auto tree = omni::ryml::core::parse("[1, 2]");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const omni::ryml::diagnostics report{
    /*config=*/{},
    /*issues=*/{
      omni::ryml::detail::make_issue(omni::json::issue_code::out_of_range,
        omni::json::value_kind::sequence,
        tree->crootref(),
        /*path=*/{}),
    },
    /*parse_error_message=*/{},
  };

  EXPECT_EQ("<root>: \"\" could not be converted to the destination scalar",
    omni::ryml::render_diagnostics(*tree, report));
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
    omni::ryml::config{
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
