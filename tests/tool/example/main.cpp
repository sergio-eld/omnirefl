#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>

#include <format>
#include <functional>
#include <map>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>

// One .cpp; no declaration headers, metadata files, or reflection macros.

namespace oceanic {
/// `.documentation()` returns this text.
struct vessel { //< Discovered as the `mapped_type` of `fleet<T>::vessels`.
  // Nested structs are supported recursively inside non-template records.
  struct position { //< Discovered via the `location` field.
    double latitude = 0;
    double longitude = 0;
  };

  using coordinates = position;

  // Aliases are preserved for field types. For the `location` field,
  // `.spelled_type_name()` returns the `vessel::coordinates` type name;
  // `.type_name()` on its reflected type returns `vessel::position` without
  // the namespace.
  coordinates location;

  // Reflected field properties are queryable.
  mutable std::string name = "before"; //< `.is_mutable()` is true.

  // Public non-static methods are reflected. Overloaded methods are skipped
  // with a warning. `.documentation()` exposes the comment; Doxygen parameter
  // descriptions, such as `@param` or `\param[in]`, are available through
  // parameter metadata.

  /** Check proximity.
   * @param other Other position.
   * @return Whether the positions are near.
   */
  bool is_near(coordinates other) const {
    return location.latitude == other.latitude
      && location.longitude == other.longitude;
  }
};

// A special `function_t` field below exposes this free function.
// `.documentation()` exposes its docs; each `@param` description is in its
// parameter meta, and `@return` is available through `returns()`.

/** Measure a distance.
 * @param scale Distance scale.
 * @return The scaled distance.
 */
double distance(vessel, double scale) {
  return scale;
}

// Primary templates are supported; `.type_name()` returns `"fleet"` without
// template arguments.
template <typename T>
struct fleet { //< Discovered as an `omni::type_t` argument to `reflected_call`.
  // Records nested inside template records are not supported.
  // struct not_supported {};

  // Special field member: `function_t` is reflected only as a record
  // dependency. See "How It Works"; signature types join discovery.
  omni::function_t<distance> measure;

  // `std` types are not reflected, but dependency protocols apply.
  // Here `mapped_type` discovers `T`.
  std::map<std::string, T> vessels;
};

struct telemetry { //< Discovered as a value argument to `reflected_call`.
  mutable unsigned depth : 10 = 42; //< `.set_value()` writes bit-fields.
  const unsigned sensor = 108; //< `.is_const()` is true.

  // Only public fields are reflected.
  private:
  [[maybe_unused]] unsigned john_cena = 49; //< can't see
};

} // namespace oceanic

// Templates may be declared outside the reflected scope and use reflection
// when called from it, but must not be instantiated outside that scope.
template <omni::record_meta RecordMeta>
std::string describe_record(RecordMeta record) {
  namespace fn = omni::fn; //< Functional QoL for tuple-like values.

  const auto describe_function = //
    [](omni::function_meta auto function) {
      const auto parameters = function.parameters()
        | fn::map([](omni::function_param_meta auto parameter) {
            const auto description = std::format("{}{}",
              parameter.spelled_type_name(),
              std::string_view{parameter.name()}.empty()
                ? std::string{}
                : std::format(" {}", parameter.name()));

            return std::string_view{parameter.documentation()}.empty()
              ? description
              : std::format("{} [{}]",
                  description,
                  parameter.documentation());
          })
        | fn::foldl(
          [](std::string before, const std::string &parameter) {
            return before.empty() ? parameter : before + ", " + parameter;
          },
          std::string{});

      return std::format("  {}({}) -> {}; // {}\n",
        function.name(),
        parameters,
        function.returns().spelled_type_name(),
        function.documentation());
    };

  const auto fields = record.public_fields()
    | fn::map([describe_function]<typename Member>(Member member) {
        if constexpr (omni::function_meta<Member>)
          return describe_function(member);
        else {
          static_assert(omni::field_meta<Member>);
          const auto field_description = std::format("  {}: {}", member.name(),
            // Use `.spelled_qualified_type_name()` to preserve namespaces.
            member.spelled_type_name());

          const std::optional default_value = std::invoke(
            [] -> std::optional<std::string> {
              // can:   int value = 8 * 100 + 15;
              // can't: int value = make_value();
              if constexpr (Member::has_default_value_access()
                && std::formattable<typename Member::type, char>)
                return std::optional{
                  std::format(" = {}", Member::default_value())};

              return std::nullopt;
            });

          // Fundamental and standard-library types are not reflected, so the
          // metadata query for the actual type name is unavailable for them.
          if constexpr (omni::is_reflected<typename Member::type>::value)
            return std::format("{} (resolves to {}){};\n",
              field_description,
              omni::meta_for<typename Member::type>::type_name(),
              default_value.value_or(""));

          return std::format("{}{};\n",
            field_description,
            default_value.value_or(""));
        }
      })
    | fn::foldl(std::plus{}, std::string{});

  const auto methods = record.public_methods()
    | fn::map(describe_function)
    | fn::foldl(std::plus{}, std::string{});

  return std::format("{} {{\n{}{}}}",
    record.qualified_type_name(),
    fields,
    methods);
}

/*
`main()` uses `describe_record()` to print discovered records in depth-first order:
```
oceanic::fleet {
  measure(vessel, double scale [Distance scale.]) -> double; // Measure a distance.
  vessels: map<std::string, T>;
}
oceanic::vessel {
  location: vessel::coordinates (resolves to vessel::position);
  name: string = before;
  is_near(vessel::coordinates other [Other position.]) -> bool; // Check proximity.
}
oceanic::vessel::position {
  latitude: double = 0;
  longitude: double = 0;
}
```
*/
void print_field_updates(oceanic::telemetry telemetry,
  const oceanic::vessel vessel) {
  namespace fn = omni::fn; //< Functional QoL for tuple-like values.
  using namespace std::string_view_literals;

  std::println("before: depth={} sensor={} name={}",
    static_cast<unsigned>(telemetry.depth), telemetry.sensor, vessel.name);

  const auto write_fields = [](auto &value) {
    omni::reflected_call(
      [](omni::record_binding auto record) //< Non-owning binding to `value`.
        // Explicit result defers scope instantiation until reflection exists.
        -> void {
        // Reflected scope: metadata is available here and in called templates.
        record.public_fields()
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
                field.set_value("oceanic"); //< Mutable field of a const object.
            });
      },
      value);
  };

  write_fields(telemetry);
  write_fields(vessel);

  std::println("after:  depth={} sensor={} name={}",
    static_cast<unsigned>(telemetry.depth), telemetry.sensor, vessel.name);
}

// For `print_field_updates({}, {})`:
// before: depth=42 sensor=108 name=before
// after:  depth=815 sensor=108 name=oceanic

int main() {
  omni::reflected_call(
    []<omni::record_meta Root>(Root)
      // Explicit result defers scope instantiation until reflection exists.
      -> void {
      // Reflected scope: metadata is available here and in called templates.
      namespace fn = omni::fn; //< Functional QoL for tuple-like values.

      // Follow reflected fields and `mapped_type` dependencies depth-first.
      const auto dfs = //
        []<typename T>(const auto &self, omni::type_t<T>) -> void {
          if constexpr (omni::is_reflected<T>::value) {
            const auto metadata = omni::reflected(omni::type<T>);
            std::println("{}", describe_record(metadata));
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
    },
    omni::type<oceanic::fleet<oceanic::vessel>>);
  print_field_updates({}, {});
}
