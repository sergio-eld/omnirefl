#include "completion.hpp"
#include "convert.hpp"
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

// `aggregate_into<T>` targets a niche but recurring form of hand-written
// adaptation between related C++ records. A validated REST envelope, for
// example, can be projected independently into a domain payload aggregate and
// a request context aggregate.
namespace aggregate_test {

struct source {
  std::string name;
  std::unique_ptr<int> payload;
  int count;
  std::string rest_token;
};

struct destination {
  std::unique_ptr<int> payload;
  std::string name;
  int count;
};

struct shared_source {
  std::shared_ptr<int> payload;
};

struct shared_destination {
  std::shared_ptr<int> payload;
};

struct user_identity {
  int id;
};

// C: a flattened transport record carrying both domain and request context.
struct rest_request {
  std::string name;
  std::unique_ptr<int> payload;
  std::unique_ptr<user_identity> identity;
  std::string access_token;
  int permissions;
};

// A: the domain-facing subset, intentionally declared in a different order.
struct domain_payload {
  std::unique_ptr<int> payload;
  std::string name;
};

// B: transport-specific data kept outside the domain model.
struct request_context {
  std::unique_ptr<user_identity> identity;
  int permissions;
  std::string access_token;
};

// A * B: the two independent projections produced from one C record.
struct request_projection {
  domain_payload domain;
  request_context context;
};

struct aggregate_base {
  int inherited;
};

struct based_destination: aggregate_base {
  int own;
};

union union_destination {
  int value;
};

struct has_bases {
  template <typename T>
  constexpr bool operator()(omni::meta_t<T> target) const noexcept {
    return target.has_bases();
  }
};

struct can_aggregate {
  template <typename T>
  constexpr bool operator()(omni::meta_t<T> target) const noexcept {
    return target.is_aggregatable();
  }
};

// Generic C -> A * B adaptation; reflected_call supplies both target schemas.
struct project_request {
  template <typename From, typename Domain, typename Context>
  request_projection operator()(omni::binding_t<From> from,
    omni::meta_t<Domain>,
    omni::meta_t<Context>) const {
    // Each cheap binding tuple consumes only its target's same-named fields.
    return {
      omni::refl::aggregate_into<Domain>(from.public_fields()),
      omni::refl::aggregate_into<Context>(from.public_fields()),
    };
  }
};

struct name_match_source {
  std::string first;
  std::string second;
  int count;
};

struct name_match_destination {
  std::string second;
  std::string first;
  long count;
};

struct required_value {
  required_value() = delete;
  explicit required_value(int value): value{value} {}

  int value;
};

struct non_default_source {
  int value;
};

struct non_default_destination {
  required_value value;
};

struct incomplete_source {
  std::string name;
  std::unique_ptr<int> payload;
};

struct completed_destination {
  ts::optional<std::string> note;
  std::string name;
  std::unique_ptr<int> payload;
  int count;
};

struct convert_with_missing_values {
  template <typename From, typename To>
  constexpr To operator()(omni::binding_t<From> from,
    omni::meta_t<To> target) const {
    return omni::refl::aggregate_into<To>(omni::fn::concat(from.public_fields(),
      target.public_fields() //
        | omni::fn::diff_by(omni::fn::field_name{}, from.public_fields())
        | omni::fn::map(supply_missing{})));
  }
};

} // namespace aggregate_test

TEST(aggregate_into, projects_source_fields_into_destination) {
  namespace aggregate = aggregate_test;

  aggregate::source source{
    "oceanic",
    omni::compat::make_unique<int>(815),
    47,
    "authorized",
  };
  const auto destination = omni::reflected_call(aggregate::convert{},
    std::move(source),
    omni::type_t<aggregate::destination>{});

  ASSERT_NE(nullptr, destination.payload);
  EXPECT_EQ(815, *destination.payload);
  EXPECT_EQ("oceanic", destination.name);
  EXPECT_EQ(47, destination.count);
  EXPECT_EQ(nullptr, source.payload);
  EXPECT_EQ("authorized", source.rest_token);
}

TEST(aggregate_into, projects_rest_request_into_domain_and_context) {
  namespace aggregate = aggregate_test;

  aggregate::rest_request request{
    "oceanic",
    omni::compat::make_unique<int>(815),
    omni::compat::make_unique<aggregate::user_identity>(
      aggregate::user_identity{108}),
    "authorized",
    47,
  };
  // Both destination schemas are made explicit to the reflected scope.
  const auto projection = omni::reflected_call(aggregate::project_request{},
    std::move(request),
    omni::type_t<aggregate::domain_payload>{},
    omni::type_t<aggregate::request_context>{});

  EXPECT_EQ("oceanic", projection.domain.name);
  EXPECT_NE(nullptr, projection.domain.payload);
  if (projection.domain.payload) {
    EXPECT_EQ(815, *projection.domain.payload);
  }
  EXPECT_NE(nullptr, projection.context.identity);
  if (projection.context.identity) {
    EXPECT_EQ(108, projection.context.identity->id);
  }
  EXPECT_EQ(47, projection.context.permissions);
  EXPECT_EQ("authorized", projection.context.access_token);
  EXPECT_EQ(nullptr, request.payload);
  EXPECT_EQ(nullptr, request.identity);
}

TEST(aggregate_into, copies_fields_from_const_source) {
  namespace aggregate = aggregate_test;

  const aggregate::shared_source source{
    std::make_shared<int>(815),
  };
  const auto destination = omni::reflected_call(aggregate::convert{},
    source,
    omni::type_t<aggregate::shared_destination>{});

  EXPECT_NE(nullptr, source.payload);
  EXPECT_EQ(source.payload, destination.payload);
  EXPECT_EQ(2L, source.payload.use_count());
}

#if defined(__cpp_designated_initializers) \
  && 201707L <= __cpp_designated_initializers
TEST(aggregate_into, designated_initialization_matches_fields_by_name) {
#else
TEST(aggregate_into, positional_initialization_matches_fields_by_name) {
#endif
  namespace aggregate = aggregate_test;

  aggregate::name_match_source source{"first", "second", 47};
  const auto destination = omni::reflected_call(aggregate::convert{},
    std::move(source),
    omni::type_t<aggregate::name_match_destination>{});

  EXPECT_EQ("second", destination.second);
  EXPECT_EQ("first", destination.first);
  EXPECT_EQ(47, destination.count);
}

TEST(aggregate_into, reports_base_class_eligibility) {
  namespace aggregate = aggregate_test;

  EXPECT_FALSE(omni::reflected_call(aggregate::has_bases{},
    omni::type_t<aggregate::destination>{}));
  EXPECT_TRUE(omni::reflected_call(aggregate::can_aggregate{},
    omni::type_t<aggregate::destination>{}));
  EXPECT_TRUE(omni::reflected_call(aggregate::has_bases{},
    omni::type_t<aggregate::based_destination>{}));
  EXPECT_FALSE(omni::reflected_call(aggregate::can_aggregate{},
    omni::type_t<aggregate::based_destination>{}));
}

TEST(aggregate_into, reports_union_ineligible_for_aggregation) {
  namespace aggregate = aggregate_test;

  EXPECT_FALSE(omni::reflected_call(aggregate::can_aggregate{},
    omni::type_t<aggregate::union_destination>{}));
}

TEST(aggregate_into, constructs_non_default_destination_field) {
  namespace aggregate = aggregate_test;

  const auto destination = omni::reflected_call(aggregate::convert{},
    aggregate::non_default_source{815},
    omni::type_t<aggregate::non_default_destination>{});

  EXPECT_EQ(815, destination.value.value);
}

TEST(aggregate_into, composes_explicit_values_for_missing_fields) {
  namespace aggregate = aggregate_test;

  aggregate::incomplete_source source{
    "oceanic",
    omni::compat::make_unique<int>(815),
  };
  const auto destination =
    omni::reflected_call(aggregate::convert_with_missing_values{},
      std::move(source),
      omni::type_t<aggregate::completed_destination>{});

  EXPECT_EQ(ts::nullopt, destination.note);
  EXPECT_EQ("oceanic", destination.name);
  ASSERT_NE(nullptr, destination.payload);
  EXPECT_EQ(815, *destination.payload);
  EXPECT_EQ(108, destination.count);
  EXPECT_EQ(nullptr, source.payload);
}
