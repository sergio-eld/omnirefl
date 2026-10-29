#include <gtest/gtest.h>

#include <omnirefl/compat.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>

#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

namespace binding_metadata {

struct record {
  int value;
  mutable int cached;

  int read(int offset) const { return value + offset; }
};

struct derived_record: record {};

union alternatives { int integer; double real; };

template <typename T>
struct record_template { T value; };

template <typename T>
union union_template { T value; };

enum class status { ready, done };

struct static_record {
  static const int shared = 37;

  static int read(int offset) { return shared + offset; }
};

const int static_record::shared;

int echo(int value) { return value; }

using echo_function = omni::compat::decay_t<decltype(OMNI_FUNCTION(echo))>;

struct command_record { echo_function run; };

struct move_only_record { std::unique_ptr<int> value; };

// Deduce metadata from the binding's base, including before C++20 concepts.
struct metadata_name {
  template <typename _M>
  std::string operator()(omni::record_meta_t<_M> m) const {
    return m.type_name();
  }

  template <typename _M>
  std::string operator()(omni::enum_meta_t<_M> m) const {
    return m.type_name();
  }

  template <typename _M>
  std::string operator()(omni::data_member_meta_t<_M> m) const {
    return m.name();
  }

  template <typename _M>
  std::string operator()(omni::function_meta_t<_M> m) const {
    return m.name();
  }
};

struct as_meta_name {
  template <typename Binding>
  std::string operator()(const Binding &b) const {
    static_assert(std::is_same<decltype(b.as_meta()),
      typename Binding::meta>::value, "as_meta must return metadata by value");

    static_assert(noexcept(b.as_meta()), "as_meta must be noexcept");
    return metadata_name{}(b.as_meta());
  }
};

struct as_meta_names {
  template <typename... Binding>
  std::vector<std::string> operator()(const Binding &...b) const {
    return {as_meta_name{}(b)...};
  }
};

struct inspect_as_meta_record {
  template <typename T>
  std::tuple<std::string, std::vector<std::string>, std::vector<std::string>>
  operator()(omni::record_binding_t<T> r) const {
    return {as_meta_name{}(r),
      omni::compat::apply(as_meta_names{}, r.accessible_data_members()),
      omni::compat::apply(as_meta_names{}, r.accessible_member_functions())};
  }
};

struct inspect_as_meta_enum {
  template <typename T>
  std::string operator()(omni::enum_binding_t<T> e) const {
    return as_meta_name{}(e);
  }
};

struct inspect_owning_meta {
  template <typename _M>
  std::pair<std::string, int> operator()(omni::record_meta_t<_M> m) const {
    auto b = m.bind(move_only_record{omni::compat::make_unique<int>(37)});
    static_assert(!std::is_copy_constructible<decltype(b)>::value,
      "the binding must own a move-only record");

    const auto metadata = std::move(b).as_meta();
    return {metadata_name{}(metadata), *b.ref().value};
  }
};

struct inspect_temporary_meta {
  template <typename _M>
  std::pair<std::string, std::size_t> operator()(
    omni::record_meta_t<_M> m) const {
    const auto metadata =
      m.bind(move_only_record{omni::compat::make_unique<int>(37)}).as_meta();

    return {metadata_name{}(metadata),
      std::tuple_size<decltype(metadata.accessible_data_members())>::value};
  }
};

struct inspect_constexpr_meta {
  template <typename _M>
  bool operator()(omni::record_meta_t<_M> m) const {
    constexpr auto metadata = m.bind(record_template<int>{7}).as_meta();
    static_assert(metadata.is_template(),
      "as_meta must support C++11 constexpr");

    return metadata.is_template();
  }
};

TEST(binding_metadata, converts_records_and_members_to_metadata_values) {
  const auto expected = std::make_tuple(std::string{"record"},
    std::vector<std::string>{"value", "cached"},
    std::vector<std::string>{"read"});
  const record value{7, 0};

  EXPECT_EQ(expected, omni::reflected_call(inspect_as_meta_record{}, value));
}

TEST(binding_metadata, converts_enums_to_metadata_values) {
  const status value = status::ready;

  EXPECT_EQ("status", omni::reflected_call(inspect_as_meta_enum{}, value));
}

TEST(binding_metadata, converting_metadata_does_not_move_the_owned_record) {
  EXPECT_EQ(std::make_pair(std::string{"move_only_record"}, 37),
    omni::reflected_call(inspect_owning_meta{},
      omni::type_t<move_only_record>{}));
}

TEST(binding_metadata, metadata_outlives_a_temporary_owning_binding) {
  EXPECT_EQ(std::make_pair(std::string{"move_only_record"}, std::size_t{1}),
    omni::reflected_call(inspect_temporary_meta{},
      omni::type_t<move_only_record>{}));
}

TEST(binding_metadata, converts_to_metadata_in_constant_expressions) {
  EXPECT_TRUE(omni::reflected_call(inspect_constexpr_meta{},
    omni::type_t<record_template<int>>{}));
}

struct member_names {
  template <typename... Member>
  std::vector<std::string> operator()(Member... m) const {
    return {metadata_name{}(m)...};
  }
};

struct inspect_record {
  template <typename T>
  std::tuple<std::string, std::vector<std::string>, std::vector<std::string>>
  operator()(omni::record_binding_t<T> r) const {
    return {metadata_name{}(r),
      omni::compat::apply(member_names{}, r.accessible_data_members()),
      omni::compat::apply(member_names{}, r.accessible_member_functions())};
  }
};

struct inspect_enum {
  template <typename T>
  std::string operator()(omni::enum_binding_t<T> e) const {
    return metadata_name{}(e);
  }
};

TEST(binding_metadata, accepts_record_and_member_bindings_as_metadata) {
  const auto expected = std::make_tuple(std::string{"record"},
    std::vector<std::string>{"value", "cached"},
    std::vector<std::string>{"read"});

  EXPECT_EQ(expected, omni::reflected_call(inspect_record{}, record{7, 0}));
}

TEST(binding_metadata, accepts_enum_bindings_as_metadata) {
  EXPECT_EQ("status",
    omni::reflected_call(inspect_enum{}, status::ready));
}

struct inspect_static_members {
  template <typename T>
  std::pair<int, int> operator()(omni::record_binding_t<T> r) const {
    const auto field = std::get</*shared=*/0>(r.accessible_data_members());
    const auto function =
      std::get</*read=*/0>(r.accessible_member_functions());

    static_assert(field.is_static(), "static field query must be inherited");
    static_assert(function.is_static(),
      "static function query must be inherited");

    return {field.value(), function(5)};
  }
};

TEST(binding_metadata, inherits_static_member_queries_and_preserves_access) {
  const static_record value{};

  EXPECT_EQ(std::make_pair(37, 42),
    omni::reflected_call(inspect_static_members{}, value));
}

struct inspect_callable_field {
  template <typename Field>
  int operator()(Field f) const {
    static_assert(f.has_callable_metadata(),
      "function tags must retain their callable metadata");

    return omni::compat::invoke(f.callable_metadata().pointer(), 7);
  }
};

struct inspect_callable_record {
  template <typename T>
  int operator()(omni::record_binding_t<T> r) const {
    return omni::compat::apply(inspect_callable_field{},
      r.accessible_data_members());
  }
};

TEST(binding_metadata, inherits_callable_metadata_queries) {
  EXPECT_EQ(7,
    omni::reflected_call(inspect_callable_record{}, command_record{}));
}

#if defined(__cpp_concepts)
template <omni::data_member_meta Field>
#else
template <typename Field>
#endif
struct mutable_member: std::integral_constant<bool, Field::is_mutable()> {};

struct inspect_mutable_members {
#if defined(__cpp_concepts)
  template <omni::record_meta T>
#else
  template <typename T>
#endif
  std::vector<std::string> operator()(T r) const {
    return omni::compat::apply(member_names{},
      r.accessible_data_members() | omni::fn::filter<mutable_member>());
  }
};

TEST(binding_metadata, shares_metadata_predicates_with_bindings) {
  const std::vector<std::string> expected{"cached"};
  const record value{7, 0};

  EXPECT_EQ(expected, omni::reflected_call(inspect_mutable_members{},
    omni::type_t<record>{}));
  EXPECT_EQ(expected,
    omni::reflected_call(inspect_mutable_members{}, value));
}

struct record_properties {
  bool is_union;
  bool is_template;
  bool has_bases;
  bool is_aggregatable;
};

struct inspect_properties {
  template <typename T>
  record_properties operator()(omni::record_binding_t<T> r) const {
    constexpr bool is_union = r.is_union();
    constexpr bool is_template = r.is_template();
    constexpr bool has_bases = r.has_bases();
    constexpr bool is_aggregatable = r.is_aggregatable();
    static_assert(noexcept(r.is_union()), "is_union must be noexcept");
    static_assert(noexcept(r.is_template()), "is_template must be noexcept");
    return {is_union, is_template, has_bases, is_aggregatable};
  }
};

struct property_case {
  std::string name;
  record_properties actual;
  record_properties expected;
};

class binding_properties: public testing::TestWithParam<property_case> {};

TEST_P(binding_properties, exposes_record_metadata_queries) {
  const auto &c = GetParam();
  EXPECT_EQ(c.expected.is_union, c.actual.is_union);
  EXPECT_EQ(c.expected.is_template, c.actual.is_template);
  EXPECT_EQ(c.expected.has_bases, c.actual.has_bases);
  EXPECT_EQ(c.expected.is_aggregatable, c.actual.is_aggregatable);
}

const property_case properties[]{
  {"Record", omni::reflected_call(inspect_properties{}, record{7, 0}),
    {false, false, false, true}},
  {"DerivedRecord",
    omni::reflected_call(inspect_properties{}, derived_record{}),
    {false, false, true, false}},
  {"Union", omni::reflected_call(inspect_properties{}, alternatives{7}),
    {true, false, false, false}},
  {"RecordTemplate", omni::reflected_call(inspect_properties{},
    record_template<int>{7}), {false, true, false, true}},
  {"UnionTemplate", omni::reflected_call(inspect_properties{},
    union_template<int>{7}), {true, true, false, false}},
};

INSTANTIATE_TEST_SUITE_P(declarations,
  binding_properties,
  testing::ValuesIn(properties),
  /*case name=*/[](const testing::TestParamInfo<property_case> &c) {
    return c.param.name;
  });

#if defined(__cpp_concepts)
struct select_metadata {
  template <omni::meta T>
  constexpr int operator()(T) const { return 1; }

  template <omni::binding T>
  constexpr int operator()(T) const { return 2; }
};

struct select_record {
  template <omni::record_meta T>
  constexpr int operator()(T) const { return 1; }

  template <omni::record_binding T>
  constexpr int operator()(T) const { return 2; }
};

struct select_enum {
  template <omni::enum_meta T>
  constexpr int operator()(T) const { return 1; }

  template <omni::enum_binding T>
  constexpr int operator()(T) const { return 2; }
};

struct select_member {
  template <omni::data_member_meta T>
  constexpr int operator()(T) const { return 1; }

  template <omni::data_member_binding T>
  constexpr int operator()(T) const { return 2; }

  template <omni::function_meta T>
  constexpr int operator()(T) const { return 1; }

  template <omni::function_binding T>
  constexpr int operator()(T) const { return 2; }
};

struct select_members {
  template <typename... Member>
  std::vector<std::pair<int, int>> operator()(Member... m) const {
    return {{select_member{}(static_cast<typename Member::meta>(m)),
      select_member{}(m)}...};
  }
};

struct inspect_record_overloads {
  template <omni::record_binding T>
  std::tuple<int, int, int, int,
    std::vector<std::pair<int, int>>, std::vector<std::pair<int, int>>>
  operator()(T r) const {
    static_assert(omni::record_meta<T>, "bindings must satisfy record_meta");
    const auto m = static_cast<typename T::meta>(r);
    return {select_metadata{}(m), select_metadata{}(r),
      select_record{}(m), select_record{}(r),
      omni::compat::apply(select_members{}, r.accessible_data_members()),
      omni::compat::apply(select_members{}, r.accessible_member_functions())};
  }
};

struct inspect_enum_overloads {
  template <omni::enum_binding T>
  std::tuple<int, int, int, int> operator()(T e) const {
    static_assert(omni::enum_meta<T>, "bindings must satisfy enum_meta");
    const auto m = static_cast<typename T::meta>(e);
    return {select_metadata{}(m), select_metadata{}(e),
      select_enum{}(m), select_enum{}(e)};
  }
};

TEST(binding_metadata, prefers_binding_overloads_for_records_and_members) {
  const auto expected = std::make_tuple(1, 2, 1, 2,
    std::vector<std::pair<int, int>>{{1, 2}, {1, 2}},
    std::vector<std::pair<int, int>>{{1, 2}});

  EXPECT_EQ(expected,
    omni::reflected_call(inspect_record_overloads{}, record{7, 0}));
}

TEST(binding_metadata, prefers_binding_overloads_for_enums) {
  EXPECT_EQ(std::make_tuple(1, 2, 1, 2),
    omni::reflected_call(inspect_enum_overloads{}, status::ready));
}
#endif

} // namespace binding_metadata

} // namespace
