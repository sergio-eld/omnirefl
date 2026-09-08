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

namespace ocean {
/// `.documentation()` returns this text.
struct vessel { //< Discovered as the mapped type of `fleet<T>::vessels`.
  struct position { //< Nested structs are supported; discovered through `location`.
    double latitude = 0;
    double longitude = 0;
  };

  using coordinates = position;
  coordinates location;
  // Reflected field properties are queryable.
  mutable std::string name = "before"; //< `.is_mutable()` is true.
};

// Primary templates are supported; `.type_name()` returns `"fleet"`.
template <typename T>
struct fleet { //< Root specialization supplied to `reflected_call`.
  // `std::map` is not reflected; its `mapped_type` dependency is discovered.
  std::map<std::string, T> vessels;
};

struct telemetry {
  mutable unsigned depth : 10 = 42; //< Bit-fields remain writable.
  const unsigned sensor = 108; //< `.is_const()` is true.
  // Only public fields are reflected.
  private:
  [[maybe_unused]] unsigned john_cena = 49; //< can't see
};

} // namespace ocean

// Instantiated only within the translation unit's reflected scope.
template <omni::record_meta RecordMeta>
std::string describe(RecordMeta record) {
  namespace fn = omni::fn; //< Functional QoL for tuple-like values.

  return record.public_fields()
    | fn::map([]<omni::field_meta FieldMeta>(FieldMeta field) {
        // `.spelled_type_name()` is available only for fields.
        const auto description =
          std::format("{}:{}", field.name(), field.spelled_type_name());

        // Fundamental and standard-library types are not reflected.
        if constexpr (omni::is_reflected<typename FieldMeta::type>::value)
          return std::format(" [{} -> {}]",
            description,
            omni::meta_for<typename FieldMeta::type>::type_name());

        return std::format(" [{}]", description);
      })
    | fn::foldl(std::plus{}, std::string{record.qualified_type_name()});
}

// `main()` traverses discovered type dependencies depth-first, calling `describe`:
// ocean::fleet<ocean::vessel> -> "ocean::fleet [vessels:map<std::string, T>]"
// ocean::vessel -> "ocean::vessel [location:vessel::coordinates -> vessel::position] [name:string]"
// ocean::vessel::position -> "ocean::vessel::position [latitude:double] [longitude:double]"

// Predicate; instantiate only within the translation unit's reflected scope.
template <omni::field_binding Field>
using mutable_field = std::bool_constant<Field::is_mutable()>;

void print_field_updates(ocean::telemetry telemetry,
  const ocean::vessel vessel) {
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
            std::println("{}", describe(metadata));
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
    omni::type<ocean::fleet<ocean::vessel>>);
  print_field_updates({}, {});
}
