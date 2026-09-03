#pragma once

#include <omnirefl/functional.hpp>
#include <omnirefl/reflection.hpp>

#include <ryml.hpp>
#include <tl/expected.hpp>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace omni {
namespace ryml {

template <bool Condition, typename Operation>
struct _case {};

template <typename... Case>
struct select;

template <typename Fallback>
struct select<Fallback> {
  using type = Fallback;
};

template <bool Condition, typename Operation, typename... Case>
struct select<_case<Condition, Operation>, Case...> {
  using type = omni::compat::conditional_t<Condition,
    Operation,
    typename select<Case...>::type>;
};

template <typename... Case>
using select_t = typename select<Case...>::type;

template <typename>
struct dependent_false: std::false_type {};

inline std::string to_string(c4::csubstr value) {
  return {value.data(), value.size()};
}

template <typename To>
struct map_fundamental {
  tl::expected<To, std::string> operator()(::ryml::ConstNodeRef node) const {
    namespace fn = omni::fn;
    using node_or_error = tl::expected<::ryml::ConstNodeRef, std::string>;

    // TODO: Add negative-path benchmarks before replacing eager allocated
    // strings with structured diagnostics. Do not stringify arbitrary node
    // values; retain the source span, a static diagnostic, and the complete
    // field path, including array indices. Consider stopping after a
    // configurable number of errors.
    const auto error = [node](c4::csubstr what) -> tl::unexpected<std::string> {
      return tl::make_unexpected(
        '"' + to_string(node.val()) + "\" " + to_string(what));
    };

    return fn::cond( //
      fn::when(std::is_same<To, bool>{},
        [node, error]() -> node_or_error {
          const c4::csubstr value = node.val();
          // Quoted JSON/YAML scalars are strings even when their spelling is
          // boolean.
          return !node.is_val_quoted()
              && ("true" == value || "false" == value || "True" == value
                || "False" == value || "TRUE" == value || "FALSE" == value)
            ? node_or_error{node}
            : node_or_error{error(c4::to_csubstr("is not a boolean"))};
        }),
      fn::when(std::is_unsigned<To>{},
        [node, error]() -> node_or_error {
          return node.val().is_unsigned_integer() //
            ? node_or_error{node}
            : node_or_error{
                error(c4::to_csubstr("is not an unsigned integer"))};
        }),
      fn::when(std::is_integral<To>{},
        [node, error]() -> node_or_error {
          return node.val().is_integer() //
            ? node_or_error{node}
            : node_or_error{error(c4::to_csubstr("is not an integer"))};
        }),
      fn::when(std::is_floating_point<To>{},
        [node, error]() -> node_or_error {
          return node.val().is_real() //
            ? node_or_error{node}
            : node_or_error{error(c4::to_csubstr("is not a real number"))};
        }),
      [node]() -> node_or_error { return node; })
      .and_then( //
        [error](::ryml::ConstNodeRef node) -> tl::expected<To, std::string> {
          To value{};
          if (!c4::from_chars(node.val(), &value))
            return error(c4::to_csubstr(
              "could not be converted to the destination scalar"));

          return value;
        });
  }
};

template <typename To>
struct map_string {
  tl::expected<To, std::string> operator()(
    const ::ryml::ConstNodeRef &from) const {
    if (!from.has_val())
      return tl::make_unexpected(to_string(from.key()) + " is not a string");

    return To{from.val().data(), from.val().size()};
  }
};

// These templates require metadata made available by the enclosing
// `reflected_call`; they are not an independent public reflection entry point.
namespace reflected_scope {

template <typename To>
tl::expected<To, std::string> map_value(const ::ryml::ConstNodeRef &from,
  omni::type_t<To>);

template <typename To>
tl::expected<void, std::string> map_tree(const ::ryml::ConstNodeRef &from,
  omni::record_binding_t<To> to);

template <typename To>
struct map_sequence {
  tl::expected<To, std::string> operator()(
    const ::ryml::ConstNodeRef &from) const {
    if (!from.is_seq())
      return tl::make_unexpected(to_string(from.key()) + " is not an array");

    To result{};
    result.reserve(from.num_children());
    for (const ::ryml::ConstNodeRef &child : from.children()) {
      auto value = map_value(child, omni::type_t<typename To::value_type>{});
      // TODO(strategy): The strict default stops at the first invalid element;
      // an accumulating strategy must retain each failing element's index.
      if (!value)
        return tl::make_unexpected(value.error());

      result.emplace_back(std::move(value).value());
    }

    return result;
  }
};

template <typename To>
struct map_enum {
  tl::expected<To, std::string> operator()(const ::ryml::ConstNodeRef &) const {
    // TODO: Define whether the schema represents reflected enumerations by
    // name or underlying value, then validate the selected enumerator here.
    static_assert(dependent_false<To>::value,
      "reflected-enum tree mapping is not implemented");
    return tl::make_unexpected(std::string{});
  }
};

template <typename To>
struct map_record {
  tl::expected<To, std::string> operator()(
    const ::ryml::ConstNodeRef &from) const {
    To result{};
    return map_tree(from, omni::reflected(result)) //
      .transform([&result]() { return std::move(result); });
  }
};

template <typename To>
struct unsupported {
  tl::expected<To, std::string> operator()(const ::ryml::ConstNodeRef &) const {
    static_assert(dependent_false<To>::value,
      "unsupported tree-mapping destination type");
    return tl::make_unexpected(std::string{});
  }
};

template <typename To>
tl::expected<To, std::string> map_value(const ::ryml::ConstNodeRef &from,
  omni::type_t<To>) {
  // Keep category dispatch explicit and ordered. Overload resolution would
  // distribute the supported schema-to-C++ mappings across SFINAE overloads.
  using operation = select_t< //
    _case<std::is_fundamental<To>::value, map_fundamental<To>>,
    _case<std::is_same<To, std::string>::value, map_string<To>>,
    _case<omni::traits::is<std::vector, To>(), map_sequence<To>>,
    _case<std::is_enum<To>::value, map_enum<To>>,
    _case<omni::is_reflected<To>::value, map_record<To>>,
    unsupported<To>>;

  return omni::compat::invoke(operation{}, from);
}

struct cpp11_lambda_field_named {
  c4::csubstr name;

  template <typename Field>
  bool operator()(Field field) const {
    return name == c4::to_csubstr(field.name());
  }
};

struct cpp11_lambda_require_field {
  template <typename Field>
  tl::expected<::ryml::ConstNodeRef, std::string> operator()(
    tl::expected<::ryml::ConstNodeRef, std::string> validated,
    Field field) const {
    if (!validated)
      return validated;

    if (!validated->find_child(c4::to_csubstr(field.name())).invalid())
      return validated;

    // TODO(strategy): The strict default rejects every missing destination
    // field.
    return tl::make_unexpected(
      "missing field '" + std::string{field.name()} + "'");
  }
};

template <typename Fields>
tl::expected<::ryml::ConstNodeRef, std::string>
  validate_object(const ::ryml::ConstNodeRef &from, const Fields &fields) {
  if (!from.is_map())
    return tl::make_unexpected(to_string(from.key()) + " is not an object");

  for (const ::ryml::ConstNodeRef &child : from.children()) {
    const c4::csubstr name = child.key();
    if (from.find_child(name).id() != child.id())
      return tl::make_unexpected("duplicate field '" + to_string(name) + "'");

    if (omni::fn::any_of(cpp11_lambda_field_named{name}, fields))
      continue;

    // TODO(strategy): The strict default rejects every input field absent from
    // the destination schema.
    return tl::make_unexpected("unknown field '" + to_string(name) + "'");
  }

  // TODO(strategy): Consider field-type validation before traversal. A full
  // recursive pass would walk the tree twice while conversion can still fail.
  return fields
    | omni::fn::foldl(cpp11_lambda_require_field{},
      tl::expected<::ryml::ConstNodeRef, std::string>{from});
}

struct cpp11_lambda_map_into_field {
  ::ryml::ConstNodeRef from;

  template <typename Field>
  tl::expected<void, std::string>
    operator()(tl::expected<void, std::string> mapped, Field field) const {
    if (!mapped)
      return mapped;

    // TODO(strategy): The strict default stops at the first invalid field; an
    // accumulating strategy must retain its full nested path.
    return map_value(from.find_child(c4::to_csubstr(field.name())),
      omni::type_t<typename Field::type>{})
      .transform([field](typename Field::type value) mutable {
        field.set_value(std::move(value));
      });
  }
};

template <typename To>
tl::expected<void, std::string> map_tree(const ::ryml::ConstNodeRef &from,
  omni::record_binding_t<To> to) {
  const auto fields = to.public_fields();

  // TODO: Consider deriving a uniform serialization schema from reflection to
  // reduce the reflected scope to schema construction. Tree validation and
  // traversal could then operate outside it.
  return validate_object(from, fields) //
    .and_then([fields](::ryml::ConstNodeRef from) {
      return fields
        | omni::fn::foldl(cpp11_lambda_map_into_field{from},
          tl::expected<void, std::string>{});
    });
}

struct cpp11_lambda_visit_nodes {
  ::ryml::ConstNodeRef from;

  template <typename To>
  tl::expected<void, std::string> operator()(
    omni::record_binding_t<To> to) const {
    return map_tree(from, to);
  }
};

template <typename To>
tl::expected<To, std::string> map_tree(const ::ryml::ConstNodeRef &from,
  omni::type_t<To>) {
  To value{};
  return omni::reflected_call(cpp11_lambda_visit_nodes{from}, value)
    .transform([&value]() { return std::move(value); });
}

} // namespace reflected_scope

/// Placeholder for parsing, validation, and diagnostic policies.
struct strategy {};

/// Parse owned text and map its tree into a reflected value.
///
/// @details Niebloid form keeps the overload set available to partial
/// application.
struct deserialize_t {
  // TODO: Return `expected<with_diagnostic<To>, diagnostics>` so successful
  // mappings can carry warnings while failures remain diagnostics-only.
  template <typename To>
  tl::expected<To, std::string>
    operator()(strategy, omni::type_t<To> target, std::string source) const {
    // TODO(strategy): Convert parser failures into owned diagnostics without
    // relying on rapidyaml's process-global error callback.
    const ::ryml::Tree tree =
      ::ryml::parse_in_arena(c4::csubstr{source.data(), source.size()});
    return reflected_scope::map_tree(tree.rootref(), target);
  }
};

/// Parse owned text and map its tree into a reflected value.
///
/// @details Niebloid form keeps the overload set available to partial
/// application.
constexpr deserialize_t deserialize{};

} // namespace ryml
} // namespace omni
