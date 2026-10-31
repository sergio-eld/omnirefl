#pragma once

#include <omnirefl/functional.hpp>

#include <c4/format.hpp>
#include <c4/std/string.hpp>

#include <algorithm>
#include <cassert>
// FIXME: [low] Freestanding: parse-error recovery requires setjmp/longjmp,
// which are not guaranteed by a freestanding implementation.
#include <csetjmp>
#include <iterator>
#include <limits>
#include <numeric>
#include <type_traits>

// TODO: Provide an omni::compat::string_view drop-in for the C++11 baseline.
#if 201703L <= OMNI_CPLUSPLUS && defined(__has_include)
#  if __has_include(<string_view>)
#    include <string_view>
#  endif
#endif

namespace omni {
namespace ryml {

namespace detail {

// Delay the unsupported-type assertion until the selected operation is invoked.
template <typename>
struct dependent_false: std::false_type {};

inline std::string to_string(c4::csubstr s) {
  // Empty views may have null data; C++11's string constructor forbids it.
  return s.empty() ? std::string{} : std::string{s.data(), s.size()};
}

inline json::value_kind node_kind(::ryml::ConstNodeRef n) {
  if (n.invalid())
    return json::value_kind::unknown;
  if (n.is_map())
    return json::value_kind::mapping;
  if (n.is_seq())
    return json::value_kind::sequence;
  if (n.has_val())
    return n.val_is_null() ? json::value_kind::null : json::value_kind::scalar;
  return json::value_kind::unknown;
}

inline std::string render_path(const std::vector<issue::path_segment> &segments) {
  std::string path;
  for (const auto &segment : segments) {
    path += '/';
    if (segment.index) {
      path += std::to_string(*segment.index);
      continue;
    }

    for (const char ch : segment.field) {
      if ('~' == ch)
        path += "~0";
      else if ('/' == ch)
        path += "~1";
      else
        path += ch;
    }
  }

  return path;
}

inline std::string kind_name(json::value_kind k) {
  switch (k) {
  case json::value_kind::mapping:
    return "object";
  case json::value_kind::sequence:
    return "array";
  case json::value_kind::string:
    return "string";
  case json::value_kind::boolean:
    return "boolean";
  case json::value_kind::integer:
    return "integer";
  case json::value_kind::unsigned_integer:
    return "unsigned integer";
  case json::value_kind::number:
    return "real number";
  case json::value_kind::null:
    return "null";
  default:
    return "scalar";
  }
}

struct mapping_context {
  const settings &options;
  omni::ryml::diagnostics</*owning=*/false> *diagnostics;
  std::vector<issue::path_segment> path;

  // Keep stop checks constant-time; counters can exceed the 16-bit limits.
  std::size_t error_count;
  std::size_t warning_count;

  void report(issue entry) {
    error_count += !entry.warning;
    warning_count += entry.warning;
    diagnostics->issues.push_back(std::move(entry));
    if (diagnostics->issues.back().warning && options.error_policy.warning_limit
      && *options.error_policy.warning_limit < warning_count) {
      auto exceeded = diagnostics->issues.back();
      exceeded.reason = json::issue_code::warning_limit_exceeded;
      exceeded.warning = false;
      diagnostics->issues.push_back(std::move(exceeded));
      ++error_count;
    }
  }

  bool stopped() const {
    return options.error_policy.error_limit < error_count
      || (options.error_policy.warning_limit
        && *options.error_policy.warning_limit < warning_count);
  }
};

inline issue make_issue(json::issue_code reason,
  json::value_kind expected,
  ::ryml::ConstNodeRef node,
  const std::vector<issue::path_segment> &path,
  bool warning = false) {
  // C++11 cannot aggregate-initialize issue because it has member defaults.
  issue result{};
  result.reason = reason;
  result.path = path;
  result.expected = expected;
  result.actual = node_kind(node);
  result.warning = warning;

  if (!node.invalid()) {
    result.node_id = node.id();
    if (node.has_val())
      result.value = node.val();
  }

  return result;
}

template <typename Value>
struct map_node_to_fundamental {
  // Overflow checking applies to integers, excluding bool.
  template <typename ScalarValue>
  static auto from_chars(c4::csubstr scalar, ScalarValue *value) ->
    typename std::enable_if<std::is_integral<ScalarValue>::value
        && !std::is_same<bool, ScalarValue>::value,
      bool>::type {
    return c4::from_chars(scalar, c4::fmt::overflow_checked(*value));
  }

  template <typename ScalarValue>
  static auto from_chars(c4::csubstr scalar, ScalarValue *value) ->
    typename std::enable_if<!std::is_integral<ScalarValue>::value
        || std::is_same<bool, ScalarValue>::value,
      bool>::type {
    return c4::from_chars(scalar, value);
  }

  void operator()(::ryml::ConstNodeRef node,
    Value *value,
    mapping_context *context) const {
    assert(value && context);

    constexpr auto expected = std::is_same<bool, Value>::value
      ? json::value_kind::boolean
      : std::is_unsigned<Value>::value ? json::value_kind::unsigned_integer
      : std::is_integral<Value>::value ? json::value_kind::integer
                                       : json::value_kind::number;

    if (node.invalid() || !node.has_val() || node.is_container()) {
      context->report(make_issue(json::issue_code::unexpected_kind,
        expected,
        node,
        context->path));

      return;
    }

    const auto scalar = node.val();
    const bool valid = std::is_same<bool, Value>::value ? !node.is_val_quoted()
        && ("true" == scalar || "false" == scalar || "True" == scalar
          || "False" == scalar || "TRUE" == scalar || "FALSE" == scalar)
      : std::is_unsigned<Value>::value ? scalar.is_unsigned_integer()
      : std::is_integral<Value>::value ? scalar.is_integer()
                                       : scalar.is_real();

    if (!valid) {
      context->report(make_issue(json::issue_code::invalid_scalar,
        expected,
        node,
        context->path));

      return;
    }

    Value converted{};
    if (!from_chars(scalar, &converted)) {
      context->report(make_issue(json::issue_code::out_of_range,
        expected,
        node,
        context->path));

      return;
    }

    *value = converted;
  }
};

struct map_node_to_string {
  void operator()(::ryml::ConstNodeRef node,
    std::string *value,
    mapping_context *context) const {
    assert(value && context);

    if (node.invalid() || !node.has_val() || node.is_container()) {
      context->report(make_issue(json::issue_code::unexpected_kind,
        json::value_kind::string,
        node,
        context->path));

      return;
    }

    *value = to_string(node.val());
  }
};

} // namespace detail

// These templates use metadata supplied by the enclosing reflected_call.
namespace reflected_scope {

template <typename Value>
void map_node(::ryml::ConstNodeRef node,
  Value *value,
  detail::mapping_context *context);

struct map_node_to_optional {
  template <typename Optional>
  void operator()(::ryml::ConstNodeRef node,
    Optional *optional,
    detail::mapping_context *context) const {
    assert(optional && context);

    if (!node.invalid() && node.has_val() && node.val_is_null()) {
      optional->reset();
      return;
    }

    // Keep an initialized fallback if any part of the supplied value fails.
    typename Optional::value_type value =
      *optional ? **optional : typename Optional::value_type{};

    const auto errors_before = context->error_count;
    map_node(node, &value, context);
    if (errors_before == context->error_count)
      *optional = std::move(value);
  }
};

struct map_node_to_sequence {
  template <typename Sequence>
  void operator()(::ryml::ConstNodeRef node,
    Sequence *sequence,
    detail::mapping_context *context) const {
    assert(sequence && context);

    if (node.invalid() || !node.is_seq()) {
      context->report(detail::make_issue(json::issue_code::unexpected_kind,
        json::value_kind::sequence,
        node,
        context->path));

      return;
    }

    // Allocate once; failed mappings never expose this sequence.
    sequence->clear();
    sequence->resize(node.num_children());

    std::size_t index = 0;
    for (const auto child : node.children()) {
      if (context->stopped())
        break;

      context->path.push_back({
        /*field=*/{},
        /*index=*/index,
      });

      // A value temporary also supports vector<bool>'s proxy elements.
      typename Sequence::value_type value{};
      map_node(child, &value, context);
      (*sequence)[index++] = std::move(value);
      context->path.pop_back();
    }
  }
};

struct unsupported_deserialization {
  template <typename Value>
  void
    operator()(::ryml::ConstNodeRef, Value *, detail::mapping_context *) const {
    static_assert(detail::dependent_false<Value>::value,
      "unsupported deserialization destination type");
  }
};

template <typename Value>
::ryml::Tree write_tree(const Value &value);

// C++11 substitutes for generic lambdas and lambdas with captures.
struct cpp11_lambda_field_named {
  c4::csubstr name;

  template <typename Field>
  bool operator()(Field field) const {
    return name == c4::to_csubstr(field.name());
  }
};

struct cpp11_lambda_require_field {
  ::ryml::ConstNodeRef node;
  detail::mapping_context *context;

  template <typename Field>
  void operator()(Field field) const {
    assert(context && context->diagnostics);
    if (context->stopped())
      return;

    const auto recovery = context->options.recovery;
    if (omni::traits::is<compat::optional, typename Field::type>()
      || (recovery
        && (((*recovery & json::recovery_policy::field_defaults)
                == json::recovery_policy::field_defaults
              && Field::has_default_value_access())
          || ((*recovery & json::recovery_policy::default_initializable)
              == json::recovery_policy::default_initializable
            && std::is_default_constructible<typename Field::type>::value))))
      return;

    const auto name = c4::to_csubstr(field.name());
    if (!node.find_child(name).invalid())
      return;

    context->path.push_back({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });
    context->report(detail::make_issue(json::issue_code::missing_field,
      json::value_kind::unknown,
      ::ryml::ConstNodeRef{},
      context->path));
    context->path.pop_back();
  }
};

template <typename Value>
struct cpp11_lambda_map_node {
  ::ryml::ConstNodeRef node;
  Value *value;
  detail::mapping_context *context;

  template <typename _M>
  void operator()(omni::record_meta_t<_M>) const {
    map_node(node, value, context);
  }
};

// The metadata argument establishes reflection even for empty containers.
// Retain the original source to write its complete root shape.
template <typename Value>
struct cpp11_lambda_write_tree {
  const Value &value;

  template <typename _M>
  ::ryml::Tree operator()(omni::record_meta_t<_M>) const {
    return write_tree(value);
  }
};

// SFINAE avoids instantiating T{} for fields without a default constructor.
template <typename Field>
auto initialize_missing_field(Field field) -> typename std::enable_if<
  std::is_default_constructible<typename Field::type>::value>::type {
  field.set_value(typename Field::type{});
}

template <typename Field>
auto initialize_missing_field(Field) -> typename std::enable_if<
  !std::is_default_constructible<typename Field::type>::value>::type {}

// Bound fields keep the result in place and reuse diagnostic path storage.
struct map_node_to_field {
  ::ryml::ConstNodeRef node;
  detail::mapping_context *context;

  template <typename Field>
  void operator()(Field field) const {
    assert(context && context->diagnostics);
    if (context->stopped())
      return;

    const auto name = c4::to_csubstr(field.name());
    const auto children = node.children();
    const auto field_node =
      (context->options.error_policy.duplicates & json::duplicate_policy::last)
        == json::duplicate_policy::last
      ? std::accumulate(children.begin(),
          children.end(),
          ::ryml::ConstNodeRef{},
          [name](::ryml::ConstNodeRef selected, ::ryml::ConstNodeRef child) {
            return name == child.key() ? child : selected;
          })
      : node.find_child(name);

    if (field_node.invalid()) {
      // Record{} already applied the member initializer; retain it only when
      // field-default recovery is enabled and reflection exposes that default.
      if (context->options.recovery
        && ((*context->options.recovery & json::recovery_policy::field_defaults)
          == json::recovery_policy::field_defaults)
        && Field::has_default_value_access())
        return;

      if (omni::traits::is<compat::optional, typename Field::type>()
        || (context->options.recovery
          && (*context->options.recovery
               & json::recovery_policy::default_initializable)
            == json::recovery_policy::default_initializable))
        initialize_missing_field(field);
      return;
    }

    context->path.push_back({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });
    // A value temporary also supports bit-fields without reference access.
    typename Field::type value = field.value();
    map_node(field_node, &value, context);
    field.set_value(std::move(value));
    context->path.pop_back();
  }
};

struct map_node_to_record {
  template <typename Record>
  void operator()(::ryml::ConstNodeRef node,
    Record *record,
    detail::mapping_context *context) const {
    assert(record && context && context->diagnostics);

    if (node.invalid() || !node.is_map()) {
      context->report(detail::make_issue(json::issue_code::unexpected_kind,
        json::value_kind::mapping,
        node,
        context->path));

      return;
    }

    // FIXME: [medium] C++11 does not require tuple element-taking constructors
    // to be constexpr. Investigate default-constructing the filtered metadata
    // tuple type instead, since these metadata objects carry no state.
    OMNI_CPP14_CONSTEXPR const auto fields =
      omni::reflected(omni::type_t<Record>{}).accessible_data_members()
      | omni::fn::filter<omni::fn::non_static>();

    // Structural validation precedes field mapping, preserving strict error
    // order.
    for (const auto child : node.children()) {
      if (context->stopped())
        break;

      const auto name = child.key();
      const bool known =
        omni::fn::any_of(cpp11_lambda_field_named{/*name=*/name}, fields);

      const auto unknown = context->options.error_policy.unknown_fields;
      const bool allow_unknown =
        (unknown & json::unknown_policy::allow) == json::unknown_policy::allow;
      const auto duplicates = context->options.error_policy.duplicates;
      const bool allow_duplicate =
        (duplicates
          & (json::duplicate_policy::allow | json::duplicate_policy::last))
        != json::duplicate_policy::reject;
      // Unknown fields are handled by their own policy, including repeats.
      const bool duplicate = known && node.find_child(name).id() != child.id();
      if (known && !duplicate)
        continue;

      const bool allowed = duplicate ? allow_duplicate : allow_unknown;
      const bool warn = duplicate
        ? (duplicates & json::duplicate_policy::warn)
          == json::duplicate_policy::warn
        : (unknown & json::unknown_policy::warn) == json::unknown_policy::warn;
      if (allowed && !warn)
        continue;

      context->path.push_back({
        /*field=*/name,
        /*index=*/compat::nullopt,
      });
      context->report(detail::make_issue(
        duplicate ? json::issue_code::duplicate_field
                  : json::issue_code::unknown_field,
        json::value_kind::unknown,
        child,
        context->path,
        allowed));

      context->path.pop_back();
    }

    fields
      | omni::fn::each(cpp11_lambda_require_field{
        /*node=*/node,
        /*context=*/context,
      });

    // TODO: Expose these type/category rules through a schema-from-type
    // extension.
    // accessible_data_members() requires a binding that outlives field
    // traversal.
    const auto binding = omni::meta_for<Record>::bind(*record);
    binding.accessible_data_members() | omni::fn::filter<omni::fn::non_static>()
      | omni::fn::each(map_node_to_field{
        /*node=*/node,
        /*context=*/context,
      });
  }
};

// Query metadata only after scalar and container mappings have been ruled out.
// Their root overloads also run during instrumentation, before metadata exists.
struct map_node_to_reflected {
  template <typename Value>
  void operator()(::ryml::ConstNodeRef node,
    Value *value,
    detail::mapping_context *context) const {
    constexpr auto map = typename omni::traits::select<
      omni::traits::case_<omni::is_reflected<Value>::value, map_node_to_record>,
      unsupported_deserialization>::type{};

    map(node, value, context);
  }
};

template <typename Value>
void map_node(::ryml::ConstNodeRef node,
  Value *value,
  detail::mapping_context *context) {
  assert(value && context && context->diagnostics);

  // TODO: [high] Handle reflected enums separately from reflected records.
  // Map a scalar against reflected enumerator names and represent the schema
  // with JSON Schema's enum constraint. Decide how aliases, numeric input,
  // unknown values, and their diagnostics behave.
  constexpr auto map = typename omni::traits::select<
    omni::traits::case_<std::is_fundamental<Value>::value,
      detail::map_node_to_fundamental<Value>>,
    omni::traits::case_<std::is_same<std::string, Value>::value,
      detail::map_node_to_string>,
    omni::traits::case_<omni::traits::is<compat::optional, Value>(),
      map_node_to_optional>,
    omni::traits::case_<omni::traits::is<std::vector, Value>(),
      map_node_to_sequence>,
    map_node_to_reflected>::type{};

  map(node, value, context);
}

} // namespace reflected_scope

namespace detail {

struct unsupported_serialization {
  template <typename Value>
  void operator()(const Value &, ::ryml::NodeRef *) const {
    static_assert(detail::dependent_false<Value>::value,
      "unsupported serialization source type");
  }
};

struct write_integer {
  template <typename Value>
  void operator()(const Value &value, ::ryml::NodeRef *node) const {
    assert(node);
    node->set_val_serialized(value);
  }
};

struct write_boolean {
  void operator()(bool value, ::ryml::NodeRef *node) const {
    assert(node);
    // c4's plain bool conversion emits 0/1, rather than document booleans.
    node->set_val_serialized(c4::fmt::boolalpha(value));
  }
};

struct write_real {
  template <typename Value>
  void operator()(const Value &value, ::ryml::NodeRef *node) const {
    assert(node);
    // Preserve enough significant digits to recover the original value.
    node->set_val_serialized(c4::fmt::real(value,
      std::numeric_limits<Value>::max_digits10,
      c4::FTOA_FLEX));
  }
};

template <typename Value>
struct is_string_source:
    std::integral_constant<bool,
      omni::traits::is<std::basic_string, Value>()
        || omni::traits::is<c4::basic_substring, Value>()
#if defined(__cpp_lib_string_view)
        // GCC 7 reports 201603; macro presence suffices for basic_string_view.
        || omni::traits::is<std::basic_string_view, Value>()
#endif
      > {
};

struct write_string {
  template <typename Value>
  void operator()(const Value &value, ::ryml::NodeRef *node) const {
    assert(node);
    static_assert(std::is_same<char,
                    compat::remove_cvref_t<typename Value::value_type>>::value,
      "serialization requires char-based strings");

    // Copy views into the arena so the returned text never borrows their
    // storage.
    node->set_val_serialized(c4::csubstr{value.data(), value.size()});
    // Strings such as "true", "12", and "null" must remain strings.
    *node |= ::ryml::VAL_DQUO;
  }
};

} // namespace detail

namespace reflected_scope {

template <typename Value>
void write_value(const Value &value, ::ryml::NodeRef *node);

template <typename Value>
bool is_null(const Value &) { return false; }

template <typename Value>
bool is_null(const compat::optional<Value> &v) { return !v || is_null(*v); }

struct write_optional {
  template <typename Value>
  void operator()(const compat::optional<Value> &value,
    ::ryml::NodeRef *node) const {
    assert(node);
    if (value)
      write_value(*value, node);
    else
      node->set_val("null");
  }
};

struct write_sequence {
  template <typename Value>
  void operator()(const Value &value, ::ryml::NodeRef *node) const {
    assert(node);
    *node |= ::ryml::SEQ;

    // Bind the value type so vector<bool> proxies convert to bool.
    std::for_each(value.begin(),
      value.end(),
      [node](const typename Value::value_type &element) {
        if (!is_null(element)) {
          auto child = node->append_child();
          write_value(element, &child);
        }
      });
  }
};

struct write_field {
  ::ryml::NodeRef *node;

  template <typename Field>
  void operator()(Field field) const {
    assert(node);
    auto child = node->append_child();
    child.set_key_serialized(field.name());
    write_value(field.value(), &child);
  }
};

// TODO: Consider an opt-in strategy for serializing and mapping static members.
struct write_record {
  template <typename Value>
  void operator()(const Value &value, ::ryml::NodeRef *node) const {
    assert(node);
    *node |= ::ryml::MAP;
    // accessible_data_members() requires a binding that outlives field
    // traversal.
    const auto binding = omni::meta_for<Value>::bind(value);
    binding.accessible_data_members() | fn::filter<fn::non_static>()
      | fn::each(write_field{/*node=*/node});
  }
};

// Scalar/container roots are parsed outside reflected_call during
// instrumentation, when is_reflected is incomplete. Query it only after those
// writers are ruled out.
struct write_reflected {
  template <typename Value>
  void operator()(const Value &value, ::ryml::NodeRef *node) const {
    constexpr auto write = typename omni::traits::select<
      omni::traits::case_<omni::is_reflected<Value>::value, write_record>,
      detail::unsupported_serialization>::type{};

    write(value, node);
  }
};

template <typename Value>
void write_value(const Value &value, ::ryml::NodeRef *node) {
  constexpr auto write = typename omni::traits::select<
    omni::traits::case_<std::is_same<bool, Value>::value,
      detail::write_boolean>,
    omni::traits::case_<std::is_floating_point<Value>::value,
      detail::write_real>,
    omni::traits::case_<std::is_integral<Value>::value, detail::write_integer>,
    omni::traits::case_<detail::is_string_source<Value>::value,
      detail::write_string>,
    omni::traits::case_<omni::traits::is<compat::optional, Value>(),
      write_optional>,
    omni::traits::case_<omni::traits::is<std::vector, Value>(), write_sequence>,
    write_reflected>::type{};

  write(value, node);
}

template <typename Value>
::ryml::Tree write_tree(const Value &value) {
  ::ryml::Tree tree;
  auto root = tree.rootref();
  write_value(value, &root);
  return tree;
}

// reflected_call accepts record/enum roots, not containers or scalar values.
// Peel vectors/optionals for reflection discovery. Mapping and writing retain
// the complete document shape, including nested or empty sequences.
template <typename Value>
struct reflection_root {
  using type = Value;
  static constexpr bool needs_reflection = !std::is_fundamental<Value>::value
    && !detail::is_string_source<Value>::value;
};

template <typename Value, typename Allocator>
struct reflection_root<std::vector<Value, Allocator>>:
    reflection_root<Value> {};

template <typename Value>
struct reflection_root<compat::optional<Value>>: reflection_root<Value> {};

// Discover record elements once, including for an empty sequence. Mapping each
// element then uses that reflected scope and one diagnostic context.
template <typename Value>
auto map_root_node(::ryml::ConstNodeRef node,
  Value *value,
  detail::mapping_context *context)
  -> typename std::enable_if<reflection_root<Value>::needs_reflection>::type {
  omni::reflected_call(cpp11_lambda_map_node<Value>{node, value, context},
    omni::type_t<typename reflection_root<Value>::type>{});
}

template <typename Value>
auto map_root_node(::ryml::ConstNodeRef node,
  Value *value,
  detail::mapping_context *context)
  -> typename std::enable_if<!reflection_root<Value>::needs_reflection>::type {
  map_node(node, value, context);
}

// Exclude the unused path from instrumentation: reflected_call rejects scalar
// roots, while record writing requires generated metadata.
template <typename Value>
typename std::enable_if<reflection_root<Value>::needs_reflection,
  ::ryml::Tree>::type
  to_ryml_tree(const Value &value) {
  return omni::reflected_call(cpp11_lambda_write_tree<Value>{value},
    omni::type_t<typename reflection_root<Value>::type>{});
}

template <typename Value>
typename std::enable_if<!reflection_root<Value>::needs_reflection,
  ::ryml::Tree>::type
  to_ryml_tree(const Value &value) {
  return write_tree(value);
}

} // namespace reflected_scope

inline compat::expected<::ryml::Tree, std::string> parse(std::string source,
  ::ryml::ParserOptions options) {
  // Static storage lets the parser callback use the signal without captures.
  static constexpr int parse_failed = 1;

  struct _error_t {
    std::jmp_buf jump;
    std::string message;
    ::ryml::Location location;
  } error{};

  // ryml documents jump recovery for its non-returning error callbacks.
  // Both the parser and tree can report syntax errors during parsing.
  ::ryml::EventHandlerTree handler{
    ::ryml::Callbacks{}.set_user_data(&error).set_error_parse(
      [](c4::csubstr message, const ::ryml::ErrorDataParse &data, void *user) {
        const auto state = static_cast<_error_t *>(user);
        // ryml already formatted the message; copy it before its buffer
        // expires.
        state->message.assign(message.data(), message.size());
        state->location = data.ymlloc;

        // Exit the parser and resume setjmp with this nonzero error signal.
        std::longjmp(state->jump, parse_failed);
      }),
  };

  ::ryml::Tree tree{
    /*node_capacity=*/0,
    /*arena_capacity=*/0,
    /*callbacks=*/handler.callbacks(),
  };

  ::ryml::Parser parser{
    /*handler=*/&handler,
    /*options=*/options,
  };

  // Ad hoc recovery boundary: ryml reports syntax errors through a callback
  // that must not return, rather than a result. Keep setjmp in this lambda
  // so longjmp cannot make the modified outer tree/parser/error indeterminate
  // or skip their destructors.
  // TODO: Replace this boundary if ryml provides a result-returning parse API.
  return compat::invoke( //
    [&error, &tree, &parser, &source]()
      -> compat::expected<::ryml::Tree, std::string> {
      if (parse_failed == setjmp(error.jump))
        return compat::unexpected<std::string>{"line "
          + std::to_string(error.location.line + 1) + ", column "
          + std::to_string(error.location.col + 1) + ": " + error.message};

      ::ryml::parse_in_arena(&parser,
        c4::csubstr{source.data(), source.size()},
        &tree);

      // Do not let the returned tree retain the local recovery state.
      tree.callbacks(::ryml::Callbacks{});
      return std::move(tree);
    });
}

namespace core {

template <typename Record>
with_diagnostics<Record, diagnostics<true>> map_tree(const settings &options,
  omni::type_t<Record> record_type,
  ::ryml::Tree tree) {
  // Node references point at the tree object; move it after traversal.
  return map_tree(options,
    record_type,
    tree.empty() ? ::ryml::ConstNodeRef{} : tree.crootref())
    .transform_error([&tree](diagnostics<false> d) -> diagnostics<true> {
      return {
        /*settings=*/std::move(d.settings),
        /*tree=*/std::move(tree),
        /*issues=*/std::move(d.issues),
        /*parse_message=*/std::move(d.parse_message),
      };
    });
}

template <typename Record>
with_diagnostics<Record, diagnostics<false>> map_tree(const settings &options,
  omni::type_t<Record>,
  ::ryml::ConstNodeRef node) {
  // FIXME: [external] GCC 15 warns about an uninitialized union when moving
  // Record{} with an empty tl::optional member; construct in optional storage.
  // TL's emplace() calls its throwing value() accessor; use the factory.
  auto value = compat::make_optional<Record>();
  diagnostics<false> report{
    /*settings=*/options,
    /*tree=*/node,
    /*issues=*/{},
    /*parse_message=*/{},
  };

  detail::mapping_context context{
    /*options=*/options,
    /*diagnostics=*/&report,
    /*path=*/{},
    /*error_count=*/0,
    /*warning_count=*/0,
  };

  reflected_scope::map_root_node(node, &*value, &context);

  return {
    /*value=*/0 == context.error_count ? std::move(value) : compat::nullopt,
    /*diagnostics=*/std::move(report),
  };
}

template <typename Record>
with_diagnostics<Record, diagnostics<true>> deserialize(const settings &options,
  omni::type_t<Record> record_type,
  std::string source) {
  if (auto parsed = parse(std::move(source))) {
    return map_tree(options, record_type, std::move(*parsed));
  } else {
    return {
      /*value=*/compat::nullopt,
      /*diagnostics=*/
      {
        /*settings=*/options,
        /*tree=*/
        ::ryml::Tree{
          /*node_capacity=*/0,
          /*arena_capacity=*/0,
        },
        /*issues=*/
        {
          detail::make_issue(json::issue_code::parse_error,
            json::value_kind::unknown,
            ::ryml::ConstNodeRef{},
            {}),
        },
        /*parse_message=*/std::move(parsed.error()),
      },
    };
  }
}

} // namespace core

template <bool OwnsTree>
std::string render_diagnostics_t::operator()(
  const diagnostics<OwnsTree> &d) const {
  std::string message;
  for (const auto &entry : d.issues) {
    if (!message.empty())
      message += '\n';

    if (json::issue_code::parse_error == entry.reason) {
      message += d.parse_message;
      continue;
    }

    const auto path = detail::render_path(entry.path);

    message += path.empty() ? std::string{"<root>"} : path;
    message += ": ";
    if (entry.warning)
      message += "warning: ";

    switch (entry.reason) {
    case json::issue_code::warning_limit_exceeded:
      message += "warning limit exceeded";
      break;
    case json::issue_code::missing_field:
      message += "missing field";
      break;
    case json::issue_code::unknown_field:
      message += "unknown field";
      break;
    case json::issue_code::duplicate_field:
      message += "duplicate field";
      break;
    case json::issue_code::unexpected_kind:
      message += "expected " + detail::kind_name(entry.expected) + ", found "
        + detail::kind_name(entry.actual);

      break;
    default:
      message += '"' + detail::to_string(entry.value) + "\" ";
      message += json::issue_code::out_of_range == entry.reason
        ? "could not be converted to the destination scalar"
        : std::string{"is not "}
          + (json::value_kind::integer == entry.expected
                || json::value_kind::unsigned_integer == entry.expected
              ? "an "
              : "a ")
          + detail::kind_name(entry.expected);

      break;
    }
  }

  return message;
}

template <typename Value>
std::string serialize_t::operator()(const Value &value) const {
  const auto tree = reflected_scope::to_ryml_tree(value);

  // Avoid a sizing traversal; YAML needs more room for indentation than JSON.
  // Escaping or deeper indentation can still require a resize and retry.
  std::string text;
  text.reserve(
    tree.arena_size() + (::ryml::EMIT_JSON == format ? 8 : 16) * tree.size());

  if (::ryml::EMIT_JSON != format) {
    ::ryml::emitrs_yaml(tree, &text);
    return text;
  }

  ::ryml::emitrs_json(tree, &text);

  // JSON strings must escape ASCII control characters (0x00-0x1F).
  // This output is compact, so raw tabs/newlines cannot be formatting
  // whitespace.
  // FIXME: [external] RapidYAML leaves some controls unescaped, even in 0.16.0.
  // Remove this pass once upstream escapes the full 0x00-0x1F range.
  // https://github.com/biojppm/rapidyaml/blob/master/src/c4/yml/emitter.def.hpp
  const auto is_control_byte = [](unsigned char c) { return 0x20 > c; };
  const auto controls =
    std::count_if(text.begin(), text.end(), is_control_byte);

  if (0 == controls)
    return text;

  // Subtract the escape's terminator and the input byte being replaced.
  const auto extra = (sizeof(R"(\u0000)") - 2) * controls;
  text.resize(text.size() + extra);

  // Write backwards so expansion cannot overwrite unread input.
  auto output = text.rbegin();
  for (auto input = text.rbegin() + extra; input != text.rend(); ++input) {
    const unsigned char c = *input;
    if (!is_control_byte(c)) {
      *output++ = c;
      continue;
    }

    const char escaped[]{
      '\\',
      'u',
      '0',
      '0',
      "0123456789abcdef"[c >> 4],
      "0123456789abcdef"[c & 0x0f],
    };

    output = std::reverse_copy(std::begin(escaped), std::end(escaped), output);
  }

  return text;
}

} // namespace ryml
} // namespace omni

#if defined(OMNI_TOOL_RUN)
// TU-wide: c4 views use string writers; compatibility wrappers expose their
// contents through value_type/error_type without needing wrapper metadata.
// Ad hoc: skip requests accept types, so int fills the required template
// arguments. Matching selects the primary templates, so every specialization
// is skipped.
OMNI_SKIP_METADATA(c4::csubstr,
  omni::compat::optional<int>,
  omni::compat::expected<int, int>);
#endif
