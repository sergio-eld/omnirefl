// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Sergei Kolesnik

#include <gtest/gtest.h>
#include <omnirefl/reflected_scope.hpp>

#include <vector>

namespace exclusion_protocol {

struct key {
  int id;
};

struct mapped {
  int count;
};

struct element {
  int value;
};

struct standard_element {
  int value;
};

struct hidden {
  int value;
};

struct virtual_element {
  int value;
};

template <typename T>
struct alias_base {
  using value_type = T;

  hidden storage;
};

template <typename T>
struct virtual_wrapper: virtual alias_base<T> {};

struct inherited_element {
  int value;
};

template <typename T>
struct inherited_wrapper: alias_base<T> {};

struct private_wrapper: private alias_base<hidden> {};

struct exposed_element {
  int value;
};

struct exposed_wrapper: private alias_base<exposed_element> {
  using alias_base<exposed_element>::value_type;
};

struct ambiguous_first {
  int value;
};

struct ambiguous_second {
  int value;
};

struct ambiguous_wrapper:
  alias_base<ambiguous_first>,
  alias_base<ambiguous_second> {};

struct lookup_root {
  exposed_wrapper exposed;
  ambiguous_wrapper ambiguous;
};

struct virtual_root {
  virtual_wrapper<virtual_element> value;
};

enum class status { ready };

struct dictionary: alias_base<hidden> {
  using value_type = int;
  using key_type = key;
  using mapped_type = mapped;
  using error_type = status;

  hidden storage;
};

template <typename T>
struct wrapper {
  using value_type = T;

  hidden storage;
};

struct root {
  dictionary concrete;
  wrapper<wrapper<element>> nested;
  wrapper<std::vector<standard_element>> standard;
  wrapper<inherited_wrapper<inherited_element>> inherited;
  private_wrapper blocked;
};

// C++11 callables keep metadata queries inside their reflected scopes.
struct inspect_protocols {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> meta) const {
    // Test-only ad hoc: spell concrete fixture types to check exact metadata.
    // Guard non-dependent queries: generated traits are absent in tool runs.
#if !defined(OMNI_TOOL_RUN)
    EXPECT_TRUE(omni::is_reflected<key>::value);
    EXPECT_TRUE(omni::is_reflected<mapped>::value);
    EXPECT_TRUE(omni::is_reflected<status>::value);
    EXPECT_TRUE(omni::is_reflected<element>::value);
    EXPECT_TRUE(omni::is_reflected<standard_element>::value);
    EXPECT_TRUE(omni::is_reflected<inherited_element>::value);
    EXPECT_FALSE(omni::is_reflected<dictionary>::value);
    EXPECT_FALSE(omni::is_reflected<wrapper<element>>::value);
    EXPECT_FALSE(omni::is_reflected<hidden>::value);
    EXPECT_FALSE(omni::is_reflected<private_wrapper>::value);
    EXPECT_FALSE(omni::is_reflected<std::vector<standard_element>>::value);
#endif

    return std::tuple_size<decltype(meta.public_fields())>::value;
  }
};

struct inspect_virtual_protocols {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> meta) const {
    // Test-only ad hoc: spell concrete fixture types to check exact metadata.
    // Guard non-dependent queries: generated traits are absent in tool runs.
#if !defined(OMNI_TOOL_RUN)
    EXPECT_TRUE(omni::is_reflected<virtual_element>::value);
    EXPECT_FALSE(omni::is_reflected<virtual_wrapper<virtual_element>>::value);
    EXPECT_FALSE(omni::is_reflected<hidden>::value);
    EXPECT_FALSE(omni::is_reflected<alias_base<virtual_element>>::value);
#endif

    return std::tuple_size<decltype(meta.public_fields())>::value;
  }
};

struct inspect_alias_lookup {
  template <typename _M>
  std::size_t operator()(omni::record_meta_t<_M> meta) const {
    // Test-only ad hoc: spell concrete fixture types to check exact metadata.
    // Guard non-dependent queries: generated traits are absent in tool runs.
#if !defined(OMNI_TOOL_RUN)
    EXPECT_TRUE(omni::is_reflected<exposed_element>::value);
    EXPECT_FALSE(omni::is_reflected<ambiguous_first>::value);
    EXPECT_FALSE(omni::is_reflected<ambiguous_second>::value);
    EXPECT_FALSE(omni::is_reflected<exposed_wrapper>::value);
    EXPECT_FALSE(omni::is_reflected<ambiguous_wrapper>::value);
#endif

    return std::tuple_size<decltype(meta.public_fields())>::value;
  }
};

} // namespace exclusion_protocol

TEST(dependency_exclusions, follows_protocols_without_reflecting_wrappers) {
  EXPECT_EQ(5U, omni::reflected_call(exclusion_protocol::inspect_protocols{},
    omni::type_t<exclusion_protocol::root>{}));
}

TEST(dependency_exclusions, metadata_restrictions_do_not_disable_protocols) {
  EXPECT_EQ(1U,
    omni::reflected_call(exclusion_protocol::inspect_virtual_protocols{},
      omni::type_t<exclusion_protocol::virtual_root>{}));
}

TEST(dependency_exclusions, resolves_public_using_and_ambiguous_aliases) {
  EXPECT_EQ(2U, omni::reflected_call(exclusion_protocol::inspect_alias_lookup{},
    omni::type_t<exclusion_protocol::lookup_root>{}));
}

#if defined(OMNI_TOOL_RUN)
OMNI_SKIP_METADATA(exclusion_protocol::dictionary);
OMNI_SKIP_METADATA(exclusion_protocol::wrapper<int>);
OMNI_SKIP_METADATA(exclusion_protocol::virtual_wrapper<int>);
OMNI_SKIP_METADATA(exclusion_protocol::inherited_wrapper<int>);
OMNI_SKIP_METADATA(exclusion_protocol::private_wrapper);
OMNI_SKIP_METADATA(exclusion_protocol::exposed_wrapper);
OMNI_SKIP_METADATA(exclusion_protocol::ambiguous_wrapper);
#endif
