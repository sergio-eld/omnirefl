#include <omnirefl/serialization/ryml.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace {
namespace optional_mapping {

// Keep a viable has_value() default to exercise conditional specialization.
// Dereference, reset(), and emplace() require adapters.
template <typename T>
struct nullable {
  using value_type = T;

  bool absent = true;
  T contents{};

  bool is_null() const { return this->absent; }
  bool has_value() const { return !this->absent; }
  const T &get() const { return this->contents; }
  void clear() { this->absent = true; }
  void replace(T value) {
    this->contents = std::move(value);
    this->absent = false;
  }
};

struct payload { int value; };

} // namespace optional_mapping
} // namespace

namespace omni {
namespace traits {
namespace cp {

template <typename T>
struct has_value<T,
  compat::void_t<decltype(std::declval<const T &>().is_null())>> {
  bool operator()(const T &value) const {
    return !value.is_null();
  }
};

template <typename T>
struct get_value<optional_mapping::nullable<T>> {
  const T &operator()(const optional_mapping::nullable<T> &value) const {
    return value.get();
  }
};

template <typename T>
struct reset<optional_mapping::nullable<T>> {
  void operator()(optional_mapping::nullable<T> *into) const { into->clear(); }
};

template <typename T>
struct emplace<optional_mapping::nullable<T>> {
  void operator()(optional_mapping::nullable<T> *into, T &&value) const {
    into->replace(std::move(value));
  }
};

} // namespace cp
} // namespace traits
} // namespace omni

namespace {

TEST(optional_mapping, detects_adapted_operations_without_registration) {
  EXPECT_TRUE(omni::traits::is_optional<
    optional_mapping::nullable<int>>::value);
  EXPECT_TRUE(omni::traits::is_optional<
    omni::compat::optional<int>>::value);
  EXPECT_FALSE(omni::traits::is_optional<int *>::value);
  EXPECT_FALSE(omni::traits::is_optional<std::unique_ptr<int>>::value);
}

TEST(optional_mapping, discovers_and_maps_the_contained_record) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<optional_mapping::nullable<optional_mapping::payload>>{},
    R"({"value": 42})");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_FALSE(result.value->is_null());
    EXPECT_EQ(42, result.value->get().value);
    EXPECT_EQ(R"({"value": 42})", omni::ryml::as_json(*result.value));
  }
}

TEST(optional_mapping, maps_null_as_a_disengaged_optional) {
  const auto result = omni::ryml::deserialize(
    omni::type_t<optional_mapping::nullable<int>>{}, "null");

  EXPECT_TRUE(result.value);
  EXPECT_TRUE(result.diagnostics.issues.empty());
  if (result.value) {
    EXPECT_TRUE(result.value->is_null());
    EXPECT_EQ("null", omni::ryml::as_json(*result.value));
  }
}

TEST(optional_mapping, omits_disengaged_elements_from_sequences) {
  optional_mapping::nullable<int> present{};
  present.replace(42);
  const std::vector<optional_mapping::nullable<int>> values{{}, present, {}};

  EXPECT_EQ("[42]", omni::ryml::as_json(values));
}

} // namespace
