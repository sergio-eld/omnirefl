#pragma once

#include <ryml_std.hpp>

#include <algorithm>
#include <cassert>
#include <csetjmp>
#include <limits>

namespace omni {
namespace ryml {

namespace detail {

// Consume retained values and issues with the result type chosen by Strategy.
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

} // namespace detail

namespace detail {

// Each traversal step uses the same diagnostics and its own path.
struct mapping_state {
  // Borrow the result's diagnostics; traversal appends issues here.
  omni::ryml::diagnostics</*owning=*/ false> *diagnostics;

  // Current field/element path, extended before descending into a child.
  std::vector<issue::path_segment> path;
  // Issues inside an optional/container field inherit its warning policy.
  bool warning;
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

template <typename To>
struct map_fundamental {
  // C++11 dispatch keeps integer-only overflow checks out of bool/real reads.
  static bool convert(c4::csubstr from, To *to, std::true_type) {
    return c4::from_chars(from, c4::fmt::overflow_checked(*to));
  }

  static bool convert(c4::csubstr from, To *to, std::false_type) {
    return c4::from_chars(from, to);
  }

  void
    operator()(::ryml::ConstNodeRef from, To *to, mapping_state *state) const {
    assert(to && state);

    const auto expected = std::is_same<To, bool>::value ? issue::kind::boolean
      : std::is_unsigned<To>::value ? issue::kind::unsigned_integer
      : std::is_integral<To>::value ? issue::kind::integer
                                    : issue::kind::number;

    if (!from.has_val() || from.is_container()) {
      state->diagnostics->issues.push_back(
        make_issue(issue::code::unexpected_kind,
          expected,
          from,
          state->path,
          state->warning));

      return;
    }

    const auto value = from.val();
    const bool valid = std::is_same<To, bool>::value ? !from.is_val_quoted()
        && ("true" == value || "false" == value || "True" == value
          || "False" == value || "TRUE" == value || "FALSE" == value)
      : std::is_unsigned<To>::value ? value.is_unsigned_integer()
      : std::is_integral<To>::value ? value.is_integer()
                                    : value.is_real();

    if (!valid) {
      state->diagnostics->issues.push_back(
        make_issue(issue::code::invalid_scalar,
          expected,
          from,
          state->path,
          state->warning));

      return;
    }

    To converted{};
    if (!convert(value,
          &converted,
          std::integral_constant < bool,
          std::is_integral<To>::value && !std::is_same<To, bool>::value > {})) {
      state->diagnostics->issues.push_back(make_issue(issue::code::out_of_range,
        expected,
        from,
        state->path,
        state->warning));

      return;
    }

    *to = converted;
  }
};

struct map_string {
  void operator()(::ryml::ConstNodeRef from,
    std::string *to,
    mapping_state *state) const {
    assert(to && state);

    if (!from.has_val() || from.is_container()) {
      state->diagnostics->issues.push_back(
        make_issue(issue::code::unexpected_kind,
          issue::kind::string,
          from,
          state->path,
          state->warning));

      return;
    }

    *to = to_string(from.val());
  }
};

} // namespace detail

// These templates use metadata supplied by the enclosing reflected_call.
namespace reflected_scope {

template <typename To>
void map_value(::ryml::ConstNodeRef from, To *to, detail::mapping_state *state);

template <typename To>
with_diagnostics<To, diagnostics</*owning=*/ false>> fold_record(
  ::ryml::ConstNodeRef from,
  with_diagnostics<To, diagnostics</*owning=*/ false>> result,
  std::vector<issue::path_segment> path,
  bool warning);

struct map_optional {
  template <typename To>
  void operator()(::ryml::ConstNodeRef from,
    To *to,
    detail::mapping_state *state) const {
    assert(to && state);

    if (from.has_val() && from.val_is_null()) {
      to->reset();
      return;
    }

    // Keep an initialized fallback if any part of the supplied value fails.
    typename To::value_type value = *to ? **to : typename To::value_type{};
    const auto issues_before = state->diagnostics->issues.size();
    map_value(from, &value, state);
    if (issues_before == state->diagnostics->issues.size())
      *to = std::move(value);
  }
};

struct map_sequence {
  template <typename To>
  void operator()(::ryml::ConstNodeRef from,
    To *to,
    detail::mapping_state *state) const {
    assert(to && state);

    if (!from.is_seq()) {
      state->diagnostics->issues.push_back(
        detail::make_issue(issue::code::unexpected_kind,
          issue::kind::sequence,
          from,
          state->path,
          state->warning));

      return;
    }

    // Preserve indices, including failed and unvisited elements in partial
    // results.
    to->clear();
    to->resize(from.num_children());

    std::size_t index = 0;
    for (const auto child : from.children()) {
      if (state->diagnostics->stopped())
        break;

      state->path.push_back({
        /*field=*/{},
        /*index=*/index,
      });
      // A value temporary also supports vector<bool>'s proxy elements.
      typename To::value_type value{};
      map_value(child, &value, state);
      (*to)[index++] = std::move(value);
      state->path.pop_back();
    }
  }
};

struct map_record {
  template <typename To>
  void operator()(::ryml::ConstNodeRef from,
    To *to,
    detail::mapping_state *state) const {
    assert(to && state);

    auto result = fold_record(from,
      with_diagnostics<To, diagnostics</*owning=*/ false>>{
        /*value=*/std::move(*to),
        /*diagnostics=*/std::move(*state->diagnostics),
      },
      state->path,
      state->warning);

    *to = std::move(*result.value);
    *state->diagnostics = std::move(result.diagnostics);
  }
};

struct unsupported {
  template <typename To>
  void operator()(::ryml::ConstNodeRef, To *, detail::mapping_state *) const {
    // TODO: Define the schema representation of reflected enumerations.
    static_assert(detail::dependent_false<To>::value,
      "unsupported deserialization destination type");
  }
};

template <typename To>
void map_value(::ryml::ConstNodeRef from,
  To *to,
  detail::mapping_state *state) {
  assert(to && state && state->diagnostics);

  const auto map = typename omni::traits::select<
    omni::traits::case_<std::is_fundamental<To>::value,
      detail::map_fundamental<To>>,
    omni::traits::case_<std::is_same<To, std::string>::value,
      detail::map_string>,
    omni::traits::case_<omni::traits::is<compat::optional, To>(), map_optional>,
    omni::traits::case_<omni::traits::is<std::vector, To>(), map_sequence>,
    omni::traits::case_<omni::is_reflected<To>::value, map_record>,
    unsupported>::type{};

  map(from, to, state);
}

struct cpp11_lambda_field_named {
  c4::csubstr name;

  template <typename Field>
  bool operator()(Field field) const {
    return name == c4::to_csubstr(field.name());
  }
};

// C++11 predicate for classifying duplicates by their destination field type.
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
  ::ryml::ConstNodeRef from;
  detail::mapping_state *state;

  template <typename Field>
  void operator()(Field field) const {
    assert(state && state->diagnostics);

    if (state->diagnostics->stopped())
      return;

    if (omni::traits::is<compat::optional, typename Field::type>())
      return;

    const auto name = c4::to_csubstr(field.name());
    if (!from.find_child(name).invalid())
      return;

    state->path.push_back({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });
    state->diagnostics->issues.push_back(
      detail::make_issue(issue::code::missing_field,
        issue::kind::unknown,
        ::ryml::ConstNodeRef{},
        state->path,
        state->warning
          || (state->diagnostics->policy.partial
            && omni::traits::is<std::vector, typename Field::type>())));
    state->path.pop_back();
  }
};

// C++11 callable for folding unbound field metadata over a borrowed result.
struct deserialize_field {
  ::ryml::ConstNodeRef from;
  const std::vector<issue::path_segment> &path;
  bool warning;

  template <typename To, typename Field>
  with_diagnostics<To, diagnostics</*owning=*/ false>> operator()(
    with_diagnostics<To, diagnostics</*owning=*/ false>> result,
    Field field) const {
    if (result.diagnostics.stopped())
      return result;

    const auto name = c4::to_csubstr(field.name());
    const auto node = from.find_child(name);
    if (node.invalid())
      return result; // Missing fields were already reported by validation.

    detail::mapping_state state{
      /*diagnostics=*/&result.diagnostics,
      /*path=*/path,
      /*warning=*/warning
        || (result.diagnostics.policy.partial
          && (omni::traits::is<compat::optional, typename Field::type>()
            || omni::traits::is<std::vector, typename Field::type>())),
    };

    state.path.push_back({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });
    // Preserve initialized defaults while supporting bit-field value access.
    typename Field::type value = field.value(*result.value);
    map_value(node, &value, &state);
    field.set_value(*result.value, std::move(value));

    return result;
  }
};

template <typename To>
with_diagnostics<To, diagnostics</*owning=*/ false>> fold_record(
  ::ryml::ConstNodeRef from,
  with_diagnostics<To, diagnostics</*owning=*/ false>> result,
  std::vector<issue::path_segment> path,
  bool warning) {
  detail::mapping_state state{
    /*diagnostics=*/&result.diagnostics,
    /*path=*/std::move(path),
    /*warning=*/warning,
  };

  if (from.invalid() || !from.is_map()) {
    result.diagnostics.issues.push_back(
      detail::make_issue(issue::code::unexpected_kind,
        issue::kind::mapping,
        from,
        state.path,
        state.warning));

    return result;
  }

  // Unbound metadata stays valid as foldl moves the result between fields.
  const auto fields = omni::reflected(omni::type_t<To>{}).public_fields();

  // Structural validation precedes field mapping, preserving strict error
  // order.
  for (const auto child : from.children()) {
    if (result.diagnostics.stopped())
      break;

    const auto name = child.key();
    const bool known = omni::fn::any_of(cpp11_lambda_field_named{
                                          /*name=*/name,
                                        },
      fields);

    if (!known && result.diagnostics.policy.extra)
      continue;

    const bool duplicate = from.find_child(name).id() != child.id();
    if (known && !duplicate)
      continue;

    state.path.push_back({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });
    result.diagnostics.issues.push_back(detail::make_issue(
      duplicate ? issue::code::duplicate_field : issue::code::unknown_field,
      issue::kind::unknown,
      child,
      state.path,
      state.warning
        || (result.diagnostics.policy.partial && duplicate
          && omni::fn::any_of(cpp11_lambda_recoverable_field_named{
                                /*name=*/name,
                              },
            fields))));
    state.path.pop_back();
  }

  fields
    | omni::fn::each(cpp11_lambda_require_field{
      /*from=*/from,
      /*state=*/&state,
    });

  // TODO: Expose these type/category rules through a schema-from-type
  // extension.
  return omni::fn::foldl(deserialize_field{
                           /*from=*/from,
                           /*path=*/state.path,
                           /*warning=*/state.warning,
                         },
    std::move(result),
    fields);
}

struct mapping_policy {
  int tolerance;
  bool partial;
  bool extra;
};

// C++11 callable establishing the reflected scope for the record fold.
template <typename To>
struct cpp11_lambda_fold_record {
  ::ryml::ConstNodeRef from;
  mapping_policy policy;

  template <typename Meta>
  with_diagnostics<To, diagnostics</*owning=*/ false>> operator()(
    omni::record_meta_t<Meta>) const {
    // Construct in optional storage: moving To{} triggers GCC 15's
    // uninitialized-union warning for models with an empty tl::optional record.
    // TL's emplace() calls its throwing value() accessor; use the factory.
    auto result = fold_record(from,
      with_diagnostics<To, diagnostics</*owning=*/ false>>{
        /*value=*/compat::make_optional<To>(),
        /*diagnostics=*/
        {
          /*policy=*/{
            /*tolerance=*/0 >= policy.tolerance
              ? 0
              : static_cast<std::size_t>(policy.tolerance),
            /*partial=*/policy.partial,
            /*extra=*/policy.extra,
          },
          /*tree=*/from,
          /*issues=*/{},
          /*parse_message=*/{},
        },
      },
      {},
      /*warning=*/false);

    // Keep defaults throughout the fold; apply retention once at the root.
    if (!result.diagnostics.issues.empty() && !policy.partial)
      result.value = compat::nullopt;

    return result;
  }
};

} // namespace reflected_scope

namespace detail {

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

struct write_string {
  void operator()(const std::string &from, ::ryml::NodeRef *to) const {
    assert(to);
    to->set_val_serialized(from);
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

// C++11 visitor writes bound fields without copying their values.
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
    fn::each(write_field{
               /*to=*/to,
             },
      binding.public_fields());
  }
};

struct unsupported_serialization {
  template <typename From>
  void operator()(const From &, ::ryml::NodeRef *) const {
    // TODO: Define the document representation of reflected enumerations.
    static_assert(detail::dependent_false<From>::value,
      "unsupported serialization source type");
  }
};

template <typename From>
void write_value(const From &from, ::ryml::NodeRef *to) {
  const auto write = typename omni::traits::select<
    omni::traits::case_<std::is_same<From, bool>::value, detail::write_boolean>,
    omni::traits::case_<std::is_floating_point<From>::value,
      detail::write_number>,
    omni::traits::case_<std::is_integral<From>::value, detail::write_scalar>,
    omni::traits::case_<std::is_same<From, std::string>::value,
      detail::write_string>,
    omni::traits::case_<omni::traits::is<std::vector, From>(), write_sequence>,
    omni::traits::case_<omni::is_reflected<From>::value, write_record>,
    unsupported_serialization>::type{};

  write(from, to);
}

// C++11 callable establishes reflection before writing the record's fields.
struct cpp11_lambda_to_tree {
  template <typename From>
  ::ryml::Tree operator()(omni::record_binding_t<From> from) const {
    ::ryml::Tree tree;
    auto root = tree.rootref();
    root |= ::ryml::MAP;
    fn::each(write_field{
               /*to=*/&root,
             },
      from.public_fields());

    return tree;
  }
};

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
template <typename To>
deserialization_result<To, Strategy, /*owning=*/ true>
  map_tree_t<Strategy>::operator()(omni::type_t<To> to,
    ::ryml::Tree tree) const {
  auto result = (*this)(to,
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
template <typename To>
deserialization_result<To, Strategy, /*owning=*/ false>
  map_tree_t<Strategy>::operator()(omni::type_t<To> to,
    ::ryml::ConstNodeRef from) const {
  return detail::make_result<Strategy>(
    omni::reflected_call(
      reflected_scope::cpp11_lambda_fold_record<To>{
        /*from=*/from,
        /*policy=*/{
          /*tolerance=*/strategy.tolerance,
          /*partial=*/static_cast<bool>(strategy.partial),
          /*extra=*/strategy.extra,
        },
      },
      to));
}

template <typename Strategy>
template <typename To>
deserialization_result<To, Strategy, /*owning=*/ true>
  deserialize_t<Strategy>::operator()(omni::type_t<To> to,
    std::string source) const {
  if (auto parsed = parse(std::move(source))) {
    return map_tree_t<Strategy>{
      /*strategy=*/strategy,
    }(to, std::move(*parsed));
  } else {
    return detail::make_result<Strategy>(with_diagnostics<To>{
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
  const auto tree =
    omni::reflected_call(reflected_scope::cpp11_lambda_to_tree{}, from);

  if (::ryml::EMIT_JSON != format)
    return ::ryml::emitrs_yaml<std::string>(tree);

  auto text = ::ryml::emitrs_json<std::string>(tree);
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
