#pragma once

#include <c4/format.hpp>
#include <c4/std/string.hpp>

#include <algorithm>
#include <cassert>
#include <csetjmp>
#include <limits>

namespace omni {
namespace ryml {

namespace detail {

// fn::branch preserves the different return types without requiring C++17.
template <typename Strategy, typename T, bool Owning>
deserialization_result<T, Strategy, Owning> make_result(
  with_diagnostics<T, diagnostics<Owning>> result) {
  return fn::branch(
    detail::returns_expected<Strategy>{},
    [&result]() -> compat::expected<T, diagnostics<Owning>> {
      if (result.value)
        return std::move(*result.value);

      return compat::unexpected<diagnostics<Owning>>{
        std::move(result.diagnostics)};
    },
    [&result]() { return std::move(result); });
}

inline std::string to_string(c4::csubstr s) {
  // Empty views may have null data; C++11's string constructor forbids it.
  return s.empty() ? std::string{} : std::string{s.data(), s.size()};
}

inline issue::kind node_kind(::ryml::ConstNodeRef n) {
  if (n.invalid())
    return issue::kind::unknown;
  if (n.is_map())
    return issue::kind::mapping;
  if (n.is_seq())
    return issue::kind::sequence;
  if (n.has_val())
    return n.val_is_null() ? issue::kind::null : issue::kind::scalar;
  return issue::kind::unknown;
}

inline std::string kind_name(issue::kind k) {
  switch (k) {
  case issue::kind::mapping:
    return "object";
  case issue::kind::sequence:
    return "array";
  case issue::kind::string:
    return "string";
  case issue::kind::boolean:
    return "boolean";
  case issue::kind::integer:
    return "integer";
  case issue::kind::unsigned_integer:
    return "unsigned integer";
  case issue::kind::number:
    return "real number";
  case issue::kind::null:
    return "null";
  default:
    return "scalar";
  }
}

struct mapping_policy {
  std::size_t tolerance;
  bool partial;
  bool extra;
};

struct mapping_context {
  const mapping_policy &policy;
  omni::ryml::diagnostics</*owning=*/ false> *diagnostics;
  std::vector<issue::path_segment> path;

  // Recoverable optional/container failures do not consume the error budget.
  bool recoverable;

  bool stopped() const {
    return policy.tolerance < static_cast<std::size_t>(
      std::count_if(diagnostics->issues.begin(),
        diagnostics->issues.end(),
        [](const issue &e) { return !e.warning; }));
  }
};

inline issue make_issue(issue::code reason,
  issue::kind expected,
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

// Delay the unsupported-type assertion until the selected operation is invoked.
template <typename>
struct dependent_false: std::false_type {};

template <typename Value>
struct map_fundamental {
  // C++11 dispatch keeps integer-only overflow checks out of bool/real reads.
  static bool convert(c4::csubstr scalar, Value *value, std::true_type) {
    return c4::from_chars(scalar, c4::fmt::overflow_checked(*value));
  }

  static bool convert(c4::csubstr scalar, Value *value, std::false_type) {
    return c4::from_chars(scalar, value);
  }

  void operator()(::ryml::ConstNodeRef node,
    Value *value,
    mapping_context *context) const {
    assert(value && context);

    const auto expected = std::is_same<bool, Value>::value
      ? issue::kind::boolean
      : std::is_unsigned<Value>::value
        ? issue::kind::unsigned_integer
        : std::is_integral<Value>::value
          ? issue::kind::integer
          : issue::kind::number;

    if (!node.has_val() || node.is_container()) {
      context->diagnostics->issues.push_back(
        make_issue(issue::code::unexpected_kind,
          expected,
          node,
          context->path,
          context->recoverable));

      return;
    }

    const auto scalar = node.val();
    const bool valid = std::is_same<bool, Value>::value
      ? !node.is_val_quoted()
        && ("true" == scalar || "false" == scalar || "True" == scalar
          || "False" == scalar || "TRUE" == scalar || "FALSE" == scalar)
      : std::is_unsigned<Value>::value
        ? scalar.is_unsigned_integer()
        : std::is_integral<Value>::value
          ? scalar.is_integer()
          : scalar.is_real();

    if (!valid) {
      context->diagnostics->issues.push_back(
        make_issue(issue::code::invalid_scalar,
          expected,
          node,
          context->path,
          context->recoverable));

      return;
    }

    Value converted{};
    if (!convert(scalar,
          &converted,
          std::integral_constant<bool,
            std::is_integral<Value>::value
              && !std::is_same<bool, Value>::value>{})) {
      context->diagnostics->issues.push_back(
        make_issue(issue::code::out_of_range,
          expected,
          node,
          context->path,
          context->recoverable));

      return;
    }

    *value = converted;
  }
};

struct map_string {
  void operator()(::ryml::ConstNodeRef node,
    std::string *value,
    mapping_context *context) const {
    assert(value && context);

    if (!node.has_val() || node.is_container()) {
      context->diagnostics->issues.push_back(
        make_issue(issue::code::unexpected_kind,
          issue::kind::string,
          node,
          context->path,
          context->recoverable));

      return;
    }

    *value = to_string(node.val());
  }
};

} // namespace detail

// These templates use metadata supplied by the enclosing reflected_call.
namespace reflected_scope {

template <typename Value>
void map_value(::ryml::ConstNodeRef node,
  Value *value,
  detail::mapping_context *context);

struct map_record {
  template <typename Record>
  void operator()(::ryml::ConstNodeRef node,
    Record *record,
    detail::mapping_context *context) const;
};

struct map_optional {
  template <typename Optional>
  void operator()(::ryml::ConstNodeRef node,
    Optional *optional,
    detail::mapping_context *context) const {
    assert(optional && context);

    if (node.has_val() && node.val_is_null()) {
      optional->reset();
      return;
    }

    // Keep an initialized fallback if any part of the supplied value fails.
    typename Optional::value_type value = *optional
      ? **optional
      : typename Optional::value_type{};
    const auto issues_before = context->diagnostics->issues.size();
    map_value(node, &value, context);
    if (issues_before == context->diagnostics->issues.size())
      *optional = std::move(value);
  }
};

struct map_sequence {
  template <typename Sequence>
  void operator()(::ryml::ConstNodeRef node,
    Sequence *sequence,
    detail::mapping_context *context) const {
    assert(sequence && context);

    if (!node.is_seq()) {
      context->diagnostics->issues.push_back(
        detail::make_issue(issue::code::unexpected_kind,
          issue::kind::sequence,
          node,
          context->path,
          context->recoverable));

      return;
    }

    // Preserve indices, including failed and unvisited elements in partial
    // results.
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
      map_value(child, &value, context);
      (*sequence)[index++] = std::move(value);
      context->path.pop_back();
    }
  }
};

struct unsupported {
  template <typename Value>
  void operator()(::ryml::ConstNodeRef,
    Value *,
    detail::mapping_context *) const {
    static_assert(detail::dependent_false<Value>::value,
      "unsupported deserialization destination type");
  }
};

template <typename Value>
void map_value(::ryml::ConstNodeRef node,
  Value *value,
  detail::mapping_context *context) {
  assert(value && context && context->diagnostics);

  // TODO: [high] Handle reflected enums separately from reflected records.
  // Map a scalar against reflected enumerator names and represent the schema
  // with JSON Schema's enum constraint. Decide how aliases, numeric input,
  // unknown values, and their diagnostics behave.
  const auto map = typename omni::traits::select<
    omni::traits::case_<std::is_fundamental<Value>::value,
      detail::map_fundamental<Value>>,
    omni::traits::case_<std::is_same<std::string, Value>::value,
      detail::map_string>,
    omni::traits::case_<omni::traits::is<compat::optional, Value>(),
      map_optional>,
    omni::traits::case_<omni::traits::is<std::vector, Value>(), map_sequence>,
    omni::traits::case_<omni::is_reflected<Value>::value, map_record>,
    unsupported>::type{};

  map(node, value, context);
}

struct cpp11_lambda_field_named {
  c4::csubstr name;

  template <typename Field>
  bool operator()(Field field) const {
    return name == c4::to_csubstr(field.name());
  }
};

struct cpp11_lambda_recoverable_field_named {
  c4::csubstr name;

  template <typename Field>
  bool operator()(Field field) const {
    return name == c4::to_csubstr(field.name())
      && (omni::traits::is<compat::optional, typename Field::type>()
        || omni::traits::is<std::vector, typename Field::type>());
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

    if (omni::traits::is<compat::optional, typename Field::type>())
      return;

    const auto name = c4::to_csubstr(field.name());
    if (!node.find_child(name).invalid())
      return;

    context->path.push_back({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });
    context->diagnostics->issues.push_back(
      detail::make_issue(issue::code::missing_field,
        issue::kind::unknown,
        ::ryml::ConstNodeRef{},
        context->path,
        context->recoverable
          || (context->policy.partial
            && omni::traits::is<std::vector, typename Field::type>())));
    context->path.pop_back();
  }
};

// Bound fields keep the result in place and reuse diagnostic path storage.
struct map_field {
  ::ryml::ConstNodeRef node;
  detail::mapping_context *context;

  template <typename Field>
  void operator()(Field field) const {
    assert(context && context->diagnostics);

    if (context->stopped())
      return;

    const auto name = c4::to_csubstr(field.name());
    const auto field_node = node.find_child(name);
    if (field_node.invalid())
      return; // Missing fields were already reported by validation.

    // Restore the parent's recovery policy before visiting a sibling field.
    const bool recoverable = context->recoverable;
    context->recoverable = recoverable
      || (context->policy.partial
        && (omni::traits::is<compat::optional, typename Field::type>()
          || omni::traits::is<std::vector, typename Field::type>()));
    context->path.push_back({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });
    // Preserve initialized defaults while supporting bit-field value access.
    typename Field::type value = field.value();
    map_value(field_node, &value, context);
    field.set_value(std::move(value));
    context->path.pop_back();
    context->recoverable = recoverable;
  }
};

template <typename Record>
void map_record::operator()(::ryml::ConstNodeRef node,
  Record *record,
  detail::mapping_context *context) const {
  assert(record && context && context->diagnostics);

  if (node.invalid() || !node.is_map()) {
    context->diagnostics->issues.push_back(
      detail::make_issue(issue::code::unexpected_kind,
        issue::kind::mapping,
        node,
        context->path,
        context->recoverable));

    return;
  }

  const auto fields = omni::reflected(omni::type_t<Record>{}).public_fields();

  // Structural validation precedes field mapping, preserving strict error
  // order.
  for (const auto child : node.children()) {
    if (context->stopped())
      break;

    const auto name = child.key();
    const bool known =
      omni::fn::any_of(cpp11_lambda_field_named{/*name=*/name}, fields);

    if (!known && context->policy.extra)
      continue;

    const bool duplicate = node.find_child(name).id() != child.id();
    if (known && !duplicate)
      continue;

    context->path.push_back({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });
    context->diagnostics->issues.push_back(detail::make_issue(
      duplicate ? issue::code::duplicate_field : issue::code::unknown_field,
      issue::kind::unknown,
      child,
      context->path,
      context->recoverable
        || (context->policy.partial && duplicate
          && omni::fn::any_of(
            cpp11_lambda_recoverable_field_named{/*name=*/name},
            fields))));
    context->path.pop_back();
  }

  fields
    | omni::fn::each(cpp11_lambda_require_field{
      /*node=*/node,
      /*context=*/context,
    });

  // TODO: Expose these type/category rules through a schema-from-type
  // extension.
  const auto binding = omni::meta_for<Record>::bind(*record);
  binding.public_fields()
    | omni::fn::each(map_field{
      /*node=*/node,
      /*context=*/context,
    });
}

template <typename _M>
auto map_node_to_record(::ryml::ConstNodeRef node,
  const detail::mapping_policy &policy,
  omni::record_meta_t<_M>)
  -> with_diagnostics<typename omni::record_meta_t<_M>::reflected_type,
    diagnostics</*owning=*/ false>> {
  using Record = typename omni::record_meta_t<_M>::reflected_type;
  // Construct in optional storage: moving Record{} triggers GCC 15's
  // uninitialized-union warning for models with an empty tl::optional record.
  // TL's emplace() calls its throwing value() accessor; use the factory.
  auto result = with_diagnostics<Record, diagnostics</*owning=*/ false>>{
    /*value=*/compat::make_optional<Record>(),
    /*diagnostics=*/{
      /*policy=*/{
        /*tolerance=*/policy.tolerance,
        /*partial=*/policy.partial,
        /*extra=*/policy.extra,
      },
      /*tree=*/node,
      /*issues=*/{},
      /*parse_message=*/{},
    },
  };
  detail::mapping_context context{
    /*policy=*/policy,
    /*diagnostics=*/&result.diagnostics,
    /*path=*/{},
    /*recoverable=*/false,
  };
  map_record{}(node, &*result.value, &context);

  // Keep initialized defaults during mapping; discard on strict failure.
  if (!result.diagnostics.issues.empty() && !policy.partial)
    result.value = compat::nullopt;

  return result;
}

struct cpp11_lambda_map_record {
  ::ryml::ConstNodeRef node;
  detail::mapping_policy policy;

  template <typename _M>
  auto operator()(omni::record_meta_t<_M> meta) const
    -> decltype(map_node_to_record(node, policy, meta)) {
    return map_node_to_record(node, policy, meta);
  }
};

} // namespace reflected_scope

namespace detail {

struct unsupported_serialization {
  template <typename From>
  void operator()(const From &, ::ryml::NodeRef *) const {
    static_assert(detail::dependent_false<From>::value,
      "unsupported serialization source type");
  }
};

struct write_scalar {
  template <typename From>
  void operator()(const From &from, ::ryml::NodeRef *to) const {
    assert(to);
    to->set_val_serialized(from);
  }
};

struct write_boolean {
  void operator()(bool from, ::ryml::NodeRef *to) const {
    assert(to);
    // c4's plain bool conversion emits 0/1, rather than document booleans.
    to->set_val_serialized(c4::fmt::boolalpha(from));
  }
};

struct write_number {
  template <typename From>
  void operator()(const From &from, ::ryml::NodeRef *to) const {
    assert(to);
    // Preserve enough significant digits to recover the original value.
    to->set_val_serialized(c4::fmt::real(from,
      std::numeric_limits<From>::max_digits10,
      c4::FTOA_FLEX));
  }
};

template <typename From>
struct is_string_source: std::integral_constant<bool,
    omni::traits::is<std::basic_string, From>()
      || omni::traits::is<c4::basic_substring, From>()
#if defined(__cpp_lib_string_view) && 201606L <= __cpp_lib_string_view
      || omni::traits::is<std::basic_string_view, From>()
#endif
    > {};

struct write_string {
  template <typename From>
  void operator()(const From &from, ::ryml::NodeRef *to) const {
    assert(to);
    static_assert(std::is_same<char,
                    compat::remove_cvref_t<typename From::value_type>>::value,
      "serialization requires char-based strings");
    // Copy views into the arena so the returned text never borrows their storage.
    to->set_val_serialized(c4::csubstr{from.data(), from.size()});
    // Strings such as "true", "12", and "null" must remain strings.
    *to |= ::ryml::VAL_DQUO;
  }
};

} // namespace detail

namespace reflected_scope {

template <typename From>
void write_value(const From &from, ::ryml::NodeRef *to);

struct write_sequence {
  template <typename From>
  void operator()(const From &from, ::ryml::NodeRef *to) const {
    assert(to);
    *to |= ::ryml::SEQ;

    // Ad hoc for vector<bool>: its iterator yields a proxy instead of bool.
    for (const typename From::value_type &value : from) {
      auto child = to->append_child();
      write_value(value, &child);
    }
  }
};

struct write_field {
  ::ryml::NodeRef *to;

  template <typename Field>
  void operator()(Field field) const {
    assert(to);
    auto child = to->append_child();
    child.set_key_serialized(field.name());
    write_value(field.value(), &child);
  }
};

struct write_record {
  template <typename From>
  void operator()(const From &from, ::ryml::NodeRef *to) const {
    assert(to);
    *to |= ::ryml::MAP;
    const auto binding = omni::meta_for<From>::bind(from);
    fn::each(write_field{/*to=*/to}, binding.public_fields());
  }
};

// Scalar/container roots are parsed outside reflected_call during instrumentation,
// when is_reflected is incomplete. Query it only after those writers are ruled out.
struct write_reflected {
  template <typename From>
  void operator()(const From &from, ::ryml::NodeRef *to) const {
    const auto write = typename omni::traits::select<
      omni::traits::case_<omni::is_reflected<From>::value, write_record>,
      detail::unsupported_serialization>::type{};
    write(from, to);
  }
};

template <typename From>
void write_value(const From &from, ::ryml::NodeRef *to) {
  const auto write = typename omni::traits::select<
    omni::traits::case_<std::is_same<bool, From>::value, detail::write_boolean>,
    omni::traits::case_<std::is_floating_point<From>::value,
      detail::write_number>,
    omni::traits::case_<std::is_integral<From>::value, detail::write_scalar>,
    omni::traits::case_<detail::is_string_source<From>::value,
      detail::write_string>,
    omni::traits::case_<omni::traits::is<std::vector, From>(), write_sequence>,
    write_reflected>::type{};

  write(from, to);
}

template <typename From>
::ryml::Tree to_tree(const From &from) {
  ::ryml::Tree tree;
  auto root = tree.rootref();
  write_value(from, &root);
  return tree;
}

// reflected_call accepts record/enum roots, not containers or scalar values.
// Peel vectors only for reflection discovery; to_tree still writes the full value.
template <typename From>
struct serialization_root {
  using type = From;
};

template <typename Value, typename Allocator>
struct serialization_root<std::vector<Value, Allocator>>:
    serialization_root<Value> {};

// The metadata argument establishes reflection for record elements, including
// empty containers. Retain the original source to write its complete root shape.
template <typename From>
struct cpp11_lambda_to_tree {
  const From &from;

  template <typename _M>
  ::ryml::Tree operator()(omni::record_meta_t<_M>) const {
    return to_tree(from);
  }
};

template <typename From>
::ryml::Tree reflect_tree(const From &from, std::true_type) {
  return omni::reflected_call(cpp11_lambda_to_tree<From>{from},
    omni::type_t<typename serialization_root<From>::type>{});
}

template <typename From>
::ryml::Tree reflect_tree(const From &from, std::false_type) {
  return to_tree(from);
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

template <typename Strategy>
template <typename Record>
deserialization_result<Record, Strategy, /*owning=*/ true>
  map_tree_t<Strategy>::operator()(omni::type_t<Record> record_type,
    ::ryml::Tree tree) const {
  auto result = (*this)(record_type,
    tree.empty()
      ? ::ryml::ConstNodeRef{}
      : tree.crootref());

  // Node references point at the tree object; move it after traversal.
  // The product always retains diagnostics; expected retains them on error.
  return std::move(result) //
    .transform_error(
      [&tree](diagnostics</*owning=*/ false> d) -> diagnostics</*owning=*/ true> {
        return {
          /*policy=*/{
            /*tolerance=*/d.policy.tolerance,
            /*partial=*/d.policy.partial,
            /*extra=*/d.policy.extra,
          },
          /*tree=*/std::move(tree),
          /*issues=*/std::move(d.issues),
          /*parse_message=*/std::move(d.parse_message),
        };
      });
}

template <typename Strategy>
template <typename Record>
deserialization_result<Record, Strategy, /*owning=*/ false>
  map_tree_t<Strategy>::operator()(omni::type_t<Record> record_type,
    ::ryml::ConstNodeRef node) const {
  return detail::make_result<Strategy>(
    omni::reflected_call(
      reflected_scope::cpp11_lambda_map_record{
        /*node=*/node,
        /*policy=*/{
          /*tolerance=*/0 >= strategy.tolerance
            ? 0
            : static_cast<std::size_t>(strategy.tolerance),
          /*partial=*/static_cast<bool>(strategy.partial),
          /*extra=*/strategy.extra,
        },
      },
      record_type));
}

template <typename Strategy>
template <typename Record>
deserialization_result<Record, Strategy, /*owning=*/ true>
  deserialize_t<Strategy>::operator()(omni::type_t<Record> record_type,
    std::string source) const {
  if (auto parsed = parse(std::move(source))) {
    return map_tree_t<Strategy>{
      /*strategy=*/strategy,
    }(record_type, std::move(*parsed));
  } else {
    return detail::make_result<Strategy>(with_diagnostics<Record>{
      /*value=*/compat::nullopt,
      /*diagnostics=*/{
        /*policy=*/{
          /*tolerance=*/0 >= strategy.tolerance
            ? 0
            : static_cast<std::size_t>(strategy.tolerance),
          /*partial=*/static_cast<bool>(strategy.partial),
          /*extra=*/strategy.extra,
        },
        /*tree=*/::ryml::Tree{
          /*node_capacity=*/0,
          /*arena_capacity=*/0,
        },
        /*issues=*/{
          detail::make_issue(issue::code::parse_error,
            issue::kind::unknown,
            ::ryml::ConstNodeRef{},
            {}),
        },
        /*parse_message=*/std::move(parsed.error()),
      },
    });
  }
}

template <bool Owning>
std::string render_diagnostics_t::operator()(
  const diagnostics<Owning> &d) const {
  std::string message;
  for (const auto &entry : d.issues) {
    if (!message.empty())
      message += '\n';

    if (issue::code::parse_error == entry.reason) {
      message += d.parse_message;
      continue;
    }

    std::string path;
    for (const auto &segment : entry.path) {
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

    message += path.empty() ? std::string{"<root>"} : path;
    message += ": ";
    if (entry.warning)
      message += "warning: ";

    switch (entry.reason) {
    case issue::code::missing_field:
      message += "missing field";
      break;
    case issue::code::unknown_field:
      message += "unknown field";
      break;
    case issue::code::duplicate_field:
      message += "duplicate field";
      break;
    case issue::code::unexpected_kind:
      message += "expected " + detail::kind_name(entry.expected) + ", found "
        + detail::kind_name(entry.actual);
      break;
    default:
      message += '"' + detail::to_string(entry.value) + "\" ";
      message += issue::code::out_of_range == entry.reason
        ? "could not be converted to the destination scalar"
        : std::string{"is not "}
          + (issue::kind::integer == entry.expected
                || issue::kind::unsigned_integer == entry.expected
              ? "an "
              : "a ")
          + detail::kind_name(entry.expected);
      break;
    }
  }

  return message;
}

template <typename From>
std::string serialize_t::operator()(const From &from) const {
  using root_type = typename reflected_scope::serialization_root<From>::type;
  const auto tree = reflected_scope::reflect_tree(from,
    std::integral_constant<bool, !std::is_fundamental<root_type>::value
      && !detail::is_string_source<root_type>::value>{});

  // Avoid a sizing traversal; YAML needs more room for indentation than JSON.
  // Escaping or deeper indentation can still require a resize and retry.
  std::string text;
  text.reserve(tree.arena_size()
    + (::ryml::EMIT_JSON == format ? 8 : 16) * tree.size());
  if (::ryml::EMIT_JSON != format) {
    ::ryml::emitrs_yaml(tree, &text);
    return text;
  }

  ::ryml::emitrs_json(tree, &text);
  if (text.end() == std::find_if(text.begin(), text.end(), [](unsigned char c) {
        return 0x20 > c;
      }))
    return text;

  // Ad hoc: ryml 0.14 leaves some JSON control bytes unescaped. This tree
  // emits compact JSON, so any remaining control bytes belong to strings.
  // TODO: Remove this pass when ryml escapes every JSON control byte.
  std::string escaped;
  escaped.reserve(text.size());
  for (const unsigned char c : text) {
    if (0x20 > c) {
      escaped += "\\u00";
      escaped += "0123456789abcdef"[c >> 4];
      escaped += "0123456789abcdef"[c & 0x0f];
    } else {
      escaped += c;
    }
  }

  return escaped;
}

} // namespace ryml
} // namespace omni
