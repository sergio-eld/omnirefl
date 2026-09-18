#pragma once

#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/extensions/compat.hpp>

#include <ryml.hpp>
#include <ryml_std.hpp>

#include <algorithm>
#include <cassert>
#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

OMNI_REQUIRE_GENERATED_REFLECTION(
  "Serialization requires the generated reflection header "
  "to be included first. With CMake, call omni_reflected_target(<target>).");

namespace omni {
namespace ryml {

/// Describes an input issue without rendering a message or deciding failure.
struct issue {
  enum class code: std::uint8_t {
    unexpected_kind,
    invalid_scalar,
    out_of_range,
    missing_field,
    unknown_field,
    duplicate_field,
    parse_error
  };

  enum class kind: std::uint8_t {
    unknown,
    scalar,
    mapping,
    sequence,
    string,
    boolean,
    integer,
    unsigned_integer,
    number,
    null
  };

  struct path_segment {
    c4::csubstr field; ///< Name borrowed from metadata or the retained tree.
    compat::optional<std::size_t> index; ///< Sequence position; absent for a field.
  };

  // Group the byte-sized kinds; keep rendering data before source positions.
  code reason;
  kind expected;
  kind actual;

  // Borrow the decoded scalar from the retained tree; render only on request.
  c4::csubstr value;

  // TODO: Revisit path storage if C++11 constant evaluation becomes necessary.
  std::vector<path_segment> path;

  // Node and field views borrow the tree retained by diagnostics.
  // Missing fields may have no corresponding node.
  ::ryml::id_type node_id = ::ryml::NONE;

  // Source positions are unavailable from a standalone ryml::Tree.
  // TODO: Accept retained source-location metadata when mapping.
  std::size_t offset = std::string::npos;
  std::size_t line = std::string::npos;
  std::size_t column = std::string::npos;

  // TODO: Add constraint metadata and renderer customization when needed.
};

/**
 * Keeps issue records and an owning tree or borrowed node view for rendering.
 * Issue node IDs and input string views refer into that tree.
 *
 * When Owning is false, keep the caller's tree alive and unchanged while using
 * diagnostics. Moving or changing it can invalidate node and string views.
 * When true, move this object; copying does not rebind issue views to its copy.
 * The copy constructor is left implicit to keep initialization simple and avoid
 * explicit copy/move special-member declarations.
 */
template <bool Owning>
struct diagnostics {
  // Preserves the call's settings.
  struct {
    // Number of errors traversal may pass before stopping on the next one.
    // Zero stops at the first error; negative call values are stored as zero.
    std::size_t tolerance;

    // Allows returning a value despite field errors, keeping their defaults.
    bool partial;
  } policy;

  // Retain the tree or refer to the caller's subtree, depending on Owning.
  // External string storage referenced by a caller-supplied tree must remain
  // alive and unchanged while using diagnostics, even when Owning is true.
  compat::conditional_t<Owning, ::ryml::Tree, ::ryml::ConstNodeRef> tree;
  std::vector<issue> issues;

  // Owns the parser's text for a parse_error issue; mapping leaves it empty.
  std::string parse_message;

  // True when mapping's error budget requires stopping.
  bool stopped() const {
    return issues.size() > policy.tolerance;
  }
};

/**
 * Holds the deserialized value, if available, and diagnostics even on failure.
 * Diagnostics can hold issue records or a rendered message.
 */
template <typename T,
  typename Diagnostics = omni::ryml::diagnostics</*owning=*/ true>>
struct with_diagnostics {
  // The deserialized value, if available. strategy.partial controls
  // whether it can be returned despite field errors.
  compat::optional<T> value;

  Diagnostics diagnostics;

  /**
   * Consume this result, keeping the optional value and applying f to
   * diagnostics. Calls f whether or not a value is available, preserving
   * partial values. The returned diagnostics have the type returned by f.
   */
  template <typename F>
  auto map_diagnostics(F &&f) && -> with_diagnostics<T,
    compat::decay_t<decltype(compat::invoke(std::forward<F>(f),
      std::move(diagnostics)))>> {
    return {
      /*value=*/std::move(value),
      /*diagnostics=*/compat::invoke(std::forward<F>(f),
        std::move(diagnostics)),
    };
  }
};

// True when mapping returned a value despite field errors.
template <typename T, bool Owning>
bool is_partial(const with_diagnostics<T, diagnostics<Owning>> &r) {
  return r.value && !r.diagnostics.issues.empty();
}

/// Controls error collection and whether to return a value despite field
/// errors.
struct strategy {
  // TODO(high): Detect at compile time whether a model permits unbounded
  // nesting (e.g. a vector of itself); consider a traversal-depth limit in
  // strategy to prevent stack exhaustion from deeply nested or malicious input.

  /// Number of errors traversal may pass before stopping on the next one.
  /// Zero stops at the first error; negative values behave as zero.
  int tolerance;

  /// Permit returning a value despite field errors, along with diagnostics.
  /// Failed, missing, and unvisited fields retain their initialized defaults.
  bool partial;

  /** Return a strategy with the new tolerance and unchanged partial setting.
   *
   * Compatibility builder for code predating C++20 designated initializers.
   */
  constexpr strategy use_tolerance(int t) const {
    return {
      /*tolerance=*/t,
      /*partial=*/partial,
    };
  }

  /** Return a strategy with the new partial setting and unchanged tolerance.
   *
   * Compatibility builder for code predating C++20 designated initializers.
   */
  constexpr strategy allow_partial(bool p) const {
    return {
      /*tolerance=*/tolerance,
      /*partial=*/p,
    };
  }
};

/** Start configuration with the given tolerance and partial=false.
 *
 * Compatibility builder for code predating C++20 designated initializers.
 */
constexpr strategy use_tolerance(int t) {
  return {
    /*tolerance=*/t,
    /*partial=*/false,
  };
}

namespace detail {

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

// A generic callable keeps map_diagnostics(render_diangostics) valid for both
// ownership modes; an overloaded function name cannot deduce its callable type.
struct render_diangostics_t {
  template <bool Owning>
  std::string operator()(const diagnostics<Owning> &d) const {
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
};

/**
 * Render retained issues with RFC 6901 paths, separated by newlines.
 * Display the root as <root>; return an empty string when there are no issues.
 */
constexpr render_diangostics_t render_diangostics{};

namespace detail {

// Each traversal step uses the same diagnostics and its own path.
struct mapping_state {
  omni::ryml::diagnostics</*owning=*/ false> *diagnostics;
  std::vector<issue::path_segment> path;
};

inline issue make_issue(issue::code reason,
  issue::kind expected,
  ::ryml::ConstNodeRef node,
  const std::vector<issue::path_segment> &path) {
  // C++11 cannot aggregate-initialize issue because it has member defaults.
  issue result{};
  result.reason = reason;
  result.path = path;
  result.expected = expected;
  result.actual = node_kind(node);

  if (!node.invalid()) {
    result.node_id = node.id();
    if (node.has_val())
      result.value = node.val();
  }

  return result;
}

template <typename>
struct dependent_false: std::false_type {};

template <bool Condition, typename Operation>
struct case_ {};

template <typename... Case>
struct select;

template <typename Fallback>
struct select<Fallback> {
  using type = Fallback;
};

template <bool Condition, typename Operation, typename... Case>
struct select<case_<Condition, Operation>, Case...> {
  using type = omni::compat::conditional_t<Condition,
    Operation,
    typename select<Case...>::type>;
};

template <typename To>
struct map_fundamental {
  void operator()(::ryml::ConstNodeRef from,
    To *to,
    mapping_state *state) const {
    assert(to && state);

    const auto expected = std::is_same<To, bool>::value
      ? issue::kind::boolean
      : std::is_unsigned<To>::value
        ? issue::kind::unsigned_integer
        : std::is_integral<To>::value
          ? issue::kind::integer
          : issue::kind::number;

    if (!from.has_val() || from.is_container()) {
      state->diagnostics->issues.push_back(
        make_issue(issue::code::unexpected_kind, expected, from, state->path));

      return;
    }

    const auto value = from.val();
    const bool valid = std::is_same<To, bool>::value
      ? !from.is_val_quoted()
          && ("true" == value || "false" == value || "True" == value
            || "False" == value || "TRUE" == value || "FALSE" == value)
      : std::is_unsigned<To>::value
        ? value.is_unsigned_integer()
        : std::is_integral<To>::value
          ? value.is_integer()
          : value.is_real();

    if (!valid) {
      state->diagnostics->issues.push_back(
        make_issue(issue::code::invalid_scalar, expected, from, state->path));

      return;
    }

    To converted{};
    if (!c4::from_chars(value, &converted)) {
      state->diagnostics->issues.push_back(
        make_issue(issue::code::out_of_range, expected, from, state->path));

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
      state->diagnostics->issues.push_back(make_issue(
        issue::code::unexpected_kind, issue::kind::string, from, state->path));

      return;
    }

    *to = to_string(from.val());
  }
};

} // namespace detail

// These templates use metadata supplied by the enclosing reflected_call.
namespace reflected_scope {

template <typename To>
void map_value(::ryml::ConstNodeRef from,
  To *to,
  detail::mapping_state *state);

template <typename To>
with_diagnostics<To, diagnostics</*owning=*/ false>> fold_record(
  ::ryml::ConstNodeRef from,
  with_diagnostics<To, diagnostics</*owning=*/ false>> result,
  std::vector<issue::path_segment> path);

struct map_sequence {
  template <typename To>
  void operator()(::ryml::ConstNodeRef from,
    To *to,
    detail::mapping_state *state) const {
    assert(to && state);

    if (!from.is_seq()) {
      state->diagnostics->issues.push_back(detail::make_issue(
        issue::code::unexpected_kind,
        issue::kind::sequence,
        from,
        state->path));

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
      state->path);

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
  using operation = typename detail::select<
    detail::case_<std::is_fundamental<To>::value, detail::map_fundamental<To>>,
    detail::case_<std::is_same<To, std::string>::value, detail::map_string>,
    detail::case_<omni::traits::is<std::vector, To>(), map_sequence>,
    detail::case_<omni::is_reflected<To>::value, map_record>,
    unsupported>::type;

  operation{}(from, to, state);
}

struct cpp11_lambda_field_named {
  c4::csubstr name;

  template <typename Field>
  bool operator()(Field field) const {
    return name == c4::to_csubstr(field.name());
  }
};

struct cpp11_lambda_require_field {
  ::ryml::ConstNodeRef from;
  detail::mapping_state *state;

  template <typename Field>
  void operator()(Field field) const {
    if (state->diagnostics->stopped())
      return;

    const auto name = c4::to_csubstr(field.name());
    if (!from.find_child(name).invalid())
      return;

    state->path.push_back({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });
    state->diagnostics->issues.push_back(detail::make_issue(
      issue::code::missing_field,
      issue::kind::unknown,
      ::ryml::ConstNodeRef{},
      state->path));
    state->path.pop_back();
  }
};

// C++11 callable for folding unbound field metadata over a borrowed result.
struct deserialize_field {
  ::ryml::ConstNodeRef from;
  const std::vector<issue::path_segment> &path;

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
  std::vector<issue::path_segment> path) {
  detail::mapping_state state{
    /*diagnostics=*/&result.diagnostics,
    /*path=*/std::move(path),
  };

  if (from.invalid() || !from.is_map()) {
    result.diagnostics.issues.push_back(detail::make_issue(
      issue::code::unexpected_kind,
      issue::kind::mapping,
      from,
      state.path));

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
    const bool duplicate = from.find_child(name).id() != child.id();
    if (!duplicate
      && omni::fn::any_of(
        cpp11_lambda_field_named{
          /*name=*/name,
        },
        fields))
      continue;

    state.path.push_back({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });
    result.diagnostics.issues.push_back(detail::make_issue(
      duplicate
        ? issue::code::duplicate_field
        : issue::code::unknown_field,
      issue::kind::unknown,
      child,
      state.path));
    state.path.pop_back();
  }

  fields
    | omni::fn::each(cpp11_lambda_require_field{
        /*from=*/from,
        /*state=*/&state,
      });

  // TODO: Expose these type/category rules through a schema-from-type
  // extension.
  return omni::fn::foldl(
    deserialize_field{
      /*from=*/from,
      /*path=*/state.path,
    },
    std::move(result),
    fields);
}

// C++11 callable establishing the reflected scope for the record fold.
template <typename To>
struct cpp11_lambda_fold_record {
  ::ryml::ConstNodeRef from;
  omni::ryml::strategy strategy;

  template <typename Meta>
  with_diagnostics<To, diagnostics</*owning=*/ false>> operator()(
    omni::record_meta_t<Meta>) const {
    auto result = fold_record(from,
      with_diagnostics<To, diagnostics</*owning=*/ false>>{
        /*value=*/To{},
        /*diagnostics=*/
        {
          /*policy=*/{
            /*tolerance=*/0 >= strategy.tolerance
              ? 0
              : static_cast<std::size_t>(strategy.tolerance),
            /*partial=*/strategy.partial,
          },
          /*tree=*/from,
          /*issues=*/{},
          /*parse_message=*/{},
        },
      },
      {});

    // Keep defaults throughout the fold; apply retention once at the root.
    if (!result.diagnostics.issues.empty() && !strategy.partial)
      result.value = compat::nullopt;

    return result;
  }
};

} // namespace reflected_scope

/** Parse text into an owning tree, returning syntax errors without throwing.
 *
 * ParserOptions are passed directly to ryml. Location tracking, if enabled,
 * ends with this call; the returned tree does not retain the parser.
 */
inline compat::expected<::ryml::Tree, std::string> parse(std::string source,
  ::ryml::ParserOptions options = {}) {
  constexpr int parse_failed = 1;

  struct _error_t {
    std::jmp_buf jump;
    std::string message;
    ::ryml::Location location;
  } error{};

  // ryml documents jump recovery for its non-returning error callbacks.
  // Both the parser and tree can report syntax errors during parsing.
  ::ryml::EventHandlerTree handler{
    ::ryml::Callbacks{}
      .set_user_data(&error)
      .set_error_parse(
        [](c4::csubstr message,
          const ::ryml::ErrorDataParse &data,
          void *user) {
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

/**
 * Map an owning tree or borrowed node view using the stored strategy.
 */
struct map_tree_t {
  omni::ryml::strategy strategy;

  /**
   * Own a copy for an lvalue tree, or transfer a moved tree after traversal.
   */
  template <typename To>
  with_diagnostics<To, diagnostics</*owning=*/ true>> operator()(
    omni::type_t<To> to,
    ::ryml::Tree tree) const {
    auto result = (*this)(to,
      tree.empty() ? ::ryml::ConstNodeRef{} : tree.crootref());

    // Node references point at the tree object; move it after traversal.
    return {
      /*value=*/std::move(result.value),
      /*diagnostics=*/{
        /*policy=*/{
          /*tolerance=*/result.diagnostics.policy.tolerance,
          /*partial=*/result.diagnostics.policy.partial,
        },
        /*tree=*/std::move(tree),
        /*issues=*/std::move(result.diagnostics.issues),
        /*parse_message=*/{},
      },
    };
  }

  /**
   * Borrow the subtree. Keep the caller's tree alive and unchanged while using
   * the returned diagnostics.
   */
  template <typename To>
  with_diagnostics<To, diagnostics</*owning=*/ false>> operator()(
    omni::type_t<To> to,
    ::ryml::ConstNodeRef from) const {
    return omni::reflected_call(
      reflected_scope::cpp11_lambda_fold_record<To>{
        /*from=*/from,
        /*strategy=*/strategy,
      },
      to);
  }
};

/// Map with zero tolerance and partial values disabled.
constexpr map_tree_t map_tree{
  /*strategy=*/use_tolerance(0).allow_partial(false),
};

/**
 * Compose parsing and tree mapping, retaining all issues in diagnostics.
 * Parse errors always return no value; strategy.partial applies to mapping.
 */
struct deserialize_t {
  omni::ryml::strategy strategy;

  // TODO(high): Store ParserOptions in deserialize_t and remove the defaulted
  // tail argument.
  template <typename To>
  with_diagnostics<To> operator()(
    omni::type_t<To> to,
    std::string source,
    ::ryml::ParserOptions options = {}) const {
    if (auto parsed = parse(std::move(source), options)) {
      return map_tree_t{
        /*strategy=*/strategy,
      }(to, std::move(*parsed));
    } else {
      return {
        /*value=*/compat::nullopt,
        /*diagnostics=*/{
          /*policy=*/{
            /*tolerance=*/0 >= strategy.tolerance
              ? 0
              : static_cast<std::size_t>(strategy.tolerance),
            /*partial=*/strategy.partial,
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
      };
    }
  }
};

/// Parse and map with zero tolerance and partial values disabled.
constexpr deserialize_t deserialize{
  /*strategy=*/use_tolerance(0).allow_partial(false),
};

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

    for (const auto &value : from) {
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
    }, binding.public_fields());
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
  using operation = typename detail::select<
    detail::case_<std::is_same<From, bool>::value, detail::write_boolean>,
    detail::case_<std::is_floating_point<From>::value, detail::write_number>,
    detail::case_<std::is_integral<From>::value, detail::write_scalar>,
    detail::case_<std::is_same<From, std::string>::value, detail::write_string>,
    detail::case_<omni::traits::is<std::vector, From>(), write_sequence>,
    detail::case_<omni::is_reflected<From>::value, write_record>,
    unsupported_serialization>::type;

  operation{}(from, to);
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
    }, from.public_fields());

    return tree;
  }
};

} // namespace reflected_scope

/**
 * Serialize a reflected record and its nested fields to YAML or JSON.
 * Strings are quoted; field values are copied into the tree's arena.
 */
struct serialize_t {
  ::ryml::EmitType_e format;

  template <typename From>
  std::string operator()(const From &from) const {
    const auto tree = omni::reflected_call(
      reflected_scope::cpp11_lambda_to_tree{}, from);

    if (::ryml::EMIT_JSON != format)
      return ::ryml::emitrs_yaml<std::string>(tree);

    auto text = ::ryml::emitrs_json<std::string>(tree);
    if (text.end() == std::find_if(text.begin(),
          text.end(),
          [](unsigned char c) { return 0x20 > c; }))
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
};

/**
 * Serialize reflected records to YAML text.
 */
constexpr serialize_t as_yaml{
  /*format=*/::ryml::EMIT_YAML,
};

/**
 * Serialize reflected records to JSON text.
 */
constexpr serialize_t as_json{
  /*format=*/::ryml::EMIT_JSON,
};

} // namespace ryml
} // namespace omni
