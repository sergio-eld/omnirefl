#include "json_match.hpp"

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
  EXPECT_TRUE(serialization_test::matches_json(
    R"([[{"column":7,"alpha":0.5}]])", omni::ryml::as_json(
      std::vector<std::vector<pixel_sample>>{{pixel_sample{7, 0.5}}})));
}

TEST(serialization_sequence_roots, writes_empty_record_array) {
  EXPECT_TRUE(serialization_test::matches_json("[]",
    omni::ryml::as_json(std::vector<pixel_sample>{})));
}

} // namespace
