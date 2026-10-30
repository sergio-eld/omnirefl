#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>

#include <cassert>
#include <format>
#include <map>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>

// One .cpp; no declaration headers, metadata files, or reflection macros.

/** This example intentionally uses C++23 syntax and library facilities to
 * reduce verbosity. The reflection APIs also support C++11. */

void print_indented(std::string_view text);
std::string format_annotation(std::string_view annotation, unsigned indent = 0);

/** Selected metadata queries used by print_reachable_records below.
 * This (optional) namespace marks helpers that must not be used outside
 * a reflected scope. Instrumentation checks out-of-scope queries
 * (best effort). */
namespace reflected_scope {

// Showcase data-member metadata queries within a reflected scope.
std::string format_qualifiers(omni::data_member_meta auto field) {
  constexpr auto format_bitfield = [](omni::data_member_meta auto field) {
    if constexpr (field.is_bitfield())
      return std::format("bitfield({}) ", field.bit_width());
    else
      return "";
  };

  // Metadata names, type names, and property queries are constexpr.
  return std::format("{}{}{}{}{}",
    field.is_static() ? "static " : "",
    field.is_const() ? "const " : "",
    field.is_mutable() ? "mutable " : "",
    field.is_volatile() ? "volatile " : "",
    format_bitfield(field));
}

std::string format_enum(omni::enum_meta auto e) {
  using namespace std::string_view_literals;

  constexpr auto enumerator_name = [](auto enumerator) {
    const auto &[value, name] = enumerator;
    // Enumerator names are string literals with static storage.
    return std::string_view{name};
  };

  return std::format("[{}]",
    e.enumerators()
      | std::views::transform(enumerator_name)
      | std::views::join_with(", "sv)
      | std::ranges::to<std::string>());
}

// Non-empty documentation includes its separating line break.
std::string format_documentation(auto meta) {
  const std::string_view text = meta.documentation();
  return text.empty() ? "" : std::format("{}\n", format_annotation(text));
}

template <typename ValueMeta>
  requires (omni::data_member_meta<ValueMeta>
    || omni::function_param_meta<ValueMeta>
    || omni::function_return_meta<ValueMeta>)
std::string format_type_name(ValueMeta meta) {
  if constexpr (omni::is_reflected<typename ValueMeta::type>::value)
    return std::format("{} (resolves to {})",
      meta.spelled_type_name(),
      // qualified_type_name() also includes enclosing namespaces.
      omni::meta_for<typename ValueMeta::type>::type_name());
  else
    /** Fall back to the data member's or function parameter's source spelling
     * when its type is not reflected. */
    return meta.spelled_type_name();
}

std::string format_function_param(omni::function_param_meta auto param) {
  const std::string_view name = param.name();
  const std::string_view documentation = param.documentation();

  return std::format("{}{}{}",
    format_type_name(param),
    name.empty() ? "" : std::format(" {}", name),
    documentation.empty() ? "" : std::format(" [{}]", documentation));
}

/** Render an available default value.
 * ```cpp
 * int make_value();
 *
 * struct defaults {
 *   int arithmetic = 8 * 100 + 15; //< default_value() available
 *   int computed = make_value();  //< default skipped with a warning
 * };
 * ``` */
template <omni::data_member_meta Field>
  requires (Field::has_default_value_access())
std::string format_default_value(Field field) {
  return std::format("{}", field.default_value());
}

// Defined below the model to keep formatting glue out of the query examples.
template <omni::data_member_meta Field>
std::string format_field(Field field);

// Defined below the model to keep formatting glue out of the query examples.
std::string format_function(omni::function_meta auto func);

void print_members(omni::record_meta auto record) {
  namespace fn = omni::fn;

  record.accessible_data_members()
    | fn::each([](omni::data_member_meta auto field) {
        /** Special case: `omni::function_t` allows reflecting free functions
         * "bound" as data members. */
        if constexpr (field.has_callable_metadata())
          print_indented(format_function(field.callable_metadata()));
        else
          print_indented(format_field(field));
      });

  record.accessible_member_functions()
    | fn::each([](omni::function_meta auto func) {
        print_indented(format_function(func));
      });
}

} // namespace reflected_scope

/** Print reachable records depth-first using the query helpers above.
 * The traversal is implemented below. */
struct print_reachable_records {
  template <omni::record_meta Root>
  void operator()(Root) const;
};

template <typename T>
void print_reflection() {
  /** `reflected_call` makes metadata for `T` and its type dependencies
   * available inside the visitor's `operator()` and the functions it calls.
   * The visitor can also be a generic lambda with an explicit return type. */
  omni::reflected_call(print_reachable_records{}, omni::type<T>);
}

/** Omnirefl reflects globally accessible, named records (structs, classes, or
 * unions) and enums on demand. */

namespace oceanic {
// Declaration comments are available as reflection metadata.
struct vessel { //< Discovered as the `mapped_type` of `fleet<T>::vessels`.
  // Nested structs are supported recursively inside non-template records.
  struct position { //< Discovered via the `location` data member.
    using degrees = double;

    degrees latitude = 0;
    degrees longitude = 0;
  };

  position location;

  enum class propulsion { sail, motor, hybrid };
  propulsion drive{};

  // Reflected data-member properties are queryable.
  mutable std::string name = "before"; //< `.is_mutable()` is true.

  /** Public static and non-static member functions are reflected.
   * Parameter and return types join dependency discovery.
   * Overloaded member functions are skipped with a warning.
   * Doxygen parameter descriptions, such as `@param` or `\param[in]`, are
   * available through parameter metadata. */

  /** Check proximity.
   * @param other Other position.
   * @return Whether the positions are near. */
  bool is_near(position other) const {
    return location.latitude == other.latitude
      && location.longitude == other.longitude;
  }
};

/** Measure a distance.
 * @param scale Multiplier.
 * @return The scaled distance. */
double distance(vessel, double scale) { return scale; }

// Primary templates are supported; names appear without template arguments.
template <typename T>
struct fleet { //< Discovered as an `omni::type_t` argument to `reflected_call`.
  // Unsupported nested records emit a warning if discovered as dependencies.
  struct not_supported {};

  /** Free functions are reflected only when discovered through a data member
   * using the special `function_t` tag. */
  omni::function_t<distance> measure;

  /** Standard-library types are not reflected, but their dependency protocols
   * still apply. The specialization for `T` is discovered via `mapped_type`. */
  std::map<std::string, T> vessels;
};

struct telemetry { //< Discovered as a value argument to `reflected_call`.
  // Writing bit-fields without UB is supported via `.set_value()`.
  mutable unsigned depth : 10 = 42;
  const unsigned sensor = 108;

  // Only public data members are reflected.
  private:
  [[maybe_unused]] unsigned john_cena = 49; //< can't see
};

} // namespace oceanic

std::string format_annotation(std::string_view annotation, unsigned indent) {
  return annotation
    | std::views::split('\n')
    | std::views::transform([indent](auto line) {
        return std::format("{:>{}}// {}", "", indent, std::string_view{line});
      })
    | std::views::join_with('\n')
    | std::ranges::to<std::string>();
}

void print_indented(std::string_view text) {
  for (auto line : text
      | std::views::split('\n')
      | std::views::filter([](auto line) { return !line.empty(); }))
    std::println("  {}", std::string_view{line});
}

namespace reflected_scope {

template <omni::data_member_meta Field>
std::string format_field(Field field) {
  constexpr auto format_type = [](Field field) {
    if constexpr (std::is_enum_v<typename Field::type>)
      return format_enum(omni::reflected(omni::type<typename Field::type>));
    else
      return format_type_name(field);
  };

  const auto description = std::format("{}{}{}: {}",
    format_documentation(field),
    format_qualifiers(field),
    field.name(),
    format_type(field));

  if constexpr (!std::is_enum_v<typename Field::type>
    && field.has_default_value_access())
    return std::format("{} = {}", description, format_default_value(field));
  else
    return description;
}

std::string format_function(omni::function_meta auto func) {
  namespace fn = omni::fn; //< Functional QoL for tuple-like values.

  constexpr auto format_param = [](omni::function_param_meta auto param) {
    return format_function_param(param);
  };

  constexpr auto join_params = [](std::string before, std::string_view param) {
    before += before.empty() ? "" : ", ";
    before += param;
    return before;
  };

  const std::string params = func.parameters()
    | fn::map(format_param)
    | fn::foldl(join_params, std::string{});

  return std::format("{}{}({}) -> {};",
    format_documentation(func),
    func.name(),
    params,
    format_type_name(func.returns()));
}

void print_record(omni::record_meta auto record) {
  std::println("{}{} {{",
    format_documentation(record),
    record.qualified_type_name());

  print_members(record);
  std::println("}}");
}

} // namespace reflected_scope

template <omni::record_meta Root>
void print_reachable_records::operator()(Root) const {
  namespace fn = omni::fn;

  // Follow reflected data members and `mapped_type` dependencies depth-first.
  const auto dfs = []<typename T>(const auto &self, omni::type_t<T>) -> void {
    if constexpr (omni::is_reflected<T>::value && !std::is_enum_v<T>) {
      const auto metadata = omni::reflected(omni::type<T>);
      reflected_scope::print_record(metadata);
      metadata.accessible_data_members()
        | fn::filter<fn::non_static>()
        | fn::each([&self]<typename Member>(Member) {
            if constexpr (omni::data_member_meta<Member>)
              self(self, omni::type<typename Member::type>);
          });
    } else if constexpr (requires { typename T::mapped_type; }) {
      self(self, omni::type<typename T::mapped_type>);
    }
  };

  dfs(dfs, omni::type<typename Root::reflected_type>);
}

/** Mutating example; its implementation showcases several reflected arguments
 * in one `reflected_call`. */
template <typename... Records>
void update_fields(Records *...values) {
  namespace fn = omni::fn;
  using namespace std::string_view_literals;

  assert((values && ...));
  omni::reflected_call(
    [](omni::record_binding auto... records)
      // Explicit result defers scope instantiation until reflection exists.
      -> void {
      // Reflected scope: metadata is available here and in called templates.
      const auto update = [](omni::record_binding auto r) {
        r.accessible_data_members()
          | fn::filter<fn::non_static>()
          | fn::filter([]<typename Member> {
              return std::remove_cvref_t<Member>::is_mutable();
            })
          | fn::each([](omni::data_member_binding auto field) {
              constexpr std::string_view name = field.name();

              if constexpr ("depth"sv == name)
                field.set_value(815u); //< Writable bit-field.
              else if constexpr ("name"sv == name)
                // Modify a mutable data member of a const object.
                field.set_value("oceanic");
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
