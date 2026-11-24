#include "../json_match.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

// Each direction discovers its own enum through the vector root.
enum class read_status { ready, busy };
enum class write_status { ready, busy };

TEST(deserialization_sequence_roots, reads_an_enum_vector_from_strings) {
  const auto tree = omni::ryml::core::parse(R"(["ready", "busy", "ready"])");
  EXPECT_TRUE(tree);
  if (!tree)
    return;

  const auto result = omni::ryml::map_by_schema(
    omni::type_t<std::vector<read_status>>{}, *tree);

  const std::vector<read_status> expected{
    read_status::ready,
    read_status::busy,
    read_status::ready,
  };

  EXPECT_EQ(expected, result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
}

TEST(serialization_sequence_roots, writes_an_enum_vector_as_strings) {
  const std::vector<write_status> value{
    write_status::ready,
    write_status::busy,
    write_status::ready,
  };

  const auto tree = omni::ryml::as_owning_tree(value);

  EXPECT_TRUE(serialization_test::matches_node(
    R"(["ready", "busy", "ready"])", tree.crootref()));
}

} // namespace
