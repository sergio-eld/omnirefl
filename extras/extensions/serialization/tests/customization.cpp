#include "json_match.hpp"

#include <tuple>

#if defined(__GNUC__) && !defined(__clang__)
// GCC warns about GTest's internal conditionals in unbraced assertion guards.
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdangling-else"
#endif

namespace {

template <typename T>
struct endpoint { T port; };

} // namespace

namespace omni {
namespace ryml {
namespace cp {

// C++14 supports variable-template registration and partial specialization.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <typename T>
const auto to_type<endpoint<T>> = reflected_scope::to_record.with_name_by_alias(
  alias_dict(std::make_tuple("port", "service_port")));
#else
// C++11 registers a struct and configures the default compositor at invocation.
template <typename T>
struct to_type_t<endpoint<T>> {
  auto operator()(::ryml::ConstNodeRef node,
    endpoint<T> *into,
    const detail::mapping_context &context) const
    -> with_diagnostics<compat::monostate, diagnostics> {
    return reflected_scope::to_record.with_name_by_alias(
      [](compat::string_view alias) -> compat::optional<compat::string_view> {
        return alias == "service_port"
          ? compat::optional<compat::string_view>{"port"}
          : compat::nullopt;
      })(node, into, context);
  }
};
#endif

} // namespace cp
} // namespace ryml
} // namespace omni

namespace {

TEST(mapping_customization, preserves_configured_alias_lookup) {
  const auto result = serialization_test::deserialize(
    omni::type_t<endpoint<int>>{}, R"({"service_port": 42})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value)
    EXPECT_EQ(42, result.value->port);
}

} // namespace

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
