#include <omnirefl/serialization/ryml.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

TEST(read_state, identifies_read_state_specializations) {
  EXPECT_TRUE(omni::ryml::is_read_state<omni::ryml::read_state_t<>>::value);
  EXPECT_TRUE(omni::ryml::is_read_state<
    const omni::ryml::read_state_t<> &>::value);
  EXPECT_FALSE(omni::ryml::is_read_state<omni::ryml::write_state_t<false>>::value);
  EXPECT_FALSE(omni::ryml::is_read_state<omni::ryml::config_t<>>::value);
}

TEST(write_state, identifies_write_state_specializations) {
  EXPECT_TRUE(omni::ryml::is_write_state<omni::ryml::write_state_t<false>>::value);
  EXPECT_TRUE(omni::ryml::is_write_state<
    const omni::ryml::write_state_t<true> &>::value);
  EXPECT_FALSE(omni::ryml::is_write_state<omni::ryml::read_state_t<>>::value);
  EXPECT_FALSE(omni::ryml::is_write_state<omni::ryml::config_t<>>::value);
}

TEST(read_state, copies_borrow_config_and_keep_independent_counters) {
  const omni::ryml::config_t<> options{};
  using state_type = omni::ryml::read_state_t<>;
  const state_type original{
    /*config=*/options,
    /*error_count=*/2,
    /*warning_count=*/3,
  };

  auto copied = original;
  ++copied.error_count;

  EXPECT_TRUE(std::is_trivially_copyable<state_type>::value);
  EXPECT_EQ(&options, &copied.config);
  EXPECT_EQ(2U, original.error_count);
  EXPECT_EQ(3U, copied.error_count);
  EXPECT_EQ(3U, copied.warning_count);
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

struct lvalue_renderer {
  std::string operator()(const ::ryml::Tree &,
    const omni::ryml::diagnostics &) & { return "lvalue"; }
};

struct rvalue_renderer {
  std::string operator()(const ::ryml::Tree &,
    const omni::ryml::diagnostics &) && { return "rvalue"; }
};

template <typename Config, typename = void>
struct is_core_deserializable: std::false_type {};

template <typename Config>
struct is_core_deserializable<Config,
  omni::compat::void_t<decltype(omni::ryml::core::deserialize(
    std::declval<Config>(), omni::type_t<int>{}, std::string{}))>>:
    std::true_type {};

TEST(deserialize_binding, preserves_lvalue_config_invocability) {
  using config_type = omni::ryml::config_t<lvalue_renderer>;
  using binding_type = omni::ryml::deserialize_t<config_type>;

  EXPECT_TRUE(is_core_deserializable<config_type &>::value);
  EXPECT_TRUE((omni::compat::is_invocable<binding_type &,
    omni::type_t<int>, std::string>::value));

  EXPECT_FALSE(is_core_deserializable<const config_type &>::value);
  EXPECT_FALSE((omni::compat::is_invocable<const binding_type &,
    omni::type_t<int>, std::string>::value));

  EXPECT_FALSE(is_core_deserializable<config_type &&>::value);
  EXPECT_FALSE((omni::compat::is_invocable<binding_type &&,
    omni::type_t<int>, std::string>::value));
}

TEST(deserialize_binding, preserves_rvalue_config_invocability) {
  using config_type = omni::ryml::config_t<rvalue_renderer>;
  using binding_type = omni::ryml::deserialize_t<config_type>;

  EXPECT_FALSE(is_core_deserializable<config_type &>::value);
  EXPECT_FALSE((omni::compat::is_invocable<binding_type &,
    omni::type_t<int>, std::string>::value));

  EXPECT_FALSE(is_core_deserializable<const config_type &>::value);
  EXPECT_FALSE((omni::compat::is_invocable<const binding_type &,
    omni::type_t<int>, std::string>::value));

  EXPECT_TRUE(is_core_deserializable<config_type &&>::value);
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
    /*config=*/{
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

TEST(diagnostics_transform, renders_by_default) {
  const auto valid = omni::ryml::deserialize(omni::type_t<int>{}, "7");
  const auto invalid = omni::ryml::deserialize(omni::type_t<int>{}, "bad");

  EXPECT_TRUE(valid.value);
  EXPECT_EQ("", valid.diagnostics);
  EXPECT_FALSE(invalid.value);
  EXPECT_EQ("<root>: expected integer, found scalar", invalid.diagnostics);
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

// Designated config needs aggregate CTAD as well as designated initializers.
// Clang 17 added aggregate CTAD; its shared feature macro also covers
// alias CTAD.
#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers \
  && ((defined(__cpp_deduction_guides) && 201907L <= __cpp_deduction_guides) \
    || (defined(__clang__) && 17 <= __clang_major__))
TEST(diagnostics_transform, accepts_designated_callable_configuration) {
  const auto load = omni::ryml::deserialize_t{
    omni::ryml::config_t{
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
