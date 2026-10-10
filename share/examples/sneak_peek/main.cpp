#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>

#include <cassert>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// This example intentionally uses C++23 syntax and library facilities to reduce
// verbosity. The demonstrated reflection features remain available through
// Omnirefl's C++11-compatible interfaces.

// One .cpp; no declaration headers, metadata files, or reflection macros.

// Example visitor that prints reflected records during a depth-first walk
// of type dependencies. The struct keeps this logic below; an inline
// lambda would also work.
struct print_reachable_records {
  template <omni::record_meta Root>
  void operator()(Root) const;
};

template <typename T>
void print_reflection() {
  // `reflected_call` makes metadata for `T` and its type dependencies available
  // inside the visitor's `operator()` and the functions it calls.
  omni::reflected_call(print_reachable_records{}, omni::type<T>);
}

// Mutating example; its implementation showcases several reflected arguments
// in one `reflected_call`.
template <typename... Values>
void update_fields(Values *...values);

// Omnirefl reflects globally accessible, named records (structs, classes, or
// unions) and enums on demand.

namespace oceanic {
/// Declaration comments are available as reflection metadata.
struct vessel { //< Discovered as the `mapped_type` of `fleet<T>::vessels`.
  // Nested structs are supported recursively inside non-template records.
  struct position { //< Discovered via the `location` field.
    using degrees = double;

    degrees latitude = 0;
    degrees longitude = 0;
  };

  position location;

  // Reflected field properties are queryable.
  mutable std::string name = "before"; //< `.is_mutable()` is true.

  // Public non-static methods are reflected. Their parameter and return types
  // join dependency discovery. Overloaded methods are skipped with a warning.
  // Doxygen parameter descriptions, such as `@param` or `\param[in]`, are
  // available through parameter metadata.

  /** Check proximity.
   * @param other Other position.
   * @return Whether the positions are near.
   */
  bool is_near(position other) const {
    return location.latitude == other.latitude
      && location.longitude == other.longitude;
  }
};

/** Measure a distance.
 * @param scale Multiplier.
 * @return The scaled distance.
 */
double distance(vessel, double scale) {
  return scale;
}

// Primary templates are supported; names appear without template arguments.
template <typename T>
struct fleet { //< Discovered as an `omni::type_t` argument to `reflected_call`.
  // Unsupported nested records emit a warning if discovered as dependencies.
  struct not_supported {};

  // Free functions are reflected only when discovered through a record field
  // using the special `function_t` tag.
  omni::function_t<distance> measure;

  // Standard-library types are not reflected, but their dependency protocols
  // still apply. The specialization for `T` is discovered via `mapped_type`.
  std::map<std::string, T> vessels;
};

struct telemetry { //< Discovered as a value argument to `reflected_call`.
  // Writing bit-fields without UB is supported via `.set_value()`.
  mutable unsigned depth : 10 = 42;
  const unsigned sensor = 108; //< `.is_const()` is true.

  // Only public fields are reflected.
  private:
  [[maybe_unused]] unsigned john_cena = 49; //< can't see
};

} // namespace oceanic

// This (optional) namespace marks helpers that must not be used outside
// a reflected scope. Instrumentation checks out-of-scope queries (best effort).
namespace reflected_scope {

std::string render_annotation(std::string_view annotation,
  unsigned indent = 0) {
  return annotation
    | std::views::split('\n')
    | std::views::transform([indent](auto line) {
        return std::format("{:>{}}// {}\n", "", indent,
          std::string_view{line});
      })
    | std::views::join
    | std::ranges::to<std::string>();
}

std::string describe_function(omni::function_meta auto function) {
  namespace fn = omni::fn; //< Functional QoL for tuple-like values.

  const std::string parameters = function.parameters()
    | fn::map([](omni::function_param_meta auto p) {
        std::string description = std::format("{}{}",
          p.spelled_type_name(),
          std::string_view{p.name()}.empty()
            ? std::string{}
            : std::format(" {}", p.name()));

        return std::string_view{p.documentation()}.empty() //
          ? std::move(description)
          : std::format("{} [{}]", description, p.documentation());
      })
    // TODO(QoL): Add fn::to_array<std::string>() so this can use
    // `| fn::to_array<std::string>() | std::views::join_with(", "sv)`.
    // Handle empty tuples (zero-argument functions).
    | fn::foldl(
      [](std::string before, std::string_view p) {
        before += before.empty() ? "" : ", ";
        before += p;
        return before;
      },
      std::string{});

  return std::format("{}({}) -> {}; // {}\n",
    function.name(),
    parameters,
    function.returns().spelled_type_name(),
    function.documentation());
}

template <omni::field_meta Field>
std::string describe_field(Field field) {
  const auto annotation = render_annotation(field.documentation(), 2u);
  const auto field_description = std::format("  {}: {}", field.name(),
    // Aliases are preserved for field type names. For the
    // `oceanic::vessel::position::latitude` field, this returns
    // `vessel::position::degrees`.
    field.spelled_type_name());

  const std::optional default_value = std::invoke(
    []() -> std::optional<std::string> {
      /*
      ```cpp
      int make_value();
      struct defaults {
        int arithmetic = 8 * 100 + 15; //< default_value() available
        int computed = make_value();   //< skipped with a warning
      };
      ```
      */
      if constexpr (Field::has_default_value_access()
        && std::formattable<typename Field::type, char>)
        return std::format(" = {}", Field::default_value());

      return std::nullopt;
    });

  // Fundamental and standard-library types are not reflected, so the
  // metadata query for the actual type name is unavailable for them.
  if constexpr (omni::is_reflected<typename Field::type>::value)
    return std::format("{}{} (resolves to {}){};\n",
      annotation,
      field_description,
      omni::meta_for<typename Field::type>::type_name(),
      default_value.value_or(""));

  return std::format("{}{}{};\n",
    annotation,
    field_description,
    default_value.value_or(""));
}

std::string describe_record(omni::record_meta auto record) {
  namespace fn = omni::fn;

  const auto fields = record.public_fields()
    | fn::foldl(
      []<typename Member>(std::string before, Member member) {
        if constexpr (omni::function_meta<Member>)
          return before + "  " + describe_function(member);
        else
          return before + describe_field(member);
      },
      std::string{});

  const std::string methods = std::apply(
    [](omni::function_meta auto... m) {
      return (std::string{} + ... + ("  " + describe_function(m)));
    },
    record.public_methods());

  return std::format("{}{} {{\n{}{}}}",
    render_annotation(record.documentation()),
    record.qualified_type_name(),
    fields,
    methods);
}

} // namespace reflected_scope

template <omni::record_meta Root>
void print_reachable_records::operator()(Root) const {
  namespace fn = omni::fn;

  // Follow reflected fields and `mapped_type` dependencies depth-first.
  const auto dfs = //
    []<typename T>(const auto &self, omni::type_t<T>) -> void {
      if constexpr (omni::is_reflected<T>::value) {
        const auto metadata = omni::reflected(omni::type<T>);
        std::println("{}", reflected_scope::describe_record(metadata));
        metadata.public_fields()
          | fn::each([&self]<typename Member>(Member) {
              if constexpr (omni::field_meta<Member>)
                self(self, omni::type<typename Member::type>);
            });
      } else if constexpr (requires { typename T::mapped_type; }) {
        self(self, omni::type<typename T::mapped_type>);
      }
    };

  dfs(dfs, omni::type<typename Root::reflected_type>);
}

template <typename... Values>
void update_fields(Values *...values) {
  namespace fn = omni::fn;
  using namespace std::string_view_literals;

  assert((values && ...));
  omni::reflected_call(
    [](omni::record_binding auto... records) //
      // Explicit result defers scope instantiation until reflection exists.
      -> void {
      // Reflected scope: metadata is available here and in called templates.
      const auto update = [](omni::record_binding auto r) {
        r.public_fields()
          | fn::filter([]<typename Member> {
              using binding = std::remove_cvref_t<Member>;
              if constexpr (omni::field_binding<binding>)
                return binding::is_mutable();

              return false; //< Special function field.
            })
          | fn::each([](omni::field_binding auto field) {
              constexpr std::string_view name = field.name();

              if constexpr ("depth"sv == name)
                field.set_value(815u); //< Writable bit-field.
              else if constexpr ("name"sv == name)
                field.set_value("oceanic"); //< Mutable field of const object.
            });
      };

      (update(records), ...);
    },
    // The caller prepares the values; `reflected_call` only binds them.
    *values...);
}

int main() {
  print_reflection<oceanic::fleet<oceanic::vessel>>();

  oceanic::telemetry telemetry;
  const oceanic::vessel vessel;

  std::println("before: depth={} sensor={} name={}",
    static_cast<unsigned>(telemetry.depth), telemetry.sensor, vessel.name);

  update_fields(&telemetry, &vessel);

  std::println("after:  depth={} sensor={} name={}",
    static_cast<unsigned>(telemetry.depth), telemetry.sensor, vessel.name);
}
