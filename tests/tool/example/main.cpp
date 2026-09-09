#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>

#include <format>
#include <functional>
#include <map>
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
};

// Primary templates are supported; `.type_name()` returns `"fleet"` without
// template arguments.
template <typename T>
struct fleet { //< Discovered as an `omni::type_t` argument to `reflected_call`.
  // Records nested inside template records are not supported.
  // struct not_supported {};

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
std::string describe_fields(RecordMeta record) {
  namespace fn = omni::fn; //< Functional QoL for tuple-like values.

  const auto description = record.public_fields()
    | fn::map([]<omni::field_meta FieldMeta>(FieldMeta field) {
        const auto field_description = std::format("  {}: {}", field.name(),
          // Use `.spelled_qualified_type_name()` to preserve namespaces.
          field.spelled_type_name());

        // Fundamental and standard-library types are not reflected, so the
        // metadata query for the actual type name is not available for them.
        if constexpr (omni::is_reflected<typename FieldMeta::type>::value)
          return std::format("{} (resolves to {});\n",
            field_description,
            omni::meta_for<typename FieldMeta::type>::type_name());

        return std::format("{};\n", field_description);
      })
    | fn::foldl(std::plus{},
      std::format("{} {{\n", record.qualified_type_name()));

  return description + "}";
}

/*
 * `main()` traverses discovered type dependencies depth-first and prints:
 * oceanic::fleet {
 *   vessels: map<std::string, T>;
 * }
 * oceanic::vessel {
 *   location: vessel::coordinates (resolves to vessel::position);
 *   name: string;
 * }
 * oceanic::vessel::position {
 *   latitude: double;
 *   longitude: double;
 * }
 */

// `omni::fn::filter<Trait>()` uses `Trait<T>::value`, like
// `std::is_integral<T>`.
// The equivalent C++20 NTTP form for this trait is:
//   omni::fn::filter<[]<omni::field_binding Field>() {
//     return Field::is_mutable();
//   }>()
// Instantiate either form only within the reflected scope.
template <omni::field_binding Field>
using mutable_field = std::bool_constant<Field::is_mutable()>;

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
          | fn::filter<mutable_field>()
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
            std::println("{}", describe_fields(metadata));
            metadata.public_fields()
              | fn::each([&self]<omni::field_meta Field>(Field) {
                  self(self, omni::type<typename Field::type>);
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
