#include "json_match.hpp"
#include "local_mapping.hpp"

#include <gtest/gtest.h>

#include <utility>

namespace {

TEST(mapping_dispatch, deserialize_selection_override) {
  const auto customize = [](omni::ryml::reflected_scope::mapping_to_type_t) {
    return local_mapping::select_mapping{5};
  };
  const auto deserialize = omni::ryml::deserialize
    .tdm_select_mapping(customize);
  const auto result = deserialize(omni::type_t<local_mapping::record>{},
    R"({"value": 7})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.empty());
  if (result.value)
    EXPECT_EQ(12, result.value->value);
}

namespace local_write_mapping {

struct add_integer {
  int increment;

  template <typename State>
  auto operator()(State state, const int &value) const -> ::ryml::NodeRef {
    return omni::ryml::map_integral(std::move(state), value + this->increment);
  }
};

struct select_mapping {
  int increment;

  template <typename Path, typename T>
  auto operator()(const Path &path, omni::type_t<T> type) const
    -> decltype(omni::ryml::reflected_scope::fn::mapping_from_type(
      path, type)) {
    return omni::ryml::reflected_scope::fn::mapping_from_type(path, type);
  }

  template <typename Path>
  add_integer operator()(Path, omni::type_t<int>) const {
    return {this->increment};
  }
};

} // namespace local_write_mapping

TEST(mapping_dispatch, serialize_selection_override) {
  const auto as_json = omni::ryml::as_json.tdm_select_mapping(
    [](omni::ryml::reflected_scope::mapping_from_type_t) {
      return local_write_mapping::select_mapping{5};
    });

  EXPECT_TRUE(serialization_test::matches_json(
    R"({"value": 12})", as_json(local_mapping::record{7})));
}

} // namespace
