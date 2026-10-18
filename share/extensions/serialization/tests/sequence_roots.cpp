#include <omnirefl/serialization/ryml.hpp>

#include <gtest/gtest.h>

#include <vector>

namespace {

// This record appears only inside root containers, so a direct record call
// elsewhere in the translation unit cannot supply its reflection metadata.
struct pixel_sample {
  unsigned column;
  double alpha;
};

TEST(serialization_sequence_roots, discovers_nested_record_elements) {
  const auto parsed = omni::ryml::parse(omni::ryml::as_json(
    std::vector<std::vector<pixel_sample>>{{pixel_sample{7, 0.5}}}));

  EXPECT_TRUE(parsed);
  if (!parsed)
    return;
  const auto root = parsed->crootref();
  EXPECT_TRUE(root.is_seq());
  if (!root.is_seq())
    return;
  EXPECT_EQ(1U, root.num_children());
  if (1U != root.num_children())
    return;
  EXPECT_TRUE(root[0].is_seq());
  if (!root[0].is_seq())
    return;
  EXPECT_EQ(1U, root[0].num_children());
  if (1U != root[0].num_children())
    return;
  EXPECT_TRUE(root[0][0].is_map());
  if (!root[0][0].is_map())
    return;
  EXPECT_TRUE(root[0][0].has_child("column"));
  EXPECT_TRUE(root[0][0].has_child("alpha"));
  if (!root[0][0].has_child("column") || !root[0][0].has_child("alpha"))
    return;
  EXPECT_EQ("7", root[0][0]["column"].val());
  EXPECT_EQ("0.5", root[0][0]["alpha"].val());
}

TEST(serialization_sequence_roots, writes_empty_record_array) {
  const auto parsed =
    omni::ryml::parse(omni::ryml::as_json(std::vector<pixel_sample>{}));

  EXPECT_TRUE(parsed);
  if (!parsed)
    return;
  EXPECT_TRUE(parsed->crootref().is_seq());
  EXPECT_EQ(0U, parsed->crootref().num_children());
}

} // namespace
