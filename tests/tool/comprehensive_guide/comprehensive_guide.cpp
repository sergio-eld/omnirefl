#include <gtest/gtest.h>

#include <omnirefl/reflection.hpp>

#include <algorithm>
#include <concepts>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace meta {

template <template <typename...> typename, typename>
struct is_template: std::false_type {};

template <template <typename...> typename Template, typename... T>
struct is_template<Template, Template<T...>>: std::true_type {};

template <template <typename...> typename Template, typename T>
consteval bool is() noexcept {
  return is_template<Template, T>::value;
}

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

namespace example {

enum class status {
  draft,
  active,
};

struct foobar_record {
  int foo_count;
  int bar_count;
  int untouched_count;
};

} // namespace example

TEST(example, foobar) {
  // C++20 and later: concepts give reflected visitors readable argument
  // constraints while keeping the reflected type generic.
  const auto write_foobar = [](omni::binding auto b)
    // The trailing return type keeps the visitor body from being instantiated
    // during the tool run, before generated reflection metadata exists.
    -> example::foobar_record {
      // Reflected scope: this body is instantiated after `reflected_call`
      // instruments the argument type and omnirefl generates its metadata.
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

      return std::move(b.value);
    };

  const auto record = omni::reflected_call(write_foobar,
    example::foobar_record{
      .foo_count = 1,
      .bar_count = 2,
      .untouched_count = 3,
    });

  EXPECT_EQ(8, record.foo_count);
  EXPECT_EQ(15, record.bar_count);
  EXPECT_EQ(3, record.untouched_count);
}

TEST(example, enum_names) {
  using namespace std::string_view_literals;

  EXPECT_EQ("draft"sv,
    omni::reflected_call(
      [](const omni::binding auto status) -> std::string_view {
        const auto enumerators = status.enumerators();
        const auto it = std::ranges::find(enumerators,
          status.value,
          [](const auto &value_name) { return value_name.first; });

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
      [](omni::meta auto status)
        -> std::vector<std::pair<example::status, std::string_view>> {
        const auto enumerators = status.enumerators();
        return {enumerators.begin(), enumerators.end()};
      },
      omni::type<example::status>));
}

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
    omni::reflected_call(read_name, example::foobar_record{}));
}

namespace cpp11 {

struct binding_name {
  template <typename T>
  const char *operator()(omni::binding_t<T> binding) const {
    return binding.type_name();
  }
};

struct meta_name {
  template <typename T>
  const char *operator()(omni::meta_t<T> type) const {
    return type.type_name();
  }
};

} // namespace cpp11

TEST(example, cpp11_struct_visitors) {
  // C++11 has no generic lambdas. Use a named visitor with a templated
  // `operator()` instead.
  EXPECT_STREQ("foobar_record",
    omni::reflected_call(cpp11::binding_name{}, example::foobar_record{}));

  EXPECT_STREQ("status",
    omni::reflected_call(cpp11::meta_name{}, omni::type<example::status>));
}

TEST(example, binding_storage_forms) {
  example::foobar_record mutable_record{
    .foo_count = 1,
    .bar_count = 2,
    .untouched_count = 3,
  };

  const example::foobar_record const_record{
    .foo_count = 4,
    .bar_count = 5,
    .untouched_count = 6,
  };

  const auto inspect = //
    [](omni::binding auto mutable_binding,
      omni::binding auto const_binding,
      omni::binding auto owning_binding) -> int {
    // Lvalue arguments become non-owning bindings. Temporary arguments become
    // owning bindings, so returning/moving the stored value is possible.
    static_assert(!decltype(mutable_binding)::owning::value);
    static_assert(!decltype(const_binding)::owning::value);
    static_assert(decltype(owning_binding)::owning::value);

    mutable_binding.value.foo_count = 8;

    return mutable_binding.value.foo_count + const_binding.value.bar_count
      + owning_binding.value.untouched_count;
  };

  EXPECT_EQ(22,
    omni::reflected_call(inspect,
      mutable_record,
      const_record,
      example::foobar_record{
        .foo_count = 7,
        .bar_count = 8,
        .untouched_count = 9,
      }));
  EXPECT_EQ(8, mutable_record.foo_count);
}

TEST(example, reflected_value_query) {
  using namespace std::string_view_literals;

  const auto query_value = [](omni::binding auto b) -> std::string_view {
    // `omni::reflected(value)` is a reflected-scope convenience query. It keeps
    // the value qualification instead of forcing the user to spell `decltype`.
    const omni::binding auto same_value = omni::reflected(b.value);
    return same_value.type_name();
  };

  EXPECT_EQ("foobar_record"sv,
    omni::reflected_call(query_value, example::foobar_record{}));
}

TEST(example, meta_bind_and_field_metadata) {
  const auto inspect = [](omni::meta auto record) -> int {
    // `meta.bind()` default-constructs an owning binding when the reflected type
    // is default constructible. `meta.bind(value)` binds an existing object.
    auto owned = record.bind();
    auto non_owning = record.bind(owned.value);

    return std::apply(
      [&non_owning]<omni::field_meta... Field>(Field... field) {
        // Field metadata can be used without first creating field bindings.
        // Indexes are local to the declaring record and follow declaration
        // order for this simple non-inherited record.
        ((field.set_value(non_owning.value,
           static_cast<typename Field::type>(field.index() + 1))),
          ...);

        return (field.value(non_owning.value) + ...);
      },
      record.public_fields());
  };

  EXPECT_EQ(6,
    omni::reflected_call(inspect, omni::type<example::foobar_record>));
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
    out << indent << "annotation: " << schema.annotation() << '\n';
    out << indent << "properties:\n";

    omni::compat::apply(
      [&out, indent](omni::field_meta auto... field) {
        const auto render_field = //
          [&out, indent]<omni::field_meta Field>(Field field) {
          out << indent << "  - name: " << field.name() << '\n';
          out << indent << "    annotation: " << field.annotation() << '\n';
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
    out << indent << "annotation: " << schema.annotation() << '\n';
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
    render_schema(out, omni::reflected<T>(), indent);
  } else if constexpr (meta::is<std::basic_string, T>()) {
    out << indent << "type: string\n";
  } else if constexpr (std::same_as<bool, T>) {
    out << indent << "type: boolean\n";
  } else if constexpr (std::integral<T>) {
    out << indent << "type: integer\n";
  } else if constexpr (std::floating_point<T>) {
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
    "annotation: user profile\n"
    "properties:\n"
    "  - name: id\n"
    "    annotation: database id\n"
    "    schema:\n"
    "      type: integer\n"
    "  - name: active\n"
    "    annotation: account enabled\n"
    "    schema:\n"
    "      type: boolean\n"
    "  - name: name\n"
    "    annotation: display name\n"
    "    schema:\n"
    "      type: string\n"
    "  - name: account_tier\n"
    "    annotation: account tier\n"
    "    schema:\n"
    "      type: string\n"
    "      name: profile::tier\n"
    "      annotation: account tier enum\n"
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
    "annotation: address collection\n"
    "properties:\n"
    "  - name: addresses\n"
    "    annotation: known addresses\n"
    "    schema:\n"
    "      type: array\n"
    "      items:\n"
    "        type: object\n"
    "        name: profile::address\n"
    "        annotation: mailing address\n"
    "        properties:\n"
    "          - name: city\n"
    "            annotation: city name\n"
    "            schema:\n"
    "              type: string\n"
    "          - name: postal_code\n"
    "            annotation: postal code\n"
    "            schema:\n"
    "              type: integer\n"
    "  - name: tags\n"
    "    annotation: public labels\n"
    "    schema:\n"
    "      type: array\n"
    "      items:\n"
    "        type: string\n"
    "  - name: address_by_name\n"
    "    annotation: named addresses\n"
    "    schema:\n"
    "      type: object\n"
    "      additionalProperties:\n"
    "        type: object\n"
    "        name: profile::address\n"
    "        annotation: mailing address\n"
    "        properties:\n"
    "          - name: city\n"
    "            annotation: city name\n"
    "            schema:\n"
    "              type: string\n"
    "          - name: postal_code\n"
    "            annotation: postal code\n"
    "            schema:\n"
    "              type: integer\n";

  EXPECT_EQ(k_expected_address_book,
    schema::annotate_schema(omni::type<schema::address_book>));
}

TEST(example, annotated_schema_small_record) {
  const std::string k_expected_feature_toggle =
    "type: object\n"
    "name: feature_toggle\n"
    "annotation: feature toggle\n"
    "properties:\n"
    "  - name: enabled\n"
    "    annotation: enabled flag\n"
    "    schema:\n"
    "      type: boolean\n"
    "  - name: rollout\n"
    "    annotation: rollout percentage\n"
    "    schema:\n"
    "      type: number\n"
    "  - name: owners\n"
    "    annotation: owner aliases\n"
    "    schema:\n"
    "      type: array\n"
    "      items:\n"
    "        type: string\n";

  EXPECT_EQ(k_expected_feature_toggle,
    schema::annotate_schema(omni::type<schema::feature_toggle>));
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

    bool operator==(const rendered_type &) const = default;
  };

  struct rendered_protocol_type {
    std::string_view protocol;
    rendered_type type;

    bool operator==(const rendered_protocol_type &) const = default;
  };

  struct rendered_field {
    std::string_view name;
    std::string_view type_name;
    std::string_view annotation;

    bool operator==(const rendered_field &) const = default;
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
    []<omni::meta Record>(Record record) -> rendered_context {
      const auto render_type = [](omni::meta auto type) -> rendered_type {
        return {
          .type_name = type.type_name(),
          .qualified_type_name = type.qualified_type_name(),
          .annotation = type.annotation(),
        };
      };

      rendered_context out{
        .type = render_type(record),
      };

      if constexpr (requires { typename Record::type::type; }) {
        out.protocol_types.push_back({
          .protocol = "type",
          .type = render_type(omni::reflected<typename Record::type::type>()),
        });
      }

      if constexpr (requires { typename Record::type::value_type; }) {
        out.protocol_types.push_back({
          .protocol = "value_type",
          .type =
            render_type(omni::reflected<typename Record::type::value_type>()),
        });
      }

      if constexpr (requires { typename Record::type::key_type; }) {
        out.protocol_types.push_back({
          .protocol = "key_type",
          .type =
            render_type(omni::reflected<typename Record::type::key_type>()),
        });
      }

      if constexpr (requires { typename Record::type::error_type; }) {
        out.protocol_types.push_back({
          .protocol = "error_type",
          .type =
            render_type(omni::reflected<typename Record::type::error_type>()),
        });
      }

      if constexpr (requires { typename Record::type::value; }) {
        out.protocol_types.push_back({
          .protocol = "value",
          .type = render_type(omni::reflected<typename Record::type::value>()),
        });
      }

      if constexpr (requires { typename Record::type::mapped_type; }) {
        out.protocol_types.push_back({
          .protocol = "mapped_type",
          .type =
            render_type(omni::reflected<typename Record::type::mapped_type>()),
        });
      }

      omni::compat::apply(
        [&out, render_type](omni::field_meta auto... field) {
          const auto render =
            [&out, render_type]<omni::field_meta Field>(Field field) {
            out.public_fields.push_back({
              .name = field.name(),
              .type_name = field.type_name(),
              .annotation = field.annotation(),
            });

            if constexpr (meta::is<std::tuple, typename Field::type>()) {
              std::invoke(
                [&out, render_type]<typename... T>(
                  omni::type_t<std::tuple<T...>>) {
                  (out.from_tuple.push_back(
                     render_type(omni::reflected<T>())),
                    ...);
                },
                omni::type<typename Field::type>);
            }

            if constexpr (meta::is<std::variant, typename Field::type>()) {
              std::invoke(
                [&out, render_type]<typename... T>(
                  omni::type_t<std::variant<T...>>) {
                  (out.from_variant.push_back(
                     render_type(omni::reflected<T>())),
                    ...);
                },
                omni::type<typename Field::type>);
            }

            if constexpr (meta::is<std::vector, typename Field::type>()) {
              out.from_vector.push_back(
                render_type(
                  omni::reflected<typename Field::type::value_type>()));
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
      .type = {
        .type_name = "protocol_type"sv,
        .qualified_type_name = "dependency::protocol_type"sv,
        .annotation = "protocol type alias dependency"sv,
      },
    },
    {
      .protocol = "value_type"sv,
      .type = {
        .type_name = "protocol_value_type"sv,
        .qualified_type_name = "dependency::protocol_value_type"sv,
        .annotation = "protocol value_type alias dependency"sv,
      },
    },
    {
      .protocol = "key_type"sv,
      .type = {
        .type_name = "protocol_key_type"sv,
        .qualified_type_name = "dependency::protocol_key_type"sv,
        .annotation = "protocol key_type alias dependency"sv,
      },
    },
    {
      .protocol = "error_type"sv,
      .type = {
        .type_name = "protocol_error_type"sv,
        .qualified_type_name = "dependency::protocol_error_type"sv,
        .annotation = "protocol error_type alias dependency"sv,
      },
    },
    {
      .protocol = "value"sv,
      .type = {
        .type_name = "protocol_value"sv,
        .qualified_type_name = "dependency::protocol_value"sv,
        .annotation = "protocol value alias dependency"sv,
      },
    },
    {
      .protocol = "mapped_type"sv,
      .type = {
        .type_name = "protocol_mapped_type"sv,
        .qualified_type_name = "dependency::protocol_mapped_type"sv,
        .annotation = "protocol mapped_type alias dependency"sv,
      },
    },
  };

  const std::vector<rendered_field> k_expected_public_fields{
    {
      .name = "base"sv,
      .type_name = "dependency::from_base"sv,
      .annotation = "field from public base"sv,
    },
    {
      .name = "field"sv,
      .type_name = "dependency::from_field"sv,
      .annotation = "direct field route"sv,
    },
    {
      .name = "tuple_route"sv,
      .type_name = "std::tuple<dependency::from_tuple, "
                   "dependency::box<dependency::from_tuple>>"sv,
      .annotation = "tuple route"sv,
    },
    {
      .name = "variant_route"sv,
      .type_name = "std::variant<dependency::from_variant, "
                   "dependency::box<dependency::from_variant>>"sv,
      .annotation = "variant route"sv,
    },
    {
      .name = "vector_route"sv,
      .type_name = "std::vector<dependency::from_vector>"sv,
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

namespace bitfield {

struct flags {
  unsigned enabled : 1;
  unsigned retries : 3;
  unsigned mode : 2;
};

} // namespace bitfield

TEST(example, write_bitfields) {
  using namespace std::string_view_literals;

  const auto set_flags = [](omni::binding auto b) -> bitfield::flags {
    omni::compat::apply(
      [](omni::field_binding auto... field) {
        const auto write = [](omni::field_binding auto field) {
          constexpr std::string_view field_name = field.name();

          if constexpr ("enabled"sv == field_name)
            field.set_value(1u);

          if constexpr ("retries"sv == field_name)
            field.set_value(5u);

          if constexpr ("mode"sv == field_name)
            field.set_value(3u);
        };

        (write(field), ...);
      },
      b.public_fields());

    return std::move(b.value);
  };

  const bitfield::flags flags =
    omni::reflected_call(set_flags, bitfield::flags{});

  EXPECT_EQ(1u, flags.enabled);
  EXPECT_EQ(5u, flags.retries);
  EXPECT_EQ(3u, flags.mode);
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

    bool operator==(const rendered_field &) const = default;
  };

  const primary_template::text_record<char> record{
    .title = "Guide"s,
    .revisions = 2,
  };

  const auto read_fields =
    [](omni::binding auto b) -> std::vector<rendered_field> {
    std::vector<rendered_field> out;

    omni::compat::apply(
      [&out](omni::field_binding auto... field) {
        const auto render = [&out](omni::field_binding auto field) {
          constexpr std::string_view field_name = field.name();

          if constexpr ("title"sv == field_name) {
            out.push_back({
              .name = field.name(),
              .annotation = field.annotation(),
              .value = field.value(),
            });
          }

          if constexpr ("revisions"sv == field_name) {
            out.push_back({
              .name = field.name(),
              .annotation = field.annotation(),
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

  const auto from_map =
    [&table]<omni::meta Result>(Result result_type) -> primary_template::result {
    const auto init =
      [&table]<typename T>(omni::type_t<T>, std::string_view name) -> T {
      const auto it = table.find(name);
      if (table.end() == it)
        return {};

      // Minimal example policy: missing keys and incompatible runtime values
      // become default field values. Real deserialization would usually report
      // those as errors.
      return std::visit(
        []<typename From>(const From &from) -> T {
          if constexpr (std::same_as<T, From>) {
            return from;
          } else if constexpr (meta::is<std::basic_string, T>()
            && meta::is<std::basic_string, From>()) {
            return {from.begin(), from.end()};
          } else if constexpr (meta::is<std::vector, T>()
            && meta::is<std::vector, From>()) {
            return {from.begin(), from.end()};
          } else {
            return {};
          }
        },
        it->second);
    };

    return std::apply(
      [&init]<omni::field_meta... Field>(Field... field)
        -> primary_template::result {
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
