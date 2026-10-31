/** Executable reflection guide.
 * The examples use C++20 syntax for convenience. Omnirefl's reflection
 * interfaces remain C++11-compatible.
 * NOTE: `<concepts>` is deliberately excluded for toolchains with partial
 * C++20 support. The examples also use `std::find_if` rather than ranges
 * algorithms.
 *
 * For each suite.test entry below, search for TEST(suite, test) to find its
 * entry point; topics may have dedicated namespaces.
 * The suite.test name can also be passed to --gtest_filter.
 *
 * Basics
 *   basics.reflected_scope - introduce reflected_call with a generic visitor
 *   basics.reflected_scope_cpp11 - C++11 equivalent with a named visitor
 *   basics.reflected_scope_cpp17 - explicit metadata type for IDE completion
 *   basics.reflected_scope_indirection - metadata through a called function
 *   basics.enum_metadata - enumerator names, values, and declaration order
 *   basics.enum_declarations - enum class and plain enum support requirements
 *   basics.record_queries - read record names and documentation
 *   basics.accessing_members - accessible own and inherited members as tuples
 *   basics.data_member_queries - inspect qualifiers, bitfields, and deprecation
 *   basics.member_function_queries - names, parameters, returns, and comments
 *   basics.reflected_call_args - metadata versus object bindings
 *   basics.reading_data_members - read member values through object bindings
 *   basics.writing_data_members - update the original object's members
 *   basics.calling_member_functions - invoke through function bindings
 *   basics.discovering_dependencies - follow supported type discovery routes
 *
 * Advanced
 *   advanced.reflected_call_args - argument conversion and value categories
 *   advanced.binding_ownership - inspect ownership and constrain callables
 *   advanced.binding_as_metadata - inherited and unbound metadata
 *   advanced.data_member_value_read - metadata and binding value accessors
 *   advanced.data_member_value_write - assignment and reference accessors
 *   advanced.const_and_mutable_access - filter writable members by binding
 *   advanced.function_tags - metadata for free functions bound as data members
 *   advanced.custom_queries - select members by reference access
 *   advanced.bitfield_access - inspect bit widths and assign without references
 *   advanced.default_member_values - initializer presence versus value access
 *   advanced.union_metadata - inspect members without reading inactive values
 *   advanced.data_member_type_names - type spelling and documentation
 *   advanced.enum_value_name - find the name of a bound enum value
 *   advanced.primary_record_templates - primary-template metadata
 *   advanced.reflected_value_query - query metadata from an existing value
 *   advanced.metadata_skips - skip metadata, preserve discovery protocols
 *   advanced.nested_enum_metadata - enclosing names and documentation
 *   advanced.non_dependent_queries - guard early concrete-type queries
 *
 * Examples
 *   example.primary_template_from_map
 *   example.annotated_dependencies
 *   example.convert_moves_same_named_fields
 *   example.filter_fields_before_set_value
 *   example.accessible_data_members_follow_member_visibility
 *   example.accessible_data_members_omit_ambiguous_base_members
 *   example.accesses_static_members_through_a_record_binding
 *   example.visit_compound_variant_inputs
 *   example.annotated_schema_profile
 *   example.annotated_schema_address_book
 *   example.annotated_schema_small_record
 */

#include <gtest/gtest.h>

#include <omnirefl/functional.hpp>
// Public reflection entry point, metadata queries, and object bindings.
#include <omnirefl/reflected_scope.hpp>

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

namespace basics {

struct address {
  int postal_code;
};

/// Account with contact information and a retry count.
struct account {
  address contact;
  int retries;
};

/// Processing status.
enum class status {
  idle,
  active,
};

/** Omnirefl uses reflected_call as the instrumentation boundary for reflection.
 * To access reflection, pass a generic visitor.
 *
 * reflected_call invokes the visitor with reflected arguments and returns its
 * result. Here, omni::type<T> requests metadata without constructing T.
 *
 * An explicit visitor return type keeps its body from being instantiated during
 * instrumentation, before generated metadata exists. Use a trailing return
 * type even for a visitor returning void: -> void.
 *
 * NOTE: `constexpr auto result = reflected_call(...)` is unsupported because
 * it forces visitor instantiation before generated metadata exists. The visitor
 * itself and its internal calculations can still be constexpr.
 */
TEST(basics, reflected_scope) {
  const auto name = omni::reflected_call(
    [](omni::record_meta auto record) -> std::string_view {
      // Reflected scope: metadata for the arguments and their discovered
      // dependencies is available here and in functions called from here.
      return record.type_name();
    },
    omni::type<account>);

  EXPECT_EQ("account", name);
}

// C++11 has no generic lambdas; use a named template callable instead.
// Generic lambda parameters (auto) become available in C++14.
struct cpp11_lambda_type_name {
  // _M is internal and required for this template declaration.
  template <typename _M>
  std::string operator()(omni::record_meta_t<_M> record) const {
    return record.type_name();
  }
};

/** C++20 concepts and their wrapper templates (omni:: omitted):
 *   meta                   -> meta_t<_M>
 *   record_meta            -> record_meta_t<_M>
 *   enum_meta              -> enum_meta_t<_M>
 *   data_member_meta       -> data_member_meta_t<_M>
 *   function_meta          -> function_meta_t<_M>
 *   function_param_meta    -> function_param_meta_t<_M>
 *   function_return_meta   -> function_return_meta_t<_M>
 *   binding                -> binding_t<T>
 *   record_binding         -> record_binding_t<T>
 *   enum_binding           -> enum_binding_t<T>
 *   data_member_binding    -> data_member_binding_t<Record, _M>
 *   function_binding       -> function_binding_t<Record, _M>
 *
 * Metadata concepts also accept the corresponding binding wrappers.
 * Binding forms are introduced below; T preserves the value's cv/ref type.
 */
TEST(basics, reflected_scope_cpp11) {
  const std::string name = omni::reflected_call(
    cpp11_lambda_type_name{}, omni::type_t<account>{});

  EXPECT_EQ("account", name);
}

// NOTE: C++17 CTAD gives the auto parameter an explicit metadata type to help
// IDE completion expose the metadata interface.
TEST(basics, reflected_scope_cpp17) {
  const auto name = omni::reflected_call(
    [](auto value) -> std::string {
      const omni::meta_t meta = value;
      return meta.type_name();
    },
    omni::type<account>);

  EXPECT_EQ("account", name);
}

// NOTE: Keeping reflection helpers in a dedicated namespace makes their scope
// requirement visible and helps avoid accidental out-of-scope calls. This
// convention is not required by instrumentation.
namespace reflection_helpers {

// Allowed within a reflected scope, including functions called by the visitor.
// Calling it outside that scope is not allowed.
std::string_view qualified_type_name(omni::record_meta auto record) {
  // Includes enclosing namespaces: "account" becomes "basics::account".
  return record.qualified_type_name();
}

} // namespace reflection_helpers

/** Indirections within a reflected scope are supported.
 * NOTE: Nested reflected_call calls are unsupported and are not detected,
 * including calls reached indirectly through other functions. Metadata for
 * their types may be available incidentally; this does not make nesting valid.
 */
TEST(basics, reflected_scope_indirection) {
  // Omnirefl detects out-of-scope metadata use on a best-effort basis.
  // Uncomment to get an instrumentation error.
  // reflection_helpers::qualified_type_name(
  //   omni::reflected(omni::type<account>));

  const auto name = omni::reflected_call(
    [](omni::record_meta auto record) -> std::string_view {
      // Functions called from the visitor can also query reflection metadata.
      return reflection_helpers::qualified_type_name(record);
    },
    omni::type<account>);

  EXPECT_EQ("basics::account", name);
}

// Enumerators are value/name pairs in declaration order.
TEST(basics, enum_metadata) {
  const auto enumerators = omni::reflected_call(
    [](omni::enum_meta auto e)
      -> std::vector<std::pair<status, std::string_view>> {
      const auto entries = e.enumerators();
      // Names refer to generated string literals with static storage.
      // Destroying the local entries array does not invalidate these views.
      return {entries.begin(), entries.end()};
    },
    omni::type<status>);

  const std::vector<std::pair<status, std::string_view>> expected{
    {status::idle, "idle"},
    {status::active, "active"},
  };

  EXPECT_EQ(expected, enumerators);

  const auto documentation = omni::reflected_call(
    [](omni::enum_meta auto e) -> std::string_view {
      static_assert(omni::reflected_entity::enumeration == e.entity());
      return e.documentation();
    },
    omni::type<status>);

  EXPECT_EQ("Processing status.", documentation);
}

// NOTE: enum class works with or without an explicit underlying type.
// A plain enum at global or namespace scope needs an explicit underlying type,
// such as `: int`, so generated metadata can forward-declare it.
// Local and unnamed enums are unsupported as reflected_call inputs.
namespace enum_declarations {

enum class scoped { idle, active };
enum unscoped_supported : int { ready, stopped };
enum unscoped_unsupported { first, last };

} // namespace enum_declarations

// At namespace scope, a plain enum needs an explicit underlying type;
// enum class does not.
TEST(basics, enum_declarations) {
  using namespace enum_declarations;

  const auto name = [](omni::enum_meta auto e) -> std::string_view {
    return e.type_name();
  };

  EXPECT_EQ("scoped", omni::reflected_call(name, omni::type<scoped>));
  EXPECT_EQ("unscoped_supported",
    omni::reflected_call(name, omni::type<unscoped_supported>));

  // Uncomment to get an instrumentation error: this plain enum has no fixed
  // underlying type and cannot be forward-declared by generated metadata.
  // omni::reflected_call(name, omni::type<unscoped_unsupported>);
}

// Record names and documentation are available without constructing a record.
TEST(basics, record_queries) {
  struct description {
    std::string_view name;
    std::string_view qualified_name;
    std::string_view documentation;
  };

  const auto result = omni::reflected_call(
    [](omni::record_meta auto record) -> description {
      static_assert(omni::reflected_entity::record == record.entity());
      return {
        .name = record.type_name(),
        .qualified_name = record.qualified_type_name(),
        .documentation = record.documentation(),
      };
    },
    omni::type<account>);

  EXPECT_EQ("account", result.name);
  EXPECT_EQ("basics::account", result.qualified_name);
  EXPECT_EQ("Account with contact information and a retry count.",
    result.documentation);
}

namespace accessible_members {

struct base {
  int inherited;
  inline static const int shared = 7;
  int inherited_function() const { return inherited; }
};

struct derived: base {
  int own;
  const int fixed = 1;
  mutable int cache;
  volatile int observed;
  unsigned flags : 3;
  [[deprecated]] int legacy;
  int own_function() const { return john_cena; }
private:
  int john_cena; // Can't see.
};

} // namespace accessible_members

/** A member is accessible when foo.member is valid from global scope, where
 * foo has the reflected type: no ambiguity or private/protected access error.
 * This also includes inherited members made publicly accessible by that type,
 * regardless of their original access level.
 * Metadata is returned as tuples whose sizes are known at compile time. */
TEST(basics, accessing_members) {
  struct member_counts {
    std::size_t data_members;
    std::size_t member_functions;
  };

  const auto counts = omni::reflected_call(
    [](omni::record_meta auto record) -> member_counts {
      return {
        .data_members = std::tuple_size<
          decltype(record.accessible_data_members())>::value,
        .member_functions = std::tuple_size<
          decltype(record.accessible_member_functions())>::value,
      };
    },
    omni::type<accessible_members::derived>);

  EXPECT_EQ(8U, counts.data_members);
  EXPECT_EQ(2U, counts.member_functions);
}

// Inspecting deprecated-member metadata is intentional in this example.
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4996)
#elif defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

// Metadata describes member qualifiers without constructing or reading an
// object. Mapping the tuple renders a description for each accessible member.
TEST(basics, data_member_queries) {
  // GCC reports this static local as unused despite its use in the visitor.
  [[maybe_unused]] static const auto format_member =
    [](omni::data_member_meta auto field) {
      std::ostringstream out;
      out << (field.is_static() ? "static " : "")
          << (field.is_const() ? "const " : "")
          << (field.is_mutable() ? "mutable " : "")
          << (field.is_volatile() ? "volatile " : "")
          << (field.is_bitfield() ? "bitfield " : "")
          << (field.is_deprecated() ? "deprecated " : "")
          << field.name();
      return out.str();
    };

  const auto descriptions = omni::reflected_call(
    [](omni::record_meta auto record) -> std::vector<std::string> {
      return std::apply(
        [](auto... description) {
          return std::vector<std::string>{std::move(description)...};
        },
        record.accessible_data_members() | omni::fn::map(format_member));
    },
    omni::type<accessible_members::derived>);

  const std::vector<std::string> expected{
    "inherited",
    "static const shared",
    "own",
    "const fixed",
    "mutable cache",
    "volatile observed",
    "bitfield flags",
    "deprecated legacy",
  };

  EXPECT_EQ(expected, descriptions);
}

#if defined(_MSC_VER)
#  pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif

namespace member_functions {

struct counter {
  int value;

  /** Increase the counter.
   * @param delta Amount to add.
   * @return Updated value. */
  int add(int delta) { return value += delta; }
};

} // namespace member_functions

/** Member functions have their own metadata: names, arity, parameter metadata,
 * return metadata, and documentation. Parameter and return documentation comes
 * from @param and @return commands attached to the function declaration.
 * NOTE: Overloaded member functions are currently skipped with warnings.
 * Free functions have limited support through function_t data members;
 * see `advanced.function_tags`.
 */
TEST(basics, member_function_queries) {
  struct description {
    std::string_view name;
    std::string_view documentation;
    std::size_t arity;
    std::vector<std::string_view> parameters;
    std::vector<std::string_view> parameter_documentation;
    std::string_view return_type;
    std::string_view return_documentation;
  };

  const auto functions = omni::reflected_call(
    [](omni::record_meta auto record) -> std::vector<description> {
      const auto describe = [](omni::function_meta auto func) {
        return func.parameters()
          | omni::fn::foldl(
            [](description result, omni::function_param_meta auto param) {
              result.parameters.push_back(param.name());
              result.parameter_documentation.push_back(param.documentation());
              return result;
            },
            description{
              .name = func.name(),
              .documentation = func.documentation(),
              .arity = func.arity(),
              .parameters = {},
              .parameter_documentation = {},
              .return_type = func.returns().spelled_type_name(),
              .return_documentation = func.returns().documentation(),
            });
      };

      return std::apply(
        [](auto... descriptions) {
          return std::vector<description>{std::move(descriptions)...};
        },
        record.accessible_member_functions() | omni::fn::map(describe));
    },
    omni::type<member_functions::counter>);

  EXPECT_EQ(1U, functions.size());
  for (const auto &func : functions) {
    EXPECT_EQ("add", func.name);
    EXPECT_EQ("Increase the counter.", func.documentation);
    EXPECT_EQ(1U, func.arity);
    const std::vector<std::string_view> parameters{"delta"};
    const std::vector<std::string_view> documentation{"Amount to add."};
    EXPECT_EQ(parameters, func.parameters);
    EXPECT_EQ(documentation, func.parameter_documentation);
    EXPECT_EQ("int", func.return_type);
    EXPECT_EQ("Updated value.", func.return_documentation);
  }
}

namespace data_member_bindings {

struct position {
  int x;
  int y;
};

struct empty_record {};

} // namespace data_member_bindings

/** reflected_call wraps each argument in the corresponding metadata or binding
 * before passing it to the visitor. See `advanced.reflected_call_args` for the
 * detailed argument-conversion and value-category rules.
 * Metadata and non-owning bindings are trivial to copy. An owning binding
 * follows its stored object's copy semantics and may be non-copyable.
 *
 * NOTE: Non-reflectable types are errors as direct arguments. Unsupported
 * record/enum dependencies are skipped with warnings. Fundamental types and
 * standard-library records receive no metadata; their omission alone does not
 * produce a warning.
 * The caller is responsible for unwrapping compound types such as tuples,
 * vectors, variants, and raw arrays before passing their records or enums to
 * reflected_call. Other data can be captured by the visitor instead of passed
 * as reflected arguments.
 */
TEST(basics, reflected_call_args) {
  // Uncomment either line to get an instrumentation error.
  // omni::reflected_call([](auto...) -> void {}, 815);
  // omni::reflected_call([](auto...) -> void {}, std::string{"text"});

  // A type tag provides metadata without constructing an object.
  const auto type_name = omni::reflected_call(
    [](omni::record_meta auto record) -> std::string_view {
      return record.type_name();
    },
    omni::type<data_member_bindings::position>);

  // An existing object provides a binding: metadata plus access to that object.
  data_member_bindings::position point{3, 7};
  const auto bound_type_name = omni::reflected_call(
    [](omni::record_binding auto record) -> std::string_view {
      // Bindings inherit the corresponding metadata interface.
      return record.type_name();
    },
    point);

  EXPECT_EQ("position", type_name);
  EXPECT_EQ(type_name, bound_type_name);
}

// Read data-member values from an existing record.
TEST(basics, reading_data_members) {
  data_member_bindings::position point{3, 7};
  const auto read = [](omni::record_binding auto record) -> std::vector<int> {
    return std::apply(
      [](omni::data_member_binding auto... member) {
        // value() reads the member from its bound record.
        return std::vector<int>{member.value()...};
      },
      // A record binding returns data-member bindings here.
      // Member bindings are trivial to copy; they never own the object
      // or extend its lifetime.
      record.accessible_data_members());
  };

  const auto values = omni::reflected_call(read, point);
  const std::vector<int> expected{3, 7};
  EXPECT_EQ(expected, values);

  data_member_bindings::empty_record empty;
  EXPECT_TRUE(omni::reflected_call(read, empty).empty());
}

// A data-member binding's set_value() writes to the original object.
TEST(basics, writing_data_members) {
  data_member_bindings::position point{3, 7};
  omni::reflected_call(
    // This optional constraint signals mutation of the caller's object through a
    // reference, rather than mutation of an owned copy.
    []<omni::record_binding Record>(Record record)
      -> std::enable_if_t<!Record::owning::value> {
      record.accessible_data_members()
        | omni::fn::each([](omni::data_member_binding auto member) {
            member.set_value(member.value() + 10);
          });
    },
    point);

  EXPECT_EQ(13, point.x);
  EXPECT_EQ(17, point.y);
}

// A record binding returns function bindings that invoke through that record.
TEST(basics, calling_member_functions) {
  member_functions::counter counter{7};
  // This optional constraint signals mutation of the caller's object through a
  // reference, rather than mutation of an owned copy.
  const auto call = []<omni::record_binding Record>(Record record)
    -> std::enable_if_t<!Record::owning::value, std::vector<int>> {
    return std::apply(
      [](auto... values) { return std::vector<int>{values...}; },
      record.accessible_member_functions()
        | omni::fn::map([](omni::function_binding auto func) {
            return func(5);
          }));
  };

  const auto results = omni::reflected_call(call, counter);
  const std::vector<int> expected{12};
  EXPECT_EQ(expected, results);
  EXPECT_EQ(12, counter.value);

  data_member_bindings::position no_functions{3, 7};
  EXPECT_TRUE(omni::reflected_call(call, no_functions).empty());
}

namespace discovering_dependencies {

struct base { int id; };
struct location { double latitude; };
struct request { int limit; };
struct response { bool accepted; };
struct sample { int value; };
struct note { std::string text; };
enum class state { idle, active };

struct document: base { // Public bases are discovered.
  // Supported public alias names: type, value_type, first_type, second_type,
  // key_type, mapped_type, error_type, and value.
  //
  // Arbitrary aliases are not discovery routes. Here vector's own value_type
  // leads to sample.
  using value_type = std::vector<sample>;
  location where; // Public data-member types are discovered.
  std::tuple<note, int> info; // Tuple elements are discovered.
  std::variant<state, int> status; // Variant alternatives are discovered.
  response process(request) const; // Parameter and return types are discovered.
};

} // namespace discovering_dependencies

/** Reflection discovers dependencies of a reflected_call argument's type
 * through the routes shown above, following them transitively. Their metadata
 * is available in the same reflected scope; no additional reflected_call is
 * needed.
 * Standard-library records do not receive metadata, but their supported aliases
 * and tuple/variant elements still expose dependencies. */
TEST(basics, discovering_dependencies) {
  using namespace discovering_dependencies;

  [[maybe_unused]] static const auto name =
    []<typename T>(omni::type_t<T>) -> std::string_view {
      if constexpr (omni::is_reflected<T>::value)
        return omni::reflected(omni::type<T>).type_name();
      else
        return "not reflected";
    };

  const auto names = omni::reflected_call(
    [](omni::record_meta auto) -> std::vector<std::string_view> {
      return {
        // Test-only ad hoc: name each concrete type to check that its discovery
        // route works. C++ may check these calls before metadata is generated,
        // so hide them during instrumentation. Application code should instead
        // obtain these types from the visitor's reflected parameters.
#if !defined(OMNI_TOOL_RUN)
        name(omni::type<base>),
        name(omni::type<location>),
        name(omni::type<request>),
        name(omni::type<response>),
        name(omni::type<sample>),
        name(omni::type<note>),
        name(omni::type<state>),
        name(omni::type<document::value_type>),
        name(omni::type<decltype(document::info)>),
        name(omni::type<decltype(document::status)>),
#endif
      };
    },
    omni::type<document>);

  const std::vector<std::string_view> expected{
    "base",
    "location",
    "request",
    "response",
    "sample",
    "note",
    "state",
    "not reflected", // std::vector
    "not reflected", // std::tuple
    "not reflected", // std::variant
  };

  EXPECT_EQ(expected, names);
}

} // namespace basics

/// Advanced examples ----------------------------------------------------------

namespace call_arguments {

struct payload {
  std::unique_ptr<int> value;
};

enum class mode { read, write };

} // namespace call_arguments

/** reflected_call accepts 1..N arguments. Each is converted to a reflected
 * wrapper and passed to the corresponding visitor parameter; the visitor's
 * signature must accept that number and those kinds of arguments.
 *
 * omni::type_t<Record> becomes record metadata; omni::type_t<Enum> becomes
 * enum metadata. Object arguments become non-owning bindings, including
 * rvalues; the call itself does not move their values.
 * If needed, one can create an owning binding with meta.bind(...).
 * See `advanced.binding_ownership` for ownership checks and constraints.
 *
 * Argument forms for a record T:
 *   omni::type_t<T> -> record_meta_t<_M>
 *   T&             -> record_binding_t<T&>
 *   const T&       -> record_binding_t<const T&>
 *   T&&            -> record_binding_t<T&&>
 *
 * NOTE: Unsupported direct arguments produce hard instrumentation errors.
 * Unsupported record/enum dependencies are skipped with warnings. Omitting
 * fundamental types and standard-library records does not itself warn.
 * Arbitrary compound class templates cannot always be distinguished from
 * ordinary records; detection is best effort.
 * For variants, pass the active alternative with std::visit or mpark::visit;
 * see `example.visit_compound_variant_inputs`. */
TEST(advanced, reflected_call_args) {
  using namespace call_arguments;

  const auto names = omni::reflected_call(
    [](omni::record_meta auto record, omni::enum_meta auto e)
      -> std::pair<std::string_view, std::string_view> {
      return {record.type_name(), e.type_name()};
    },
    omni::type<payload>,
    omni::type<mode>);

  const std::pair<std::string_view, std::string_view> expected_names{
    "payload", "mode"};

  EXPECT_EQ(expected_names, names);

  payload original{std::make_unique<int>(7)};
  const auto value = omni::reflected_call(
    [](omni::record_binding auto binding) -> int {
      static_assert(!decltype(binding)::owning::value);
      return *binding.ref().value;
    },
    original);

  EXPECT_EQ(7, value);
  EXPECT_EQ(7, *original.value);

  const payload constant{std::make_unique<int>(9)};
  const auto const_value = omni::reflected_call(
    [](omni::record_binding auto binding) -> int {
      static_assert(!decltype(binding)::owning::value);
      static_assert(std::is_const_v<
        std::remove_reference_t<decltype(binding.ref())>>);
      return *binding.value().value;
    },
    constant);

  EXPECT_EQ(9, const_value);
  EXPECT_EQ(9, *constant.value);

  // This optional constraint signals mutation of the caller's object through a
  // reference, rather than mutation of an owned copy.
  const auto move_value = []<omni::record_binding Record>(Record binding)
    -> std::enable_if_t<!Record::owning::value, payload &&> {
    return std::move(binding).value();
  };

  payload source{std::make_unique<int>(42)};
  payload &&returned = omni::reflected_call(move_value, std::move(source));
  static_assert(std::is_rvalue_reference_v<decltype(returned)>);
  EXPECT_EQ(42, *source.value); // Returning an xvalue has not moved it yet.

  payload destination = std::move(returned);
  EXPECT_EQ(42, *destination.value);
  EXPECT_EQ(nullptr, source.value);

  // Consume the returned xvalue before this temporary's lifetime ends.
  payload temporary = omni::reflected_call(
    move_value, payload{std::make_unique<int>(15)});

  EXPECT_EQ(15, *temporary.value);
}

namespace binding_ownership {

struct record {
  int value = 7;
};

// C++20: accept only owning record bindings.
template <omni::record_binding Binding>
  requires Binding::owning::value
int read_owned(const Binding &binding) {
  return binding.value().value;
}

// C++11: the ownership check can also participate in SFINAE.
template <typename Binding,
  typename std::enable_if<Binding::owning::value, int>::type = 0>
int read_owned_cpp11(const Binding &binding) {
  return binding.value().value;
}

} // namespace binding_ownership

/** owning::value reports whether a binding stores its own object or refers to
 * another object. If needed, one can create an owning binding with meta.bind():
 * no argument default-constructs an owned object, an rvalue is stored by value,
 * and an lvalue remains referenced. Ownership can constrain template overloads.
 */
TEST(advanced, binding_ownership) {
  struct observations {
    bool argument_owns;
    bool default_owns;
    bool lvalue_owns;
    bool rvalue_owns;
    int constrained_value;
    int sfinae_value;
  };

  binding_ownership::record input{13};
  const auto result = omni::reflected_call(
    [](omni::record_binding auto borrowed) -> observations {
      const auto meta = borrowed.as_meta();
      auto owned = meta.bind();
      auto lvalue_borrowed = meta.bind(borrowed.ref());
      auto rvalue_owned = meta.bind(binding_ownership::record{29});

      // Neither overload participates for a non-owning binding.
      static_assert(!requires { binding_ownership::read_owned(borrowed); });
      static_assert(
        !requires { binding_ownership::read_owned_cpp11(borrowed); });

      return {
        .argument_owns = decltype(borrowed)::owning::value,
        .default_owns = decltype(owned)::owning::value,
        .lvalue_owns = decltype(lvalue_borrowed)::owning::value,
        .rvalue_owns = decltype(rvalue_owned)::owning::value,
        .constrained_value = binding_ownership::read_owned(owned),
        .sfinae_value = binding_ownership::read_owned_cpp11(rvalue_owned),
      };
    },
    input);

  EXPECT_FALSE(result.argument_owns);
  EXPECT_TRUE(result.default_owns);
  EXPECT_FALSE(result.lvalue_owns);
  EXPECT_TRUE(result.rvalue_owns);
  EXPECT_EQ(7, result.constrained_value);
  EXPECT_EQ(29, result.sfinae_value);
  EXPECT_EQ(13, input.value);
}

/** Bindings inherit metadata and satisfy its concept; binding concepts refine
 * metadata concepts for overload selection. as_meta() returns metadata by
 * value, without retaining the object. Its member query returns unbound
 * metadata.
 * _M is internal; use the wrapper's reflected_type for the represented type.
 */
TEST(advanced, binding_as_metadata) {
  const auto count = omni::reflected_call(
    [](omni::record_meta auto record) -> std::size_t {
      static_assert(omni::record_binding<decltype(record)>);
      const auto meta = record.as_meta();
      static_assert(!omni::record_binding<decltype(meta)>);
      using Record = typename decltype(meta)::reflected_type;
      static_assert(std::is_same_v<Record,
        basics::data_member_bindings::position>);
      return std::tuple_size<decltype(meta.accessible_data_members())>::value;
    },
    basics::data_member_bindings::position{});

  EXPECT_EQ(2U, count);
}

namespace member_value_access {

struct record {
  std::string first;
  std::string second;
};

} // namespace member_value_access

/** Unbound metadata takes the record as an explicit argument. A data-member
 * binding already refers to its record, so its accessors need no argument.
 * ref(), operator*, and operator-> require has_reference_access(); bitfields
 * and misaligned packed members cannot expose references.
 * Data-member bindings always borrow their record and cannot outlive it.
 * Their record type preserves the bound object's cv qualification. */
TEST(advanced, data_member_value_read) {
  struct access_paths {
    std::vector<std::string> metadata;
    std::vector<std::string> binding;
    std::vector<std::string> reference;
    std::vector<std::string> dereferenced;
    std::vector<std::string> arrow;
  };

  member_value_access::record input{"north", "south"};
  const auto values = omni::reflected_call(
    [](omni::record_binding auto record) -> access_paths {
      return record.accessible_data_members()
        | omni::fn::foldl(
          [&record](access_paths values,
            omni::data_member_binding auto member) {
            using Meta = typename decltype(member)::meta;
            values.metadata.push_back(Meta::value(record.ref()));
            values.binding.push_back(member.value());
            values.reference.push_back(member.ref());
            values.dereferenced.push_back(*member);
            values.arrow.push_back(member->substr());
            return values;
          },
          access_paths{});
    },
    input);

  const std::vector<std::string> expected{"north", "south"};
  EXPECT_EQ(expected, values.metadata);
  EXPECT_EQ(expected, values.binding);
  EXPECT_EQ(expected, values.reference);
  EXPECT_EQ(expected, values.dereferenced);
  EXPECT_EQ(expected, values.arrow);
}

/** set_value() supports writable members even when references are unavailable.
 * For referenceable members, ref(), operator*, and operator-> expose direct
 * access to the member. See `advanced.const_and_mutable_access` for const and
 * mutable members. */
TEST(advanced, data_member_value_write) {
  member_value_access::record input{"north", "south"};
  omni::reflected_call(
    // This optional constraint signals mutation of the caller's object through a
    // reference, rather than mutation of an owned copy.
    []<omni::record_binding Record>(Record record)
      -> std::enable_if_t<!Record::owning::value> {
      record.accessible_data_members()
        | omni::fn::each([&record](omni::data_member_binding auto member) {
            using Meta = typename decltype(member)::meta;
            Meta::set_value(record.ref(), member.value() + " meta");
            member.set_value(member.value() + " value");
            member.ref() += " ref";
            *member += " *";
            member->append(" ->");
          });
    },
    input);

  EXPECT_EQ("north meta value ref * ->", input.first);
  EXPECT_EQ("south meta value ref * ->", input.second);
}

namespace qualified_access {

struct record {
  int regular;
  const int fixed;
  mutable int cache;
};

// Check assignment for the actual binding, including its record's constness.
template <typename Member>
struct writable: std::bool_constant<requires(Member member) {
  member.set_value(typename Member::type{});
}> {};

} // namespace qualified_access

/** is_const() describes the declared member type, not the bound record's
 * constness. A const record prevents writing ordinary members but still allows
 * writing mutable members. Declared const members remain read-only.
 * Filter by assignment availability on the binding before writing its value.
 */
TEST(advanced, const_and_mutable_access) {
  // This optional constraint signals mutation of the caller's object through a
  // reference, rather than mutation of an owned copy.
  const auto write = []<omni::record_binding Record>(Record record)
    -> std::enable_if_t<!Record::owning::value, std::vector<std::string_view>> {
    const auto writable = record.accessible_data_members()
      | omni::fn::filter<qualified_access::writable>();

    writable | omni::fn::each([](omni::data_member_binding auto member) {
      member.set_value(42);
    });

    return std::apply(
      [](omni::data_member_binding auto... member) {
        return std::vector<std::string_view>{member.name()...};
      },
      writable);
  };

  qualified_access::record object{3, 7, 11};
  const auto writable = omni::reflected_call(write, object);
  const std::vector<std::string_view> expected{"regular", "cache"};
  EXPECT_EQ(expected, writable);
  EXPECT_EQ(42, object.regular);
  EXPECT_EQ(7, object.fixed);
  EXPECT_EQ(42, object.cache);

  const qualified_access::record const_object{3, 7, 11};
  const auto const_writable = omni::reflected_call(write, const_object);
  const std::vector<std::string_view> expected_const{"cache"};
  EXPECT_EQ(expected_const, const_writable);
  EXPECT_EQ(3, const_object.regular);
  EXPECT_EQ(7, const_object.fixed);
  EXPECT_EQ(42, const_object.cache);
}

namespace function_tags {

int double_value(int value) { return 2 * value; }

struct commands {
  omni::function_t<double_value> twice;
  int ignored;
};

} // namespace function_tags

/** omni::function_t binds a free function as a data member. That member still
 * has data-member metadata; has_callable_metadata() identifies the special case
 * and callable_metadata() exposes the associated function metadata.
 * OMNI_FUNCTION(double_value) provides a tag spelling across language versions.
 */
TEST(advanced, function_tags) {
  const auto invoke = [](omni::record_meta auto record) -> std::vector<int> {
    const auto callable = record.accessible_data_members()
      | omni::fn::filter([]<typename Member>() {
          using Meta = omni::compat::remove_cvref_t<Member>;
          return Meta::has_callable_metadata();
        });

    return std::apply(
      [](auto... values) { return std::vector<int>{values...}; },
      callable | omni::fn::map([](omni::data_member_meta auto member) {
        const auto func = member.callable_metadata();
        return std::invoke(func.pointer(), 21);
      }));
  };

  const auto results = omni::reflected_call(
    invoke, omni::type<function_tags::commands>);

  const std::vector<int> expected{42};
  EXPECT_EQ(expected, results);

  const auto no_functions = omni::reflected_call(
    invoke, omni::type<basics::data_member_bindings::position>);

  EXPECT_TRUE(no_functions.empty());
}

namespace custom_queries {

struct record {
  int value;
  const int fixed = 1;
  unsigned bits : 3;
};

// An application-defined predicate can test whether an accessor is available.
template <typename Member>
struct reference_accessible: std::bool_constant<requires(Member member) {
  member.ref();
}> {};

} // namespace custom_queries

/** Custom predicates can select members for an operation before invoking it.
 * Reference access and writability are different: a const member can expose a
 * reference, whereas a bitfield cannot. This query checks ref() availability.
 */
TEST(advanced, custom_queries) {
  custom_queries::record input{7, 11, 3};
  const auto names = omni::reflected_call(
    [](omni::record_binding auto record) -> std::vector<std::string_view> {
      return std::apply(
        [](omni::data_member_binding auto... member) {
          return std::vector<std::string_view>{member.name()...};
        },
        record.accessible_data_members()
          | omni::fn::filter<custom_queries::reference_accessible>());
    },
    input);

  const std::vector<std::string_view> expected{"value", "fixed"};
  EXPECT_EQ(expected, names);
}

namespace bitfield_access {

struct record {
  int foo_count;
  unsigned bar_count : 5;
  int untouched_count;
};

} // namespace bitfield_access

/** Bitfields expose value access and assignment, but cannot provide references.
 * Query the declared width with bit_width(), available for supported bitfields.
 * Member names are also constexpr and can select operations at compile time.
 * NOTE: Width expressions that depend on template parameters are not supported
 * yet. Omnirefl omits bit_width() with a warning for those members. In generic
 * code, requires { member.bit_width(); } checks whether the query is available;
 * is_bitfield() alone does not guarantee width access.
 */
TEST(advanced, bitfield_access) {
  bitfield_access::record input{1, 2, 3};
  omni::reflected_call(
    // This optional constraint signals mutation of the caller's object through a
    // reference, rather than mutation of an owned copy.
    []<omni::record_binding Record>(Record record)
      -> std::enable_if_t<!Record::owning::value> {
      record.accessible_data_members()
        | omni::fn::each([](omni::data_member_binding auto member) {
            if constexpr (member.is_bitfield()) {
              static_assert(5 == member.bit_width());
              static_assert(!member.has_reference_access());
              member.set_value(15);
            } else if constexpr (std::string_view(member.name()).find("foo")
              != std::string_view::npos) {
              member.set_value(8);
            }
          });
    },
    input);

  EXPECT_EQ(8, input.foo_count);
  EXPECT_EQ(15U, input.bar_count);
  EXPECT_EQ(3, input.untouched_count);
}

namespace default_member_values {

int computed_value() { return 42; }

struct record {
  int literal = 7;
  int computed = computed_value();
  int required;
};

} // namespace default_member_values

/** An initializer's presence does not guarantee default_value() access.
 * Omnirefl can reproduce supported literal initializers; an initializer calling
 * a function still belongs to the original declaration but is not replayed.
 * default_value() currently returns a cached value at runtime.
 */
TEST(advanced, default_member_values) {
  struct description {
    std::string_view name;
    bool initialized;
    bool value_access;
  };

  const auto descriptions = omni::reflected_call(
    [](omni::record_meta auto record) -> std::vector<description> {
      return std::apply(
        [](auto... member) {
          return std::vector<description>{description{member.name(),
            member.has_default_member_initializer(),
            member.has_default_value_access()}...};
        },
        record.accessible_data_members());
    },
    omni::type<default_member_values::record>);

  for (const auto &member : descriptions) {
    EXPECT_EQ("required" != member.name, member.initialized);
    EXPECT_EQ("literal" == member.name, member.value_access);
  }

  const auto defaults = omni::reflected_call(
    [](omni::record_meta auto record) -> std::vector<int> {
      return std::apply(
        [](auto... member) {
          return std::vector<int>{member.default_value()...};
        },
        record.accessible_data_members()
          | omni::fn::filter([]<typename Member>() {
              using Meta = omni::compat::remove_cvref_t<Member>;
              return Meta::has_default_value_access();
            }));
    },
    omni::type<default_member_values::record>);

  const std::vector<int> expected{7};
  EXPECT_EQ(expected, defaults);
}

namespace union_metadata {

union number {
  int integer;
  double real;
};

} // namespace union_metadata

/** Union metadata lists the declared members without reading their values.
 * NOTE: Reflection does not identify a union's active member. The caller must
 * know which member is active before reading through a binding.
 */
TEST(advanced, union_metadata) {
  const auto names = omni::reflected_call(
    [](omni::record_meta auto record) -> std::vector<std::string_view> {
      static_assert(record.is_union());
      return std::apply(
        [](auto... member) {
          return std::vector<std::string_view>{member.name()...};
        },
        record.accessible_data_members());
    },
    omni::type<union_metadata::number>);

  const std::vector<std::string_view> expected{"integer", "real"};
  EXPECT_EQ(expected, names);
}

namespace member_type_names {

struct record {
  /// Number of entries.
  int count;
  /// Display labels.
  std::vector<std::string> labels;
};

} // namespace member_type_names

/** Source-spelled names preserve aliases and template arguments without needing
 * metadata for the member type. spelled_type_name() omits its outer namespace;
 * spelled_qualified_type_name() restores namespaces where Clang can resolve
 * them. Documentation comes from the comment attached to the member.
 */
TEST(advanced, data_member_type_names) {
  struct descriptions {
    std::vector<std::string_view> names;
    std::vector<std::string_view> spelling;
    std::vector<std::string_view> qualified_spelling;
    std::vector<std::string_view> documentation;
  };

  const auto result = omni::reflected_call(
    [](omni::record_meta auto record) -> descriptions {
      return record.accessible_data_members()
        | omni::fn::foldl(
          [](descriptions result, omni::data_member_meta auto member) {
            result.names.push_back(member.name());
            result.spelling.push_back(member.spelled_type_name());
            result.qualified_spelling.push_back(
              member.spelled_qualified_type_name());
            result.documentation.push_back(member.documentation());
            return result;
          },
          descriptions{});
    },
    omni::type<member_type_names::record>);

  const std::vector<std::string_view> names{"count", "labels"};
  const std::vector<std::string_view> spelling{"int", "vector<std::string>"};
  const std::vector<std::string_view> qualified_spelling{
    "int", "std::vector<std::string>"};

  const std::vector<std::string_view> documentation{
    "Number of entries.", "Display labels."};

  EXPECT_EQ(names, result.names);
  EXPECT_EQ(spelling, result.spelling);
  EXPECT_EQ(qualified_spelling, result.qualified_spelling);
  EXPECT_EQ(documentation, result.documentation);
}

// Find the bound enum value's name in the metadata's value/name list.
TEST(advanced, enum_value_name) {
  const auto name = omni::reflected_call(
    [](omni::enum_binding auto e) -> std::string_view {
      const auto enumerators = e.enumerators();
      const auto found = std::find_if(enumerators.begin(),
        enumerators.end(),
        [&e](const auto &entry) { return entry.first == e.value(); });

      return enumerators.end() == found ? "unknown" : found->second;
    },
    basics::status::idle);

  EXPECT_EQ("idle", name);
}

namespace primary_record_templates {

template <typename Char>
struct text_record {
  /// display title
  std::basic_string<Char> title;

  /// number of revisions
  int revisions;
};

} // namespace primary_record_templates

/** Metadata is generated for the primary record template and reused for its
 * instantiations. Member access resolves types for the current instantiation.
 * NOTE: Explicit and partial record-template specializations are unsupported.
 */
TEST(advanced, primary_record_templates) {
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

  const primary_record_templates::text_record<char> record{
    .title = "Guide"s,
    .revisions = 2,
  };

  const auto read_fields =
    [](omni::record_binding auto record) -> std::vector<rendered_field> {
      static_assert(record.is_template());
      const auto render = [](omni::data_member_binding auto member) {
        std::ostringstream text;
        text << member.value();
        return rendered_field{
          .name = member.name(),
          .annotation = member.documentation(),
          .value = text.str(),
        };
      };

      return std::apply(
        [](auto... fields) {
          return std::vector<rendered_field>{std::move(fields)...};
        },
        record.accessible_data_members() | omni::fn::map(render));
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

TEST(advanced, reflected_value_query) {
  using namespace std::string_view_literals;

  const auto query_value = [](omni::record_binding auto b) -> std::string_view {
    // `omni::reflected(value)` is a reflected-scope convenience query. It keeps
    // the value qualification instead of forcing the user to spell `decltype`.
    const omni::record_binding auto same_value = omni::reflected(b.ref());
    return same_value.type_name();
  };

  EXPECT_EQ("position"sv,
    omni::reflected_call(query_value,
      basics::data_member_bindings::position{}));
}

namespace metadata_tweaks {

struct element { int value; };

template <typename T>
struct view {
  using value_type = T;
  T *data;
};

struct record {
  view<element> values;
};

} // namespace metadata_tweaks

/** skip_metadata requests omit metadata generation for selected types, rather
 * than merely silencing their warnings. Supported discovery protocols still
 * expose their dependencies. See the guarded request at the end of this file.
 *
 * NOTE: Requests are experimental and apply throughout the translation unit,
 * regardless of scope or position. A specialization selects its primary
 * template, so the request applies to its other specializations as well.
 * Keep requests together or in a dedicated header included under OMNI_TOOL_RUN;
 * avoid conditional template usage. Skipped types cannot be direct arguments.
 */
TEST(advanced, metadata_skips) {
  struct observations {
    bool view_reflected;
    bool element_reflected;
  };

  const auto result = omni::reflected_call(
    [](omni::record_meta auto record) -> observations {
      using Record = typename decltype(record)::reflected_type;
      using View = decltype(Record::values);
      return {
        .view_reflected = omni::is_reflected<View>::value,
        .element_reflected =
          omni::is_reflected<typename View::value_type>::value,
      };
    },
    omni::type<metadata_tweaks::record>);

  EXPECT_FALSE(result.view_reflected);
  EXPECT_TRUE(result.element_reflected);
}

namespace nested_declarations {

struct record {
  /// State declared inside a record.
  enum class state { ready, paused };
};

} // namespace nested_declarations

// An enum's name retains its enclosing record, with namespaces added by the
// qualified-name query. Its own documentation remains attached to the enum.
TEST(advanced, nested_enum_metadata) {
  struct description {
    std::string_view name;
    std::string_view qualified_name;
    std::string_view documentation;
    std::vector<std::string_view> enumerators;
  };

  const auto result = omni::reflected_call(
    [](omni::enum_meta auto e) -> description {
      std::vector<std::string_view> names;
      for (const auto &[value, name] : e.enumerators())
        names.push_back(name);

      return {e.type_name(),
        e.qualified_type_name(),
        e.documentation(),
        std::move(names)};
    },
    omni::type<nested_declarations::record::state>);

  EXPECT_EQ("record::state", result.name);
  EXPECT_EQ("nested_declarations::record::state", result.qualified_name);
  EXPECT_EQ("State declared inside a record.", result.documentation);
  const std::vector<std::string_view> expected{"ready", "paused"};
  EXPECT_EQ(expected, result.enumerators);
}

/** Naming a concrete type can cause C++ to check metadata before the visitor is
 * instantiated. Derive queried types from reflected parameters instead.
 * This test deliberately names a concrete type to demonstrate the guarded form.
 */
TEST(advanced, non_dependent_queries) {
  const auto available = omni::reflected_call(
    [](omni::record_meta auto) -> bool {
      // Test-only ad hoc: metadata does not exist during instrumentation.
      // A fix for these early concrete-type queries is under consideration.
#if !defined(OMNI_TOOL_RUN)
      return omni::is_reflected<basics::data_member_bindings::position>::value;
#else
      return false;
#endif
    },
    omni::type<basics::data_member_bindings::position>);

  EXPECT_TRUE(available);
}

/// Examples ----------------------------------------------------------

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
      [&init]<omni::data_member_meta... Field>(
        Field... field) -> primary_template::result {
        return {
          init(omni::type<typename Field::type>, field.name())...,
        };
      },
      result_type.accessible_data_members());
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
        // Dependency metadata is available in the same reflected scope. Use
        // `reflected(type_t<T>{})` for a value and `meta_for<T>` to name its
        // type.
        using dependency = typename reflected_record::type;
        using dependency_meta = omni::meta_for<dependency>;
        const dependency_meta metadata =
          omni::reflected(omni::type_t<dependency>{});

        out.protocol_types.push_back({
          .protocol = "type",
          .type = render_type(metadata),
        });
      }

      if constexpr (requires { typename reflected_record::value_type; }) {
        out.protocol_types.push_back({
          .protocol = "value_type",
          .type = render_type(omni::reflected(
            omni::type_t<typename reflected_record::value_type>{})),
        });
      }

      if constexpr (requires { typename reflected_record::first_type; }) {
        out.protocol_types.push_back({
          .protocol = "first_type",
          .type = render_type(omni::reflected(
            omni::type_t<typename reflected_record::first_type>{})),
        });
      }

      if constexpr (requires { typename reflected_record::second_type; }) {
        out.protocol_types.push_back({
          .protocol = "second_type",
          .type = render_type(omni::reflected(
            omni::type_t<typename reflected_record::second_type>{})),
        });
      }

      if constexpr (requires { typename reflected_record::key_type; }) {
        out.protocol_types.push_back({
          .protocol = "key_type",
          .type = render_type(omni::reflected(
            omni::type_t<typename reflected_record::key_type>{})),
        });
      }

      if constexpr (requires { typename reflected_record::error_type; }) {
        out.protocol_types.push_back({
          .protocol = "error_type",
          .type = render_type(omni::reflected(
            omni::type_t<typename reflected_record::error_type>{})),
        });
      }

      if constexpr (requires { typename reflected_record::value; }) {
        out.protocol_types.push_back({
          .protocol = "value",
          .type = render_type(omni::reflected(
            omni::type_t<typename reflected_record::value>{})),
        });
      }

      if constexpr (requires { typename reflected_record::mapped_type; }) {
        out.protocol_types.push_back({
          .protocol = "mapped_type",
          .type = render_type(omni::reflected(
            omni::type_t<typename reflected_record::mapped_type>{})),
        });
      }

      record.accessible_data_members()
        | omni::fn::each(
          [&out, render_type]<omni::data_member_meta Member>(Member member) {
            out.public_fields.push_back({
              .name = member.name(),
              .type_name = member.spelled_type_name(),
              .annotation = member.documentation(),
            });

            if constexpr (omni::traits::is<std::tuple,
                            typename Member::type>()) {
              [&out, render_type]<typename... T>(
                omni::type_t<std::tuple<T...>>) {
                (out.from_tuple.push_back(
                   render_type(omni::reflected(omni::type_t<T>{}))),
                  ...);
              }(omni::type_t<typename Member::type>{});
            }

            if constexpr (omni::traits::is<std::variant,
                            typename Member::type>()) {
              [&out, render_type]<typename... T>(
                omni::type_t<std::variant<T...>>) {
                (out.from_variant.push_back(
                   render_type(omni::reflected(omni::type_t<T>{}))),
                  ...);
              }(omni::type_t<typename Member::type>{});
            }

            if constexpr (omni::traits::is<std::vector,
                            typename Member::type>()) {
              out.from_vector.push_back(render_type(omni::reflected(
                omni::type_t<typename Member::type::value_type>{})));
            }
          });

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

namespace record_conversion {

template <typename From, typename To>
To convert(From &&from, omni::type_t<To> target_type) {
  // This optional constraint signals mutation of the caller's object through a
  // reference, rather than mutation of an owned copy.
  const auto visit =
    []<omni::record_binding Source>(Source source,
      omni::record_meta auto target)
      -> std::enable_if_t<!Source::owning::value, To> {
      To result{};

      // Bind the existing destination without moving its storage.
      auto destination = target.bind(result);
      source.accessible_data_members()
        | omni::fn::each([&destination](omni::data_member_binding auto from) {
            const auto matching = destination.accessible_data_members()
              | omni::fn::filter([]<typename Member>() {
                  using Target = omni::compat::remove_cvref_t<Member>;
                  return std::string_view(decltype(from)::name())
                    == std::string_view(Target::name());
                });

            static_assert(0 != std::tuple_size<decltype(matching)>::value,
              "destination has no same-named field");
            matching
              | omni::fn::each([&from](omni::data_member_binding auto to) {
                  static_assert(requires {
                    to.ref() = std::move(from).value();
                  });
                  // Moving the binding selects move access to its member.
                  // std::move(from.ref()) is equivalent here.
                  to.ref() = std::move(from).value();
                });
          });

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
    // This optional constraint signals mutation of the caller's object through a
    // reference, rather than mutation of an owned copy.
    []<omni::record_binding Record>(Record binding)
      -> std::enable_if_t<!Record::owning::value> {
      // Scalar assignment includes bitfields but excludes const, array, and
      // nonassignable members before the write operation is instantiated.
      binding.accessible_data_members()
        | omni::fn::filter([]<typename Member>() {
            using Field = omni::compat::remove_cvref_t<Member>;
            return std::is_assignable_v<typename Field::type &, int>;
          })
        | omni::fn::each([](omni::data_member_binding auto member) {
            member.set_value(42);
          });

      // std::array supports whole-value assignment; raw arrays do not.
      static constexpr auto replacement = std::array{42, 43};
      binding.accessible_data_members()
        | omni::fn::filter([]<typename Member>() {
            using Field = omni::compat::remove_cvref_t<Member>;
            return std::is_assignable_v<typename Field::type &,
              decltype(replacement)>;
          })
        | omni::fn::each([](omni::data_member_binding auto member) {
            member.set_value(replacement);
          });
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

TEST(example, accessible_data_members_follow_member_visibility) {
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

  // `middle_base::shared` hides `root_base::shared`, so
  // `accessible_data_members()` exposes only the member visible through
  // `record`. Accessing hidden base storage through a derived binding is not
  // considered a worthwhile scenario for the current implementation.
  EXPECT_EQ(expected,
    omni::reflected_call(
      [](omni::record_binding auto binding)
        -> std::vector<std::pair<std::string_view, int>> {
        return std::apply(
          [](omni::data_member_binding auto... field)
            -> std::vector<std::pair<std::string_view, int>> {
            return {{field.name(), field.value()}...};
          },
          binding.accessible_data_members());
      },
      input));
}

TEST(example, accessible_data_members_omit_ambiguous_base_members) {
  using namespace std::string_view_literals;

  field_visibility::ambiguous_bases input{};
  static_cast<field_visibility::repeated_base<int> &>(input).value = 3;
  static_cast<field_visibility::repeated_base<double> &>(input).value = 4.5;
  input.own = 5;

  const std::vector expected{
    std::pair{"own"sv, 5},
  };

  // Both inherited `value` members require explicit base qualification, so
  // neither is visible through `ambiguous_bases` or exposed by
  // accessible_data_members().
  EXPECT_EQ(expected,
    omni::reflected_call(
      [](omni::record_binding auto binding)
        -> std::vector<std::pair<std::string_view, int>> {
        return std::apply(
          [](omni::data_member_binding auto... field)
            -> std::vector<std::pair<std::string_view, int>> {
            return {{field.name(), field.value()}...};
          },
          binding.accessible_data_members());
      },
      input));
}

/// Static members ------------------------------------------------------------

namespace static_access {

struct counter {
  int local;
  inline static int total = 10;

  static int advance(int delta) { return total + delta; }
};

} // namespace static_access

// Both member queries include static members. filter<fn::is_static>() selects
// shared members; filter<fn::non_static>() selects instance members. Access
// through either path still uses a record binding.
// TODO: [medium] Replace this synthetic counter with a practical example of
// static-member access and filtering.
TEST(example, accesses_static_members_through_a_record_binding) {
  const static_access::counter input{7};
  const auto result = omni::reflected_call(
    [](omni::record_binding auto record)
      -> std::pair<std::tuple<int>, std::tuple<int>> {
      return {
        record.accessible_data_members()
          | omni::fn::filter<omni::fn::is_static>()
          | omni::fn::map([](omni::data_member_binding auto field) {
              return field.value();
            }),
        record.accessible_member_functions()
          | omni::fn::filter<omni::fn::is_static>()
          | omni::fn::map([](omni::function_binding auto function) {
              return function(5);
            }),
      };
    },
    input);

  EXPECT_EQ(std::tuple{10}, result.first);
  EXPECT_EQ(std::tuple{15}, result.second);
  EXPECT_EQ(7, input.local);
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

template <typename T>
concept range_like = requires(T value) {
  typename T::value_type;
  value.begin();
  value.end();
};

template <typename T>
concept map_like = range_like<T> && requires { typename T::mapped_type; };

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

    schema.accessible_data_members()
      | omni::fn::each([&out, indent]<omni::data_member_meta Member>(
                          Member member) {
          out << indent << "  - name: " << member.name() << '\n';
          out << indent << "    documentation: " << member.documentation()
              << '\n';
          out << indent << "    schema:\n";
          render_schema(out,
            omni::type<typename Member::type>,
            std::string{indent} + "      ");
        });
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
  } else if constexpr (map_like<T>) {
    out << indent << "type: object\n";
    out << indent << "additionalProperties:\n";
    render_schema(out,
      omni::type<typename T::mapped_type>,
      std::string{indent} + "  ");
  } else if constexpr (range_like<T>) {
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

// TODO: [high] Extend the nested-enum example with nested records, explaining
// their supported discovery routes and declaration restrictions.

// TU-wide request; the guard keeps it out of ordinary compilation. The macro
// instantiates tweaks::skip_metadata without creating a namespace-scope object.
#if defined(OMNI_TOOL_RUN)
OMNI_SKIP_METADATA(metadata_tweaks::view<metadata_tweaks::element>);
#endif
