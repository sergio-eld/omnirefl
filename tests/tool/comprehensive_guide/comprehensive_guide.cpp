// The guide is written for compilers with the C++20 features used by
// omnirefl's public concepts. It deliberately avoids <concepts>; the package
// matrix still covers toolchains where C++20 is partial. Similar small
// substitutions are preferred here, such as std::find_if instead of
// std::ranges::find_if.

#include <gtest/gtest.h>

#include <omnirefl/reflection.hpp>

#include <mpark/variant.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace meta {

template <typename T>
concept range_like = requires(T value) {
  typename T::value_type;
  value.begin();
  value.end();
};

template <typename T>
concept map_like = range_like<T> && requires { typename T::mapped_type; };

} // namespace meta

} // namespace

/**
 * Invocation
 *
 * Conceptual signature:
 *   template <typename Visit, typename... T>
 *   auto reflected_call(Visit visit, T... args)
 *     -> result of `visit(reflected argument for each T...)`;
 *
 * `auto` is the visitor result. `reflected_call` only maps every input `T` to
 * the corresponding reflected argument before invoking `visit`.
 *
 * `Visit` must be a template callable, for example a generic lambda or a
 * struct with templated `operator()`.
 *
 * The caller is responsible for passing sanitized reflected inputs: a directly
 * reflectable record/enum or `omni::type_t<T>`. Compound arguments such as
 * tuple, vector, variant, or raw arrays are unsupported. Arbitrary compound
 * class templates cannot be distinguished reliably from ordinary record
 * templates, so enforcement is best effort. For variants, use `std::visit` or
 * `mpark::visit` to pass the active alternative, as shown in the advanced
 * examples. Compound types can still expose dependencies through reflected
 * fields or supported protocol typedefs.
 *
 * Reflected argument examples:
 *   foo f = ...;
 *   reflected_call(visit, omni::type<foo>);  // record_meta_t<_M>
 *   reflected_call(visit, foo{});            // record_binding_t<foo &&>
 *   reflected_call(visit, f);                // record_binding_t<foo &>
 *   reflected_call(visit, std::as_const(f)); // record_binding_t<const foo &>
 *   reflected_call(visit, std::move(f));     // record_binding_t<foo &&>
 *
 *   const bar b = ...;
 *   reflected_call(visit, f, b);             // record_binding_t<foo &>,
 *                                            // record_binding_t<const bar &>
 *
 * NOTE: `constexpr auto result = reflected_call(...)` is not supported as of
 * this writing because it triggers visitor body instantiation. The `visit`
 * object itself can still be constexpr, including its internal calculations.
 *
 * Reflected scope
 *
 * Only generic lambdas or callable types with templated `operator()` are
 * allowed, so the visitor body is not instantiated during the tool run.
 *
 *   []<typename... Meta>(Meta... reflected_args) -> result_type {
 *     // reflected scope: reflection queries are available here
 *   }
 *
 * The lambda form must use an explicit trailing return type, including
 * `-> void`. A deduced return type can instantiate the lambda body during tool
 * run, before generated metadata exists.
 *
 * Public metadata wraps an opaque generated `_M`; use
 * `meta_t<_M>::reflected_type`, not `_M` directly.
 *
 * Field tuple mapping:
 *   omni::record_meta_t<_M>::public_fields() -> omni::field_meta_t<FieldMeta>
 *   omni::record_binding_t<T>::public_fields()
 *     -> omni::field_binding_t<Owner, FieldMeta>
 *
 * `Owner` carries the cv qualification of the bound record object. Field
 * bindings are always non-owning because a field cannot outlive its record.
 *
 * The first test below shows the stable reflected-scope interface. The rest of
 * this file covers enums, bindings, dependency routes, documentation,
 * bitfields, primary templates, and schema-style traversal.
 */

/// API exposition -------------------------------------------------------------

namespace exposition {

/// exposition record annotation
struct record {
  /// exposition enum annotation
  enum class state {
    ready,
    paused,
  };

  /// foo field annotation
  int foo;

  /// enabled field annotation
  bool enabled;

  /// version field annotation
  const int version;

  /// cache field annotation
  mutable int cache;

  /// observed field annotation
  volatile int observed;

  /// scores field annotation
  std::vector<std::string> scores;
};

struct field_summary {
  std::string_view name;
  std::string_view type_name;
  std::string_view qualified_type_name;
  std::string_view annotation;
  bool is_const;
  bool is_mutable;
  bool is_volatile;

  bool operator==(const field_summary &rhs) const {
    return name == rhs.name //
      && type_name == rhs.type_name
      && qualified_type_name == rhs.qualified_type_name
      && annotation == rhs.annotation && is_const == rhs.is_const
      && is_mutable == rhs.is_mutable && is_volatile == rhs.is_volatile;
  }
};

struct reflected_summary {
  std::string_view entity;
  std::string_view type_name;
  std::string_view qualified_type_name;
  std::string_view annotation;
  std::vector<field_summary> fields;
  std::vector<std::string_view> enumerators;

  bool operator==(const reflected_summary &rhs) const {
    return entity == rhs.entity //
      && type_name == rhs.type_name
      && qualified_type_name == rhs.qualified_type_name
      && annotation == rhs.annotation && fields == rhs.fields
      && enumerators == rhs.enumerators;
  }
};

struct foobar_record {
  int foo_count;
  unsigned bar_count : 5;
  int untouched_count;
};

} // namespace exposition

TEST(exposition, reflected_scope_overview) {
  using namespace std::string_view_literals;

  exposition::reflected_summary ctx;

  // Prefer captures for extra arguments that should not be reflected. This
  // exposition is artificial; a summary-only visitor should usually return by
  // value.
  const auto render = [&ctx]<typename Meta>(Meta m) -> void {
    ctx.entity =
      omni::reflected_entity::record == m.entity() ? "record"sv : "enum"sv;
    ctx.type_name = m.type_name();
    ctx.qualified_type_name = m.qualified_type_name();
    ctx.annotation = m.documentation();

    if constexpr (omni::reflected_entity::record == m.entity()) {
      // `omni::compat::apply` provides the same shape for earlier standards.
      std::apply(
        [&ctx]<omni::field_meta... FieldMeta>(FieldMeta... field) -> void {
          (ctx.fields.push_back({
             .name = field.name(),
             // Field declaration spelling with the outer namespace omitted.
             // Reflectable field types can be queried separately through
             // `omni::reflected(omni::type<FieldMeta::type>)`.
             .type_name = field.type_name(),
             // Source spelling with namespaces restored when Clang can
             // identify the declaration.
             .qualified_type_name = field.qualified_type_name(),
             .annotation = field.documentation(),
             .is_const = field.is_const(),
             .is_mutable = field.is_mutable(),
             .is_volatile = field.is_volatile(),
           }),
            ...);
        },
        m.public_fields());
    } else {
      static_assert(omni::reflected_entity::enumeration == m.entity());
      for (const auto &value_name : m.enumerators())
        ctx.enumerators.push_back(value_name.second);
    }
  };

  const exposition::reflected_summary k_expected_record{
    .entity = "record"sv,
    .type_name = "record"sv,
    .qualified_type_name = "exposition::record"sv,
    .annotation = "exposition record annotation"sv,
    .fields =
      {
        {
          .name = "foo"sv,
          .type_name = "int"sv,
          .qualified_type_name = "int"sv,
          .annotation = "foo field annotation"sv,
          .is_const = false,
          .is_mutable = false,
          .is_volatile = false,
        },
        {
          .name = "enabled"sv,
          .type_name = "bool"sv,
          .qualified_type_name = "bool"sv,
          .annotation = "enabled field annotation"sv,
          .is_const = false,
          .is_mutable = false,
          .is_volatile = false,
        },
        {
          .name = "version"sv,
          .type_name = "int"sv,
          .qualified_type_name = "int"sv,
          .annotation = "version field annotation"sv,
          .is_const = true,
          .is_mutable = false,
          .is_volatile = false,
        },
        {
          .name = "cache"sv,
          .type_name = "int"sv,
          .qualified_type_name = "int"sv,
          .annotation = "cache field annotation"sv,
          .is_const = false,
          .is_mutable = true,
          .is_volatile = false,
        },
        {
          .name = "observed"sv,
          .type_name = "int"sv,
          .qualified_type_name = "int"sv,
          .annotation = "observed field annotation"sv,
          .is_const = false,
          .is_mutable = false,
          .is_volatile = true,
        },
        {
          .name = "scores"sv,
          // Template arguments keep their source spelling; only the outer
          // namespace qualifier is omitted from `type_name()`.
          .type_name = "vector<std::string>"sv,
          .qualified_type_name = "std::vector<std::string>"sv,
          .annotation = "scores field annotation"sv,
          .is_const = false,
          .is_mutable = false,
          .is_volatile = false,
        },
      },
    .enumerators = {},
  };

  omni::reflected_call(render, omni::type<exposition::record>);
  EXPECT_EQ(k_expected_record, ctx);

  ctx = {};
  const exposition::reflected_summary k_expected_state{
    .entity = "enum"sv,
    .type_name = "record::state"sv,
    .qualified_type_name = "exposition::record::state"sv,
    .annotation = "exposition enum annotation"sv,
    .fields = {},
    .enumerators = std::vector<std::string_view>{"ready"sv, "paused"sv},
  };

  omni::reflected_call(render, omni::type<exposition::record::state>);
  EXPECT_EQ(k_expected_state, ctx);
}

// Field bindings allow mutation of public data members, including bitfields.
TEST(exposition, write_foobar) {
  const auto write_foobar = //
    [](omni::record_binding auto b) -> exposition::foobar_record {
    omni::compat::apply(
      [](omni::field_binding auto... field) {
        const auto write = [](omni::field_binding auto field) {
          constexpr std::string_view field_name = field.name();

          if constexpr (field_name.find("foo") != std::string_view::npos)
            field.set_value(8);

          if constexpr (field_name.find("bar") != std::string_view::npos)
            field.set_value(15);
        };

        (write(field), ...);
      },
      b.public_fields());

    return std::move(b.record);
  };

  const auto record = omni::reflected_call(write_foobar,
    exposition::foobar_record{
      .foo_count = 1,
      .bar_count = 2,
      .untouched_count = 3,
    });

  EXPECT_EQ(8, record.foo_count);
  EXPECT_EQ(15u, record.bar_count);
  EXPECT_EQ(3, record.untouched_count);
}

/// Examples -------------------------------------------------------------------

namespace example {

enum class status {
  draft,
  active,
};

} // namespace example

TEST(example, enum_names) {
  using namespace std::string_view_literals;

  EXPECT_EQ("draft"sv,
    omni::reflected_call(
      [](const omni::enum_binding auto status) -> std::string_view {
        const auto enumerators = status.enumerators();
        const auto it = std::find_if(enumerators.begin(),
          enumerators.end(),
          [&status](const auto &value_name) {
            return value_name.first == status.enum_value;
          });

        return enumerators.end() == it ? "unknown"sv : it->second;
      },
      example::status::draft));

  const std::vector<std::pair<example::status, std::string_view>>
    k_expected_pairs{
      {example::status::draft, "draft"sv},
      {example::status::active, "active"sv},
    };

  EXPECT_EQ(k_expected_pairs,
    omni::reflected_call(
      [](omni::enum_meta auto status)
        -> std::vector<std::pair<example::status, std::string_view>> {
        const auto enumerators = status.enumerators();
        return {enumerators.begin(), enumerators.end()};
      },
      omni::type<example::status>));
}

TEST(example, binding_storage_forms) {
  exposition::foobar_record mutable_record{
    .foo_count = 1,
    .bar_count = 2,
    .untouched_count = 3,
  };

  const exposition::foobar_record const_record{
    .foo_count = 4,
    .bar_count = 5,
    .untouched_count = 6,
  };

  const auto inspect = //
    [](omni::record_binding auto mutable_binding,
      omni::record_binding auto const_binding,
      omni::record_binding auto rvalue_binding) -> int {
    // `reflected_call` arguments are non-owning bindings. An rvalue binding
    // preserves the consuming value category without moving the whole value.
    EXPECT_EQ(false, decltype(mutable_binding)::owning::value);
    EXPECT_EQ(false, decltype(const_binding)::owning::value);
    EXPECT_EQ(false, decltype(rvalue_binding)::owning::value);

    mutable_binding.record.foo_count = 8;

    return mutable_binding.record.foo_count //
      + const_binding.record.bar_count //
      + rvalue_binding.record.untouched_count;
  };

  EXPECT_EQ(22,
    omni::reflected_call(inspect,
      mutable_record,
      const_record,
      exposition::foobar_record{
        .foo_count = 7,
        .bar_count = 8,
        .untouched_count = 9,
      }));
  EXPECT_EQ(8, mutable_record.foo_count);
}

TEST(example, reflected_value_query) {
  using namespace std::string_view_literals;

  const auto query_value = [](omni::record_binding auto b) -> std::string_view {
    // `omni::reflected(value)` is a reflected-scope convenience query. It keeps
    // the value qualification instead of forcing the user to spell `decltype`.
    const omni::record_binding auto same_value = omni::reflected(b.record);
    return same_value.type_name();
  };

  EXPECT_EQ("foobar_record"sv,
    omni::reflected_call(query_value, exposition::foobar_record{}));
}

TEST(example, meta_bind_and_field_metadata) {
  const auto inspect = [](omni::record_meta auto record) -> int {
    // `meta.bind()` default-constructs an owning binding when the reflected
    // type is default constructible. `meta.bind(value)` binds an existing
    // object.
    auto owned = record.bind();
    auto non_owning = record.bind(owned.record);

    return std::apply(
      [&non_owning]<omni::field_meta... Field>(Field... field) {
        // Field metadata can be used without first creating field bindings.
        // Indexes are local to the declaring record and follow declaration
        // order for this simple non-inherited record.
        ((field.set_value(non_owning.record,
           static_cast<typename Field::type>(field.index() + 1))),
          ...);

        return (field.value(non_owning.record) + ...);
      },
      record.public_fields());
  };

  EXPECT_EQ(6,
    omni::reflected_call(inspect, omni::type<exposition::foobar_record>));
}

namespace primary_template {

template <typename Char>
struct text_record {
  /// display title
  std::basic_string<Char> title;

  /// number of revisions
  int revisions;
};

} // namespace primary_template

TEST(example, primary_template_read_data) {
  using namespace std::string_literals;
  using namespace std::string_view_literals;

  struct rendered_field {
    std::string_view name;
    std::string_view annotation;
    std::string value;

    bool operator==(const rendered_field &rhs) const {
      return name == rhs.name //
        && annotation == rhs.annotation && value == rhs.value;
    }
  };

  const primary_template::text_record<char> record{
    .title = "Guide"s,
    .revisions = 2,
  };

  const auto read_fields =
    [](omni::record_binding auto b) -> std::vector<rendered_field> {
    std::vector<rendered_field> out;

    omni::compat::apply(
      [&out](omni::field_binding auto... field) {
        const auto render = [&out](omni::field_binding auto field) {
          constexpr std::string_view field_name = field.name();

          if constexpr ("title"sv == field_name) {
            out.push_back({
              .name = field.name(),
              .annotation = field.documentation(),
              .value = field.value(),
            });
          }

          if constexpr ("revisions"sv == field_name) {
            out.push_back({
              .name = field.name(),
              .annotation = field.documentation(),
              .value = std::to_string(field.value()),
            });
          }
        };

        (render(field), ...);
      },
      b.public_fields());

    return out;
  };

  const std::vector<rendered_field> k_expected{
    {
      .name = "title"sv,
      .annotation = "display title"sv,
      .value = "Guide"s,
    },
    {
      .name = "revisions"sv,
      .annotation = "number of revisions"sv,
      .value = "2"s,
    },
  };

  EXPECT_EQ(k_expected, omni::reflected_call(read_fields, record));
}

namespace primary_template {

// Dummy arena shape for the primary-template example. It deliberately delegates
// to std::allocator; the example is about reflecting a record template that
// uses an allocator-shaped template argument, not about allocation strategy.
struct arena_allocator {
  template <typename T>
  T *allocate(std::size_t n) {
    return std::allocator<T>{}.allocate(n);
  }

  template <typename T>
  void deallocate(T *p, std::size_t n) noexcept {
    std::allocator<T>{}.deallocate(p, n);
  }

  template <typename T>
  struct allocator {
    using value_type = T;

    template <typename U>
    struct rebind {
      using other = allocator<U>;
    };

    allocator() noexcept = default;

    template <typename U>
    allocator(const allocator<U> &) noexcept {}

    T *allocate(std::size_t n) {
      return arena_allocator{}.allocate<T>(n);
    }

    void deallocate(T *p, std::size_t n) noexcept {
      arena_allocator{}.deallocate(p, n);
    }
  };
};

template <typename T, typename U>
bool operator==(const arena_allocator::allocator<T> &,
  const arena_allocator::allocator<U> &) noexcept {
  return true;
}

template <typename T, typename U>
bool operator!=(const arena_allocator::allocator<T> &,
  const arena_allocator::allocator<U> &) noexcept {
  return false;
}

template <typename Alloc>
struct allocated_record {
  std::basic_string<char,
    std::char_traits<char>,
    typename Alloc::template rebind<char>::other>
    name;
  std::vector<int, Alloc> scores;
  int count;
};

using result = allocated_record<arena_allocator::allocator<int>>;
using runtime_value = std::variant<int, std::string, std::vector<int>>;
using runtime_table = std::map<std::string, runtime_value, std::less<>>;

} // namespace primary_template

TEST(example, primary_template_from_map) {
  using namespace std::string_literals;

  const primary_template::runtime_table table{
    {"name"s, "Ada"s},
    {"scores"s, std::vector{1, 2, 3}},
    {"count"s, 8},
  };

  const auto from_map = [&table]<omni::meta Result>(
                          Result result_type) -> primary_template::result {
    const auto init = [&table]<typename T>(omni::type_t<T>,
                        std::string_view name) -> T {
      const auto it = table.find(name);
      if (table.end() == it)
        return {};

      // Minimal example policy: missing keys and incompatible runtime values
      // become default field values. Real deserialization would usually report
      // those as errors.
      return std::visit(
        []<typename From>(const From &from) -> T {
          if constexpr (std::is_same_v<From, T>) {
            return from;
          } else if constexpr (omni::traits::is<std::basic_string, From>()
            && omni::traits::is<std::basic_string, T>()) {
            // Convert between different `std::basic_string` specializations,
            // for example when the target record uses a custom allocator.
            return {from.begin(), from.end()};
          } else if constexpr (omni::traits::is<std::vector, From>()
            && omni::traits::is<std::vector, T>()) {
            return {from.begin(), from.end()};
          } else {
            return {};
          }
        },
        it->second);
    };

    return std::apply(
      [&init]<omni::field_meta... Field>(
        Field... field) -> primary_template::result {
        return {
          init(omni::type<typename Field::type>, field.name())...,
        };
      },
      result_type.public_fields());
  };

  const primary_template::result record =
    omni::reflected_call(from_map, omni::type<primary_template::result>);
  const std::string name{record.name.begin(), record.name.end()};
  const std::vector<int> scores{record.scores.begin(), record.scores.end()};

  EXPECT_EQ("Ada", name);
  EXPECT_EQ((std::vector{1, 2, 3}), scores);
  EXPECT_EQ(8, record.count);
}

namespace dependency {

/// protocol type alias dependency
struct protocol_type {
  int value;
};

/// protocol value_type alias dependency
struct protocol_value_type {
  int value;
};

/// protocol first_type alias dependency
struct protocol_first_type {
  int value;
};

/// protocol second_type alias dependency
struct protocol_second_type {
  int value;
};

/// protocol key_type alias dependency
struct protocol_key_type {
  int value;
};

/// protocol error_type alias dependency
struct protocol_error_type {
  int value;
};

/// protocol value alias dependency
struct protocol_value {
  int value;
};

/// protocol mapped_type alias dependency
struct protocol_mapped_type {
  int value;
};

/// public base field dependency
struct from_base {
  int value;
};

/// direct public field dependency
struct from_field {
  int value;
};

/// tuple element dependency
struct from_tuple {
  int value;
};

/// variant alternative dependency
struct from_variant {
  int value;
};

/// vector value_type dependency
struct from_vector {
  int value;
};

template <typename T>
struct box {
  T value;
};

struct public_base {
  /// field from public base
  from_base base;
};

/// supported dependency routes
struct supported_routes: public_base {
  using type = protocol_type;
  using value_type = protocol_value_type;
  using first_type = protocol_first_type;
  using second_type = protocol_second_type;
  using key_type = protocol_key_type;
  using error_type = protocol_error_type;
  using value = protocol_value;
  using mapped_type = protocol_mapped_type;

  /// direct field route
  from_field field;

  /// tuple route
  std::tuple<from_tuple, box<from_tuple>> tuple_route;

  /// variant route
  std::variant<from_variant, box<from_variant>> variant_route;

  /// vector value_type route
  std::vector<from_vector> vector_route;
};

} // namespace dependency

TEST(example, annotated_dependencies) {
  using namespace std::string_view_literals;

  struct rendered_type {
    std::string_view type_name;
    std::string_view qualified_type_name;
    std::string_view annotation;

    bool operator==(const rendered_type &rhs) const {
      return type_name == rhs.type_name //
        && qualified_type_name == rhs.qualified_type_name
        && annotation == rhs.annotation;
    }
  };

  struct rendered_protocol_type {
    std::string_view protocol;
    rendered_type type;

    bool operator==(const rendered_protocol_type &rhs) const {
      return protocol == rhs.protocol //
        && type == rhs.type;
    }
  };

  struct rendered_field {
    std::string_view name;
    std::string_view type_name;
    std::string_view annotation;

    bool operator==(const rendered_field &rhs) const {
      return name == rhs.name //
        && type_name == rhs.type_name && annotation == rhs.annotation;
    }
  };

  struct rendered_context {
    rendered_type type;
    std::vector<rendered_protocol_type> protocol_types{};
    std::vector<rendered_field> public_fields{};
    std::vector<rendered_type> from_tuple{};
    std::vector<rendered_type> from_variant{};
    std::vector<rendered_type> from_vector{};
  };

  const auto rendered = omni::reflected_call(
    []<omni::record_meta RecordMeta>(RecordMeta record) -> rendered_context {
      const auto render_type = [](omni::meta auto type) -> rendered_type {
        return {
          .type_name = type.type_name(),
          .qualified_type_name = type.qualified_type_name(),
          .annotation = type.documentation(),
        };
      };

      // Supported nested type aliases are resolved as dependencies of the
      // reflected record and are therefore available in this reflected scope.
      using reflected_record = typename RecordMeta::reflected_type;

      rendered_context out{
        .type = render_type(record),
      };

      if constexpr (requires { typename reflected_record::type; }) {
        out.protocol_types.push_back({
          .protocol = "type",
          .type = render_type(
            omni::reflected(omni::type<typename reflected_record::type>)),
        });
      }

      if constexpr (requires { typename reflected_record::value_type; }) {
        out.protocol_types.push_back({
          .protocol = "value_type",
          .type = render_type(
            omni::reflected(omni::type<typename reflected_record::value_type>)),
        });
      }

      if constexpr (requires { typename reflected_record::first_type; }) {
        out.protocol_types.push_back({
          .protocol = "first_type",
          .type = render_type(
            omni::reflected(omni::type<typename reflected_record::first_type>)),
        });
      }

      if constexpr (requires { typename reflected_record::second_type; }) {
        out.protocol_types.push_back({
          .protocol = "second_type",
          .type = render_type(omni::reflected(
            omni::type<typename reflected_record::second_type>)),
        });
      }

      if constexpr (requires { typename reflected_record::key_type; }) {
        out.protocol_types.push_back({
          .protocol = "key_type",
          .type = render_type(
            omni::reflected(omni::type<typename reflected_record::key_type>)),
        });
      }

      if constexpr (requires { typename reflected_record::error_type; }) {
        out.protocol_types.push_back({
          .protocol = "error_type",
          .type = render_type(
            omni::reflected(omni::type<typename reflected_record::error_type>)),
        });
      }

      if constexpr (requires { typename reflected_record::value; }) {
        out.protocol_types.push_back({
          .protocol = "value",
          .type = render_type(
            omni::reflected(omni::type<typename reflected_record::value>)),
        });
      }

      if constexpr (requires { typename reflected_record::mapped_type; }) {
        out.protocol_types.push_back({
          .protocol = "mapped_type",
          .type = render_type(omni::reflected(
            omni::type<typename reflected_record::mapped_type>)),
        });
      }

      omni::compat::apply(
        [&out, render_type](omni::field_meta auto... field) {
          const auto render = //
            [&out, render_type]<omni::field_meta Field>(Field field) {
              out.public_fields.push_back({
                .name = field.name(),
                .type_name = field.type_name(),
                .annotation = field.documentation(),
              });

              if constexpr (omni::traits::is<std::tuple,
                              typename Field::type>()) {
                std::invoke(
                  [&out, render_type]<typename... T>(
                    omni::type_t<std::tuple<T...>>) {
                    (out.from_tuple.push_back(
                       render_type(omni::reflected(omni::type<T>))),
                      ...);
                  },
                  omni::type<typename Field::type>);
              }

              if constexpr (omni::traits::is<std::variant,
                              typename Field::type>()) {
                std::invoke(
                  [&out, render_type]<typename... T>(
                    omni::type_t<std::variant<T...>>) {
                    (out.from_variant.push_back(
                       render_type(omni::reflected(omni::type<T>))),
                      ...);
                  },
                  omni::type<typename Field::type>);
              }

              if constexpr (omni::traits::is<std::vector,
                              typename Field::type>()) {
                out.from_vector.push_back(render_type(omni::reflected(
                  omni::type<typename Field::type::value_type>)));
              }
            };

          (render(field), ...);
        },
        record.public_fields());

      return out;
    },
    omni::type<dependency::supported_routes>);

  const rendered_type k_expected_type{
    .type_name = "supported_routes"sv,
    .qualified_type_name = "dependency::supported_routes"sv,
    .annotation = "supported dependency routes"sv,
  };

  const std::vector<rendered_protocol_type> k_expected_protocol_types{
    {
      .protocol = "type"sv,
      .type =
        {
          .type_name = "protocol_type"sv,
          .qualified_type_name = "dependency::protocol_type"sv,
          .annotation = "protocol type alias dependency"sv,
        },
    },
    {
      .protocol = "value_type"sv,
      .type =
        {
          .type_name = "protocol_value_type"sv,
          .qualified_type_name = "dependency::protocol_value_type"sv,
          .annotation = "protocol value_type alias dependency"sv,
        },
    },
    {
      .protocol = "first_type"sv,
      .type =
        {
          .type_name = "protocol_first_type"sv,
          .qualified_type_name = "dependency::protocol_first_type"sv,
          .annotation = "protocol first_type alias dependency"sv,
        },
    },
    {
      .protocol = "second_type"sv,
      .type =
        {
          .type_name = "protocol_second_type"sv,
          .qualified_type_name = "dependency::protocol_second_type"sv,
          .annotation = "protocol second_type alias dependency"sv,
        },
    },
    {
      .protocol = "key_type"sv,
      .type =
        {
          .type_name = "protocol_key_type"sv,
          .qualified_type_name = "dependency::protocol_key_type"sv,
          .annotation = "protocol key_type alias dependency"sv,
        },
    },
    {
      .protocol = "error_type"sv,
      .type =
        {
          .type_name = "protocol_error_type"sv,
          .qualified_type_name = "dependency::protocol_error_type"sv,
          .annotation = "protocol error_type alias dependency"sv,
        },
    },
    {
      .protocol = "value"sv,
      .type =
        {
          .type_name = "protocol_value"sv,
          .qualified_type_name = "dependency::protocol_value"sv,
          .annotation = "protocol value alias dependency"sv,
        },
    },
    {
      .protocol = "mapped_type"sv,
      .type =
        {
          .type_name = "protocol_mapped_type"sv,
          .qualified_type_name = "dependency::protocol_mapped_type"sv,
          .annotation = "protocol mapped_type alias dependency"sv,
        },
    },
  };

  const std::vector<rendered_field> k_expected_public_fields{
    {
      .name = "base"sv,
      .type_name = "from_base"sv,
      .annotation = "field from public base"sv,
    },
    {
      .name = "field"sv,
      .type_name = "from_field"sv,
      .annotation = "direct field route"sv,
    },
    {
      .name = "tuple_route"sv,
      .type_name = "tuple<from_tuple, box<from_tuple>>"sv,
      .annotation = "tuple route"sv,
    },
    {
      .name = "variant_route"sv,
      .type_name = "variant<from_variant, box<from_variant>>"sv,
      .annotation = "variant route"sv,
    },
    {
      .name = "vector_route"sv,
      .type_name = "vector<from_vector>"sv,
      .annotation = "vector value_type route"sv,
    },
  };

  const std::vector<rendered_type> k_expected_from_tuple{
    {
      .type_name = "from_tuple"sv,
      .qualified_type_name = "dependency::from_tuple"sv,
      .annotation = "tuple element dependency"sv,
    },
    {
      .type_name = "box"sv,
      .qualified_type_name = "dependency::box"sv,
      .annotation = ""sv,
    },
  };

  const std::vector<rendered_type> k_expected_from_variant{
    {
      .type_name = "from_variant"sv,
      .qualified_type_name = "dependency::from_variant"sv,
      .annotation = "variant alternative dependency"sv,
    },
    {
      .type_name = "box"sv,
      .qualified_type_name = "dependency::box"sv,
      .annotation = ""sv,
    },
  };

  const std::vector<rendered_type> k_expected_from_vector{
    {
      .type_name = "from_vector"sv,
      .qualified_type_name = "dependency::from_vector"sv,
      .annotation = "vector value_type dependency"sv,
    },
  };

  EXPECT_EQ(k_expected_type, rendered.type);
  EXPECT_EQ(k_expected_protocol_types, rendered.protocol_types);
  EXPECT_EQ(k_expected_public_fields, rendered.public_fields);
  EXPECT_EQ(k_expected_from_tuple, rendered.from_tuple);
  EXPECT_EQ(k_expected_from_variant, rendered.from_variant);
  EXPECT_EQ(k_expected_from_vector, rendered.from_vector);
}

/// Compatibility
/// ---------------------------------------------------------------

TEST(example, cpp14_cpp17_binding_name) {
  using namespace std::string_view_literals;

  const auto read_name = [](const auto _binding) -> std::string_view {
    // Generic `auto` lambda arguments are available since C++14.
    //
    // `const omni::binding_t binding = _binding;` uses omitted class-template
    // specialization, which is available since C++17. It gives the binding a
    // concrete interface type without spelling the reflected type.
    const omni::binding_t binding = _binding;
    return binding.type_name();
  };

  EXPECT_EQ("foobar_record"sv,
    omni::reflected_call(read_name, exposition::foobar_record{}));
}

namespace cpp11 {

// C++11 has no generic lambdas. Use a named visitor with a templated
// `operator()` instead.
struct type_name {
  template <typename T>
  const char *operator()(omni::binding_t<const T &> binding) const {
    return binding.type_name();
  }

  template <typename _M>
  const char *operator()(omni::meta_t<_M> type) const {
    return type.type_name();
  }
};

} // namespace cpp11

TEST(example, cpp11_struct_visitors) {
  const exposition::foobar_record record{};

  EXPECT_STREQ("foobar_record",
    omni::reflected_call(cpp11::type_name{}, record));

  EXPECT_STREQ("status",
    omni::reflected_call(cpp11::type_name{}, omni::type<example::status>));
}

/// Advanced examples ----------------------------------------------------------

namespace record_conversion {

template <typename From, typename To>
To convert(From &&from, omni::type_t<To> target_type) {
  const auto visit = //
    [](omni::record_binding auto source, omni::record_meta auto target) -> To {
    [[maybe_unused]] static auto assign_matching_field = //
      [](omni::field_binding auto from,
        omni::field_binding auto... to) -> void {
      constexpr bool found = //
        ((std::string_view(to.name()) == std::string_view(from.name())) || ...);

      static_assert(found, "destination has no same-named field");
      (std::invoke(
         [&from](omni::field_binding auto to) -> void {
           if constexpr (std::string_view(to.name())
             == std::string_view(from.name())) {
             static_assert(requires { to.ref() = std::move(from).value(); });
             // Like optional/expected value access, moving the field binding
             // selects consuming access. `std::move(from.ref())` is the
             // explicit equivalent for referenceable fields.
             to.ref() = std::move(from).value();
           }
         },
         to),
        ...);
    };

    To result{};
    // Bind the existing destination non-owningly so conversion does not move
    // the complete object into and back out of binding storage.
    omni::record_binding auto destination = target.bind(result);
    std::apply(
      [&destination](omni::field_binding auto... f) -> void {
        std::apply(
          [&f...](omni::field_binding auto... to_field) -> void {
            (std::invoke(assign_matching_field, std::move(f), to_field...),
              ...);
          },
          destination.public_fields());
      },
      source.public_fields());

    return result;
  };

  return omni::reflected_call(visit, std::forward<From>(from), target_type);
}

struct source {
  std::string name;
  std::unique_ptr<int> payload;
};

struct destination {
  // Destination fields are deliberately ordered differently.
  std::unique_ptr<int> payload;
  std::string name;
};

} // namespace record_conversion

TEST(example, convert_moves_same_named_fields) {
  record_conversion::source input{
    .name = "example",
    .payload = std::make_unique<int>(42),
  };

  auto output = record_conversion::convert(std::move(input),
    omni::type<record_conversion::destination>);

  EXPECT_EQ("example", output.name);
  ASSERT_NE(nullptr, output.payload);
  EXPECT_EQ(42, *output.payload);
  EXPECT_EQ(nullptr, input.payload);
}

namespace writable_fields {

struct nonassignable {
  int value = 7;

  nonassignable() = default;
  nonassignable(const nonassignable &) = delete;
  nonassignable &operator=(const nonassignable &) = delete;
};

struct record {
  int writable;
  const int fixed;
  // TODO(High): provide a safe whole-field access path for misaligned packed
  // raw arrays without changing their declared type.
  int raw[2];
  std::array<int, 2> values;
  nonassignable locked;
  unsigned bitfield : 8;
};

} // namespace writable_fields

TEST(example, filter_fields_before_set_value) {
  writable_fields::record input{
    .writable = 1,
    .fixed = 2,
    .raw = {3, 4},
    .values = {5, 6},
    .locked = {},
    .bitfield = 5,
  };

  omni::reflected_call(
    [](omni::record_binding auto binding) -> void {
      std::apply(
        [](omni::field_binding auto... field) -> void {
          const auto set_if_assignable = //
            []<omni::field_binding Field>(Field field) -> void {
            constexpr std::string_view field_name = field.name();
            constexpr auto replacement = std::array{42, 43};

            // std::array supports whole-value assignment. Misaligned packed
            // raw arrays currently have no whole-field accessor.
            if constexpr ("values" == field_name) {
              if constexpr (std::is_assignable_v<typename Field::type &,
                              decltype(replacement)>) {
                field.set_value(replacement);
              }
            } else if constexpr (std::is_assignable_v<typename Field::type &,
                                   int>) {
              // set_value also supports bitfields, whose address cannot be
              // taken.
              field.set_value(42);
            }
          };

          (set_if_assignable(field), ...);
        },
        binding.public_fields());
    },
    input);

  EXPECT_EQ(42, input.writable);
  EXPECT_EQ(2, input.fixed);
  EXPECT_EQ(3, input.raw[0]);
  EXPECT_EQ(4, input.raw[1]);

  const auto k_expected_values = std::array{42, 43};
  EXPECT_EQ(k_expected_values, input.values);

  EXPECT_EQ(7, input.locked.value);
  EXPECT_EQ(42u, input.bitfield);
}

namespace field_visibility {

struct root_base {
  int shared;
  int root;
};

struct middle_base: root_base {
  int shared;
  int middle;
};

struct record: middle_base {
  int own;
};

template <typename T>
struct repeated_base {
  T value;
};

struct ambiguous_bases: repeated_base<int>, repeated_base<double> {
  int own;
};

} // namespace field_visibility

TEST(example, public_fields_follow_member_visibility) {
  using namespace std::string_view_literals;

  field_visibility::record input{};
  input.root_base::shared = 1;
  input.root = 2;
  input.middle_base::shared = 3;
  input.middle = 4;
  input.own = 5;

  const std::vector expected{
    std::pair{"root"sv, 2},
    std::pair{"shared"sv, 3},
    std::pair{"middle"sv, 4},
    std::pair{"own"sv, 5},
  };

  // `middle_base::shared` hides `root_base::shared`, so `public_fields()`
  // exposes only the member visible through `record`. Accessing hidden base
  // storage through a derived binding is not considered a worthwhile scenario
  // for the current implementation.
  EXPECT_EQ(expected,
    omni::reflected_call(
      [](omni::record_binding auto binding)
        -> std::vector<std::pair<std::string_view, int>> {
        return std::apply(
          [](omni::field_binding auto... field)
            -> std::vector<std::pair<std::string_view, int>> {
            return {{field.name(), field.value()}...};
          },
          binding.public_fields());
      },
      input));
}

TEST(example, public_fields_omit_ambiguous_base_members) {
  using namespace std::string_view_literals;

  field_visibility::ambiguous_bases input{};
  static_cast<field_visibility::repeated_base<int> &>(input).value = 3;
  static_cast<field_visibility::repeated_base<double> &>(input).value = 4.5;
  input.own = 5;

  const std::vector expected{
    std::pair{"own"sv, 5},
  };

  // Both inherited `value` members require explicit base qualification, so
  // neither is visible through `ambiguous_bases` or exposed by public_fields().
  EXPECT_EQ(expected,
    omni::reflected_call(
      [](omni::record_binding auto binding)
        -> std::vector<std::pair<std::string_view, int>> {
        return std::apply(
          [](omni::field_binding auto... field)
            -> std::vector<std::pair<std::string_view, int>> {
            return {{field.name(), field.value()}...};
          },
          binding.public_fields());
      },
      input));
}

namespace variant_visitation {

struct a {
  int value;
};

struct b {
  bool value;
};

struct c {
  std::string value;
};

struct reflected_type_name {
  template <typename T>
  std::string_view operator()(const T &value) const {
    return omni::reflected_call(
      [](omni::binding auto binding) -> std::string_view {
        return binding.type_name();
      },
      value);
  }
};

template <typename... T>
std::string_view type_name(const std::variant<T...> &value) {
  return std::visit(reflected_type_name{}, value);
}

template <typename... T>
std::string_view type_name(const mpark::variant<T...> &value) {
  return mpark::visit(reflected_type_name{}, value);
}

} // namespace variant_visitation

TEST(example, visit_compound_variant_inputs) {
  using namespace std::string_view_literals;
  using namespace variant_visitation;

  using std_value = std::variant<a, b, c>;
  EXPECT_EQ("a"sv, type_name(std_value{a{}}));
  EXPECT_EQ("b"sv, type_name(std_value{b{}}));
  EXPECT_EQ("c"sv, type_name(std_value{c{}}));

  using mpark_value = mpark::variant<a, b, c>;
  EXPECT_EQ("a"sv, type_name(mpark_value{a{}}));
  EXPECT_EQ("b"sv, type_name(mpark_value{b{}}));
  EXPECT_EQ("c"sv, type_name(mpark_value{c{}}));
}

namespace schema {

/// user profile
struct profile {
  /// account tier enum
  enum class tier {
    basic,
    premium,
  };

  /// mailing address
  struct address {
    /// city name
    std::string city;

    /// postal code
    int postal_code;
  };

  /// database id
  int id;

  /// account enabled
  bool active;

  /// display name
  std::string name;

  /// account tier
  tier account_tier;
};

/// address collection
struct address_book {
  /// known addresses
  std::vector<profile::address> addresses;

  /// public labels
  std::set<std::string> tags;

  /// named addresses
  std::map<std::string, profile::address> address_by_name;
};

/// feature toggle
struct feature_toggle {
  /// enabled flag
  bool enabled;

  /// rollout percentage
  double rollout;

  /// owner aliases
  std::set<std::string> owners;
};

template <typename T>
void render_schema(std::ostringstream &out,
  omni::type_t<T>,
  std::string_view indent);

void render_schema(std::ostringstream &out,
  omni::meta auto schema,
  std::string_view indent) {
  if constexpr (omni::reflected_entity::record == schema.entity()) {
    out << indent << "type: object\n";
    out << indent << "name: " << schema.type_name() << '\n';
    out << indent << "documentation: " << schema.documentation() << '\n';
    out << indent << "properties:\n";

    omni::compat::apply(
      [&out, indent](omni::field_meta auto... field) {
        const auto render_field = //
          [&out, indent]<omni::field_meta Field>(Field field) {
            out << indent << "  - name: " << field.name() << '\n';
            out << indent << "    documentation: " << field.documentation()
                << '\n';
            out << indent << "    schema:\n";
            render_schema(out,
              omni::type<typename Field::type>,
              std::string{indent} + "      ");
          };

        (render_field(field), ...);
      },
      schema.public_fields());
  } else {
    static_assert(omni::reflected_entity::enumeration == schema.entity());

    out << indent << "type: string\n";
    out << indent << "name: " << schema.type_name() << '\n';
    out << indent << "documentation: " << schema.documentation() << '\n';
    out << indent << "enum:\n";

    for (const auto &value_name : schema.enumerators()) {
      out << indent << "  - " << value_name.second << '\n';
    }
  }
}

template <typename T>
void render_schema(std::ostringstream &out,
  omni::type_t<T>,
  std::string_view indent) {
  if constexpr (omni::is_reflected<T>::value) {
    render_schema(out, omni::reflected(omni::type<T>), indent);
  } else if constexpr (omni::traits::is<std::basic_string, T>()) {
    out << indent << "type: string\n";
  } else if constexpr (std::is_same_v<bool, T>) {
    out << indent << "type: boolean\n";
  } else if constexpr (std::is_integral_v<T>) {
    out << indent << "type: integer\n";
  } else if constexpr (std::is_floating_point_v<T>) {
    out << indent << "type: number\n";
  } else if constexpr (meta::map_like<T>) {
    out << indent << "type: object\n";
    out << indent << "additionalProperties:\n";
    render_schema(out,
      omni::type<typename T::mapped_type>,
      std::string{indent} + "  ");
  } else if constexpr (meta::range_like<T>) {
    out << indent << "type: array\n";
    out << indent << "items:\n";
    render_schema(out,
      omni::type<typename T::value_type>,
      std::string{indent} + "  ");
  }
}

template <typename T>
std::string annotate_schema(omni::type_t<T>) {
  const auto annotate = [](omni::meta auto) -> std::string {
    std::ostringstream out;
    render_schema(out, omni::type<T>, "");
    return out.str();
  };

  return omni::reflected_call(annotate, omni::type<T>);
}

} // namespace schema

TEST(example, annotated_schema_profile) {
  const std::string k_expected_profile =
    "type: object\n"
    "name: profile\n"
    "documentation: user profile\n"
    "properties:\n"
    "  - name: id\n"
    "    documentation: database id\n"
    "    schema:\n"
    "      type: integer\n"
    "  - name: active\n"
    "    documentation: account enabled\n"
    "    schema:\n"
    "      type: boolean\n"
    "  - name: name\n"
    "    documentation: display name\n"
    "    schema:\n"
    "      type: string\n"
    "  - name: account_tier\n"
    "    documentation: account tier\n"
    "    schema:\n"
    "      type: string\n"
    "      name: profile::tier\n"
    "      documentation: account tier enum\n"
    "      enum:\n"
    "        - basic\n"
    "        - premium\n";

  EXPECT_EQ(k_expected_profile,
    schema::annotate_schema(omni::type<schema::profile>));
}

TEST(example, annotated_schema_address_book) {
  const std::string k_expected_address_book =
    "type: object\n"
    "name: address_book\n"
    "documentation: address collection\n"
    "properties:\n"
    "  - name: addresses\n"
    "    documentation: known addresses\n"
    "    schema:\n"
    "      type: array\n"
    "      items:\n"
    "        type: object\n"
    "        name: profile::address\n"
    "        documentation: mailing address\n"
    "        properties:\n"
    "          - name: city\n"
    "            documentation: city name\n"
    "            schema:\n"
    "              type: string\n"
    "          - name: postal_code\n"
    "            documentation: postal code\n"
    "            schema:\n"
    "              type: integer\n"
    "  - name: tags\n"
    "    documentation: public labels\n"
    "    schema:\n"
    "      type: array\n"
    "      items:\n"
    "        type: string\n"
    "  - name: address_by_name\n"
    "    documentation: named addresses\n"
    "    schema:\n"
    "      type: object\n"
    "      additionalProperties:\n"
    "        type: object\n"
    "        name: profile::address\n"
    "        documentation: mailing address\n"
    "        properties:\n"
    "          - name: city\n"
    "            documentation: city name\n"
    "            schema:\n"
    "              type: string\n"
    "          - name: postal_code\n"
    "            documentation: postal code\n"
    "            schema:\n"
    "              type: integer\n";

  EXPECT_EQ(k_expected_address_book,
    schema::annotate_schema(omni::type<schema::address_book>));
}

TEST(example, annotated_schema_small_record) {
  const std::string k_expected_feature_toggle =
    "type: object\n"
    "name: feature_toggle\n"
    "documentation: feature toggle\n"
    "properties:\n"
    "  - name: enabled\n"
    "    documentation: enabled flag\n"
    "    schema:\n"
    "      type: boolean\n"
    "  - name: rollout\n"
    "    documentation: rollout percentage\n"
    "    schema:\n"
    "      type: number\n"
    "  - name: owners\n"
    "    documentation: owner aliases\n"
    "    schema:\n"
    "      type: array\n"
    "      items:\n"
    "        type: string\n";

  EXPECT_EQ(k_expected_feature_toggle,
    schema::annotate_schema(omni::type<schema::feature_toggle>));
}
