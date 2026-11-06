#include "json_match.hpp"

#include <omnirefl/functional.hpp>

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace {

struct stateful_transform {
  int next;

  ::ryml::Tree operator()(::ryml::Tree tree) {
    tree.rootref().set_serialized(next++);
    return tree;
  }
};

struct consuming_transform {
  std::unique_ptr<int> value;

  ::ryml::Tree operator()(::ryml::Tree tree) && {
    tree.rootref().set_serialized(*value);
    value.reset();
    return tree;
  }
};

TEST(core_deserialization_tree_transform, accepts_an_identity_transform) {
  omni::ryml::config options;
  options.transform_tree = omni::fn::identity{};
  const auto result = omni::ryml::core::deserialize(
    options, omni::type_t<int>{}, "7");

  EXPECT_EQ(omni::compat::make_optional(7), result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
}

TEST(core_deserialization_tree_transform, invokes_the_original_mutable_lvalue) {
  stateful_transform transform{7};
  omni::ryml::config options;
  options.transform_tree = std::ref(transform);
  const auto first = omni::ryml::core::deserialize(
    options, omni::type_t<int>{}, "0");
  const auto second = omni::ryml::core::deserialize(
    options, omni::type_t<int>{}, "0");

  EXPECT_EQ(omni::compat::make_optional(7), first.value);
  EXPECT_EQ(omni::compat::make_optional(8), second.value);
  EXPECT_EQ(9, transform.next);
}

TEST(core_deserialization_tree_transform,
  invokes_a_borrowed_move_only_callable) {
  consuming_transform transform{omni::compat::make_unique<int>(42)};
  omni::ryml::config options;
  options.transform_tree = [&transform](::ryml::Tree tree) {
    return std::move(transform)(std::move(tree));
  };

  const auto result = omni::ryml::core::deserialize(
    options, omni::type_t<int>{}, "7");

  EXPECT_EQ(omni::compat::make_optional(42), result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  EXPECT_FALSE(transform.value);
}

TEST(core_deserialization_tree_transform,
  parse_failure_preserves_the_callable) {
  consuming_transform transform{omni::compat::make_unique<int>(42)};
  omni::ryml::config options;
  options.transform_tree = [&transform](::ryml::Tree tree) {
    return std::move(transform)(std::move(tree));
  };

  const auto result = omni::ryml::core::deserialize(
    options, omni::type_t<int>{}, "[unterminated");

  EXPECT_FALSE(result.value);
  EXPECT_TRUE(transform.value);
  if (transform.value) {
    EXPECT_EQ(42, *transform.value);
  }
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (1U == result.diagnostics.issues.size()) {
    EXPECT_EQ(omni::json::issue_code::parse_error,
      result.diagnostics.issues.front().reason);
  }
}

TEST(configuration_transform, applies_directly_to_config) {
  const auto configure = omni::ryml::with_transform_tree([](::ryml::Tree tree) {
    tree.rootref().set_serialized(42);
    return tree;
  });
  omni::ryml::config original;
  original.error_policy.error_limit = 2;
  const auto configured = configure(original);

  const auto result = omni::ryml::core::deserialize(
    configured, omni::type_t<int>{}, "7");
  const auto unchanged = omni::ryml::core::deserialize(
    original, omni::type_t<int>{}, "7");

  EXPECT_EQ(omni::compat::make_optional(42), result.value);
  EXPECT_EQ(omni::compat::make_optional(7), unchanged.value);
  EXPECT_EQ(2, configured.error_policy.error_limit);
}

#if defined(__cpp_constexpr) && 201603L <= __cpp_constexpr
TEST(configuration_transform, constructs_constexpr_transformations) {
  constexpr auto replace =
    omni::ryml::with_transform_tree(omni::fn::identity{});
  constexpr auto decorate = omni::ryml::with_transform_tree(
    [](::ryml::Tree tree,
      const std::function<::ryml::Tree(::ryml::Tree)> &previous) {
      return previous(std::move(tree));
    });

  const auto configured = decorate(replace(omni::ryml::config{}));
  const auto result = omni::ryml::core::deserialize(
    configured, omni::type_t<int>{}, "7");

  EXPECT_EQ(omni::compat::make_optional(7), result.value);
}
#endif

TEST(deserialization_tree_transform, accepts_a_forwarding_identity) {
  const auto load = omni::ryml::deserialize.transform_config(
    omni::ryml::with_transform_tree(omni::fn::identity{}));

  EXPECT_EQ(omni::compat::make_optional(7),
    load(omni::type_t<int>{}, "7").value);
}

class owning_tree_transform: public testing::TestWithParam<bool> {};

TEST_P(owning_tree_transform, retains_captured_state) {
  auto value = std::make_shared<const int>(42);
  const std::weak_ptr<const int> lifetime{value};
  {
    const auto load = /*decorator_signature=*/GetParam()
      ? omni::ryml::deserialize.transform_config(
          omni::ryml::with_transform_tree([value](::ryml::Tree tree,
            const std::function<::ryml::Tree(::ryml::Tree)> &previous) {
            tree = previous(std::move(tree));
            tree.rootref().set_serialized(*value);
            return tree;
          }))
      : omni::ryml::deserialize.transform_config(
          omni::ryml::with_transform_tree([value](::ryml::Tree tree) {
            tree.rootref().set_serialized(*value);
            return tree;
          }));

    value.reset();
    EXPECT_FALSE(lifetime.expired());
    const auto result = load(omni::type_t<int>{}, "7");

    EXPECT_EQ(omni::compat::make_optional(42), result.value);
    EXPECT_TRUE(result.diagnostics.issues.empty());
  }

  EXPECT_TRUE(lifetime.expired());
}

INSTANTIATE_TEST_SUITE_P(signatures,
  owning_tree_transform,
  testing::Bool(),
  [](const testing::TestParamInfo<bool> &p) {
    return p.param ? "decorator" : "unary";
  });

TEST(deserialization_tree_transform, defers_execution_and_preserves_source) {
  int calls = 0;
  const auto load = omni::ryml::deserialize.transform_config(
    omni::ryml::with_transform_tree([&calls](::ryml::Tree) {
      ++calls;
      return omni::ryml::core::as_owning_tree(42);
    }));

  EXPECT_EQ(0, calls);

  const auto original = omni::ryml::deserialize(omni::type_t<int>{}, "7");
  const auto first = load(omni::type_t<int>{}, "7");
  const auto second = load(omni::type_t<int>{}, "8");

  EXPECT_EQ(omni::compat::make_optional(7), original.value);
  EXPECT_EQ(omni::compat::make_optional(42), first.value);
  EXPECT_EQ(omni::compat::make_optional(42), second.value);
  EXPECT_EQ(2, calls);
}

TEST(deserialization_tree_transform, unary_replaces_previous) {
  const auto previous = omni::ryml::deserialize.transform_config(
    omni::ryml::with_transform_tree([](::ryml::Tree) {
      return omni::ryml::core::as_owning_tree(42);
    }));
  const auto replacement = previous.transform_config(
    omni::ryml::with_transform_tree([](::ryml::Tree tree) { return tree; }));

  EXPECT_EQ(omni::compat::make_optional(7),
    replacement(omni::type_t<int>{}, "7").value);
  EXPECT_EQ(omni::compat::make_optional(42),
    previous(omni::type_t<int>{}, "7").value);
}

TEST(deserialization_tree_transform,
  null_unary_replaces_previous_with_identity) {
  const auto previous = omni::ryml::deserialize.transform_config(
    omni::ryml::with_transform_tree([](::ryml::Tree) {
      return omni::ryml::core::as_owning_tree(42);
    }));
  ::ryml::Tree (*transform)(::ryml::Tree) = nullptr;
  const auto load = previous.transform_config(
    omni::ryml::with_transform_tree(transform));

  EXPECT_EQ(omni::compat::make_optional(7),
    load(omni::type_t<int>{}, "7").value);
  EXPECT_EQ(omni::compat::make_optional(42),
    previous(omni::type_t<int>{}, "7").value);
}

TEST(deserialization_tree_transform, null_decorator_resets_to_identity) {
  const auto previous = omni::ryml::deserialize.transform_config(
    omni::ryml::with_transform_tree([](::ryml::Tree) {
      return omni::ryml::core::as_owning_tree(42);
    }));
  ::ryml::Tree (*decorate)(::ryml::Tree,
    const std::function<::ryml::Tree(::ryml::Tree)> &) = nullptr;
  const auto load = previous.transform_config(
    omni::ryml::with_transform_tree(decorate));

  EXPECT_EQ(omni::compat::make_optional(7),
    load(omni::type_t<int>{}, "7").value);
  EXPECT_EQ(omni::compat::make_optional(42),
    previous(omni::type_t<int>{}, "7").value);
}

#if defined(__cpp_noexcept_function_type)
TEST(deserialization_tree_transform,
  null_noexcept_decorator_resets_to_identity) {
  const auto previous = omni::ryml::deserialize.transform_config(
    omni::ryml::with_transform_tree([](::ryml::Tree) {
      return omni::ryml::core::as_owning_tree(42);
    }));
  ::ryml::Tree (*decorate)(::ryml::Tree,
    const std::function<::ryml::Tree(::ryml::Tree)> &) noexcept = nullptr;
  const auto load = previous.transform_config(
    omni::ryml::with_transform_tree(decorate));

  EXPECT_EQ(omni::compat::make_optional(7),
    load(omni::type_t<int>{}, "7").value);
}
#endif

TEST(deserialization_tree_transform, decorator_receives_identity_by_default) {
  const auto load = omni::ryml::deserialize.transform_config(
    omni::ryml::with_transform_tree([](::ryml::Tree tree,
      const std::function<::ryml::Tree(::ryml::Tree)> &previous) {
      return previous(std::move(tree));
    }));

  EXPECT_EQ(omni::compat::make_optional(7),
    load(omni::type_t<int>{}, "7").value);
}

TEST(deserialization_tree_transform, decorator_precedes_unary_when_both_match) {
  struct transform {
    ::ryml::Tree operator()(::ryml::Tree tree,
      const std::function<::ryml::Tree(::ryml::Tree)> &previous = {}) const {
      return previous ? previous(std::move(tree)) : std::move(tree);
    }
  };

  const auto load = omni::ryml::deserialize.transform_config(
    omni::ryml::with_transform_tree([](::ryml::Tree) {
      return omni::ryml::core::as_owning_tree(42);
    }))
    .transform_config(omni::ryml::with_transform_tree(transform{}));

  EXPECT_EQ(omni::compat::make_optional(42),
    load(omni::type_t<int>{}, "7").value);
}

TEST(deserialization_tree_transform, decorator_can_invoke_previous) {
  const auto load = omni::ryml::deserialize.transform_config(
    omni::ryml::with_transform_tree([](::ryml::Tree) {
      return omni::ryml::core::as_owning_tree(7);
    }))
    .transform_config(
      omni::ryml::with_transform_tree([](::ryml::Tree tree,
        const std::function<::ryml::Tree(::ryml::Tree)> &previous) {
        tree = previous(std::move(tree));
        int value = 0;
        tree.crootref().load(&value);
        tree.rootref().set_serialized(2 * value);
        return tree;
      }));

  EXPECT_EQ(omni::compat::make_optional(14),
    load(omni::type_t<int>{}, "3").value);
}

TEST(deserialization_tree_transform, decorator_can_ignore_previous) {
  const auto load = omni::ryml::deserialize.transform_config(
    omni::ryml::with_transform_tree([](::ryml::Tree) {
      return omni::ryml::core::as_owning_tree(42);
    }))
    .transform_config(
      omni::ryml::with_transform_tree([](::ryml::Tree tree,
        const std::function<::ryml::Tree(::ryml::Tree)> &) { return tree; }));

  EXPECT_EQ(omni::compat::make_optional(7),
    load(omni::type_t<int>{}, "7").value);
}

TEST(deserialization_tree_transform, retains_mutable_callable_state) {
  const auto load = omni::ryml::deserialize.transform_config(
    omni::ryml::with_transform_tree(stateful_transform{7}));

  EXPECT_EQ(omni::compat::make_optional(7),
    load(omni::type_t<int>{}, "0").value);
  EXPECT_EQ(omni::compat::make_optional(8),
    load(omni::type_t<int>{}, "0").value);
}

class tree_transform_config: public testing::TestWithParam<bool> {};

TEST_P(tree_transform_config, preserves_both_customizations) {
  const auto transform = [](::ryml::Tree) {
    return omni::ryml::core::as_owning_tree(42);
  };
  const auto configure = [](omni::ryml::config config) {
    config.error_policy.error_limit = 2;
    return config;
  };
  const auto load = GetParam()
    ? omni::ryml::deserialize
        .transform_config(omni::ryml::with_transform_tree(transform))
        .transform_config(configure)
    : omni::ryml::deserialize.transform_config(configure)
        .transform_config(omni::ryml::with_transform_tree(transform));

  const auto result = load(omni::type_t<int>{}, "7");

  EXPECT_EQ(omni::compat::make_optional(42), result.value);
  EXPECT_EQ(2, result.diagnostics.config.error_policy.error_limit);
  EXPECT_EQ(0, omni::ryml::deserialize.config.error_policy.error_limit);
}

INSTANTIATE_TEST_SUITE_P(binding_order,
  tree_transform_config,
  testing::Bool(),
  [](const testing::TestParamInfo<bool> &p) {
    return p.param ? "transform_first" : "config_first";
  });

TEST(deserialization_tree_transform, parse_errors_skip_transform) {
  int calls = 0;
  const auto load = omni::ryml::deserialize.transform_config(
    omni::ryml::with_transform_tree([&calls](::ryml::Tree tree) {
      ++calls;
      return tree;
    }));

  const auto result = load(omni::type_t<int>{}, "[unterminated");

  EXPECT_FALSE(result.value);
  EXPECT_EQ(0, calls);
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (1U == result.diagnostics.issues.size()) {
    EXPECT_EQ(omni::json::issue_code::parse_error,
      result.diagnostics.issues.front().reason);
  }
}

TEST(deserialization_tree_transform, diagnostics_retain_transformed_storage) {
  const auto result = omni::ryml::deserialize.transform_config(
    omni::ryml::with_transform_tree([](::ryml::Tree) {
      return omni::ryml::core::as_owning_tree(std::string{"bad"});
    }))(omni::type_t<int>{}, "7");

  EXPECT_FALSE(result.value);
  EXPECT_TRUE(serialization_test::matches_node(
    R"("bad")", result.diagnostics.tree.crootref()));
  EXPECT_EQ(1U, result.diagnostics.issues.size());
  if (1U == result.diagnostics.issues.size()) {
    const auto &issue = result.diagnostics.issues.front();
    EXPECT_EQ(omni::json::issue_code::invalid_scalar, issue.reason);
    EXPECT_EQ(c4::csubstr{"bad"}, issue.value);
  }
}

} // namespace
