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

constexpr compat::string_view to_string_view(c4::csubstr s) {
  // Keep empty views safe to append through C++11 string's pointer/size API.
  return {s.len ? s.str : "", s.len};
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

OMNI_CPP14_CONSTEXPR inline compat::string_view kind_name(json::value_kind k) {
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
  const config<> &options;
  std::vector<issue::path_segment> path;

  // Diagnostics already returned by earlier steps in the enclosing traversal.
  std::size_t error_count;
  std::size_t warning_count;

  // FIXME: [high] Preserve the builder's compile-time string-ownership policy
  // through the writing interface instead of erasing it into a runtime flag.
  bool borrow_strings;

  bool stopped() const {
    return options.error_policy.error_limit < error_count
      || (options.error_policy.warning_limit
        && *options.error_policy.warning_limit < warning_count);
  }
};

// Local traversal output owns the accumulated diagnostics and limit counters.
// Child contexts are immutable snapshots; no diagnostics escape through them.
struct node_accumulator {
  const mapping_context &context;
  with_diagnostics<compat::monostate, diagnostics> result;
  std::size_t error_count;
  std::size_t warning_count;

  bool stopped() const {
    return this->context.options.error_policy.error_limit < this->error_count
      || (this->context.options.error_policy.warning_limit
        && *this->context.options.error_policy.warning_limit
          < this->warning_count);
  }

  void report(issue entry) {
    this->error_count += !entry.warning;
    this->warning_count += entry.warning;
    if (!entry.warning)
      this->result.value.reset();

    this->result.diagnostics.issues.push_back(std::move(entry));
    if (this->result.diagnostics.issues.back().warning
      && this->context.options.error_policy.warning_limit
      && this->warning_count
        == std::size_t{1} + *this->context.options.error_policy.warning_limit) {
      auto exceeded = this->result.diagnostics.issues.back();
      exceeded.reason = json::issue_code::warning_limit_exceeded;
      exceeded.warning = false;
      this->result.diagnostics.issues.push_back(std::move(exceeded));
      ++this->error_count;
      this->result.value.reset();
    }
  }

  void append(with_diagnostics<compat::monostate, diagnostics> child) {
    if (!child.value)
      this->result.value.reset();

    // A child may already have emitted the limit error. Preserve that report
    // without generating the same warning-limit error again at each parent.
    const bool has_limit_error = std::any_of(child.diagnostics.issues.begin(),
      child.diagnostics.issues.end(),
      [](const issue &entry) {
        return entry.reason == json::issue_code::warning_limit_exceeded;
      });

    for (auto &entry : child.diagnostics.issues) {
      if (has_limit_error) {
        this->error_count += !entry.warning;
        this->warning_count += entry.warning;
        if (!entry.warning)
          this->result.value.reset();

        this->result.diagnostics.issues.push_back(std::move(entry));
      } else {
        this->report(std::move(entry));
      }
    }
    if (this->stopped())
      this->result.value.reset();
  }

  mapping_context next_context() const {
    return {
      /*options=*/this->context.options,
      /*path=*/this->context.path,
      /*error_count=*/this->error_count,
      /*warning_count=*/this->warning_count,
      /*borrow_strings=*/this->context.borrow_strings,
    };
  }

  mapping_context child_context(issue::path_segment segment) const {
    auto child = this->next_context();
    child.path.push_back(segment);
    return child;
  }

  auto finish() && -> with_diagnostics<compat::monostate, diagnostics> {
    return std::move(this->result);
  }
};

inline node_accumulator make_node_accumulator(const mapping_context &context,
  ::ryml::ConstNodeRef) {
  return {
    /*context=*/context,
    /*result=*/{
      /*value=*/compat::monostate{},
      /*diagnostics=*/{
        /*config=*/context.options,
        /*issues=*/{},
        /*parse_error_message=*/{},
      },
    },
    /*error_count=*/context.error_count,
    /*warning_count=*/context.warning_count,
  };
}

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
  }

  return result;
}

// Format validation precedes conversion; failure here means out of range.
template <typename T,
  compat::enable_if_t<std::is_integral<T>::value, int> = 0>
compat::expected<T, json::issue_code> from_chars(c4::csubstr scalar) {
  T value{};
  // C4 accepts a pointer to either the checked wrapper or the original value.
  compat::conditional_t<std::is_same<bool, T>::value,
    T &,
    c4::fmt::overflow_checked_<T>> target{value};

  if (!c4::from_chars(scalar, &target))
    return compat::unexpected<json::issue_code>{json::issue_code::out_of_range};

  return value;
}

template <typename T,
  compat::enable_if_t<!std::is_integral<T>::value, int> = 0>
compat::expected<T, json::issue_code> from_chars(c4::csubstr scalar) {
  T value{};
  if (!c4::from_chars(scalar, &value))
    return compat::unexpected<json::issue_code>{json::issue_code::out_of_range};

  return value;
}

struct write_integer {
  template <typename Value>
  void operator()(const Value &value, ::ryml::NodeRef *node) const {
    assert(node);
    node->set_serialized(value);
  }
};

struct write_boolean {
  void operator()(bool value, ::ryml::NodeRef *node) const {
    assert(node);
    // c4's plain bool conversion emits 0/1, rather than document booleans.
    node->set_serialized(c4::fmt::boolalpha(value));
  }
};

struct write_real {
  template <typename Value>
  void operator()(const Value &value, ::ryml::NodeRef *node) const {
    assert(node);
    // Preserve enough significant digits to recover the original value.
    node->set_serialized(c4::fmt::real(value,
      std::numeric_limits<Value>::max_digits10,
      c4::FTOA_FLEX));
  }
};

} // namespace detail

namespace core {

// Core algorithm

template <typename T>
void write_node(const T &value,
  ::ryml::NodeRef node,
  const omni::ryml::detail::mapping_context &context) {
  cp::from_type<T>{}(value, node, context);
}

template <typename T>
auto read_node(::ryml::ConstNodeRef node,
  T *into,
  const omni::ryml::detail::mapping_context &context)
  -> with_diagnostics<compat::monostate, diagnostics> {
  auto result = omni::ryml::detail::make_node_accumulator(context, node);
  if (context.stopped()) {
    result.result.value.reset();
  } else {
    result.append(omni::ryml::to_type(omni::type_t<T>{})(node, into, context));
  }
  return std::move(result).finish();
}

namespace detail {

template <bool BorrowStrings, typename T>
struct cpp11_lambda_write {
  const T &value;

  template <typename _M, omni::reflected_entity Entity>
  ::ryml::Tree operator()(omni::meta_t<_M, Entity>) const {
    ::ryml::Tree tree;
    const config<> options{};
    const omni::ryml::detail::mapping_context context{
      /*options=*/options,
      /*path=*/{},
      /*error_count=*/0,
      /*warning_count=*/0,
      /*borrow_strings=*/BorrowStrings,
    };

    core::write_node(this->value, tree.rootref(), context);
    return tree;
  }
};

template <typename T>
struct cpp11_lambda_read {
  const ::ryml::ConstNodeRef &node;
  T &into;
  const omni::ryml::detail::mapping_context &context;

  template <typename _M, omni::reflected_entity Entity>
  auto operator()(omni::meta_t<_M, Entity>) const
    -> with_diagnostics<compat::monostate, diagnostics> {
    return core::read_node(this->node,
      &this->into,
      this->context);
  }
};

} // namespace detail

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

namespace detail {

template <typename T,
  typename Discovery = compat::remove_cvref_t<T>,
  compat::enable_if_t<cp::reflect_as_root<Discovery>::value, int> = 0>
auto map_tree_dispatch(::ryml::ConstNodeRef node,
  T *into,
  const omni::ryml::detail::mapping_context &context)
  -> with_diagnostics<compat::monostate, diagnostics> {
  return omni::reflected_call(
    cpp11_lambda_read<T>{
      /*node=*/node,
      /*into=*/ *into,
      /*context=*/context,
    },
    omni::type_t<Discovery>{});
}

template <typename T,
  typename Discovery = compat::remove_cvref_t<T>,
  compat::enable_if_t<!cp::reflect_as_root<Discovery>::value
      && !compat::disjunction<omni::ryml::detail::is_mapped_range<Discovery>,
        traits::is_optional<Discovery>>::value,
    int> = 0>
auto map_tree_dispatch(::ryml::ConstNodeRef node,
  T *into,
  const omni::ryml::detail::mapping_context &context)
  -> with_diagnostics<compat::monostate, diagnostics> {
  return read_node(node, into, context);
}

template <typename T,
  typename Discovery = compat::remove_cvref_t<T>,
  compat::enable_if_t<!cp::reflect_as_root<Discovery>::value
      && compat::disjunction<omni::ryml::detail::is_mapped_range<Discovery>,
        traits::is_optional<Discovery>>::value,
    int> = 0>
auto map_tree_dispatch(::ryml::ConstNodeRef node,
  T *into,
  const omni::ryml::detail::mapping_context &context)
  -> with_diagnostics<compat::monostate, diagnostics> {
  return map_tree_dispatch<T,
    typename omni::ryml::detail::discovered_range_type<Discovery>::type>(
    node, into, context);
}

} // namespace detail

template <typename T, typename TransformDiagnostics>
with_diagnostics<T> map_tree(const config<TransformDiagnostics> &options,
  omni::type_t<T>, ::ryml::ConstNodeRef node) {
  auto value = compat::make_optional<T>();
  // Node traversal uses policies; the renderer belongs to deserialize.
  const config<> policies{options.error_policy, {}};
  const omni::ryml::detail::mapping_context context{
    /*options=*/policies,
    /*path=*/{},
    /*error_count=*/0,
    /*warning_count=*/0,
    /*borrow_strings=*/false,
  };

  auto mapped = detail::map_tree_dispatch(node, &*value, context);
  return {
    /*value=*/mapped.value ? std::move(value) : compat::nullopt,
    /*diagnostics=*/std::move(mapped.diagnostics),
  };
}

template <typename T, typename TransformDiagnostics>
with_diagnostics<T> map_tree(const config<TransformDiagnostics> &options,
  omni::type_t<T> type, const ::ryml::Tree &tree) {
  return map_tree(options, type, tree.crootref());
}

#if defined(__cpp_concepts) && 201907L <= __cpp_concepts
template <typename Config, typename Record>
  requires (traits::is_specialization_of<config, Config>())
#else
template <typename Config, typename Record,
  compat::enable_if_t<traits::is_specialization_of<config, Config>(), int>>
#endif
auto deserialize(Config &&options, omni::type_t<Record> record_type, std::string source)
  -> with_diagnostics<Record, transformed_diagnostics_type<Config>> {
  auto parsed = omni::ryml::core::parse(std::move(source));
  if (!parsed) {
    const ::ryml::Tree tree{/*node_capacity=*/0, /*arena_capacity=*/0};
    diagnostics report{
      /*config=*/{options.error_policy, {}},
      /*issues=*/{
        omni::ryml::detail::make_issue(json::issue_code::parse_error,
          json::value_kind::unknown, ::ryml::ConstNodeRef{}, /*path=*/{}),
      },
      /*parse_error_message=*/std::move(parsed.error()),
    };

    return {
      /*value=*/compat::nullopt,
      /*diagnostics=*/compat::invoke(
        std::forward<Config>(options).transform_diagnostics, tree,
        std::move(report)),
    };
  }

  auto mapped = map_tree(options, record_type, parsed->crootref());
  return {
    /*value=*/std::move(mapped.value),
    /*diagnostics=*/compat::invoke(
      std::forward<Config>(options).transform_diagnostics,
      static_cast<const ::ryml::Tree &>(*parsed),
      std::move(mapped.diagnostics)),
  };
}

namespace detail {

template <typename T,
  typename Discovery = compat::remove_cvref_t<T>,
  compat::enable_if_t<cp::reflect_as_root<Discovery>::value, int> = 0>
::ryml::Tree as_owning_tree_dispatch(const T &value) {
  return omni::reflected_call(
    cpp11_lambda_write</*BorrowStrings=*/false, T>{/*value=*/value},
    omni::type_t<Discovery>{});
}

template <typename T,
  typename Discovery = compat::remove_cvref_t<T>,
  compat::enable_if_t<!cp::reflect_as_root<Discovery>::value
      && !compat::disjunction<omni::ryml::detail::is_mapped_range<Discovery>,
        traits::is_optional<Discovery>>::value,
    int> = 0>
::ryml::Tree as_owning_tree_dispatch(const T &value) {
  ::ryml::Tree tree;
  const config<> options{};
  const omni::ryml::detail::mapping_context context{
    /*options=*/options,
    /*path=*/{},
    /*error_count=*/0,
    /*warning_count=*/0,
    /*borrow_strings=*/false,
  };

  write_node(value, tree.rootref(), context);
  return tree;
}

template <typename T,
  typename Discovery = compat::remove_cvref_t<T>,
  compat::enable_if_t<!cp::reflect_as_root<Discovery>::value
      && compat::disjunction<omni::ryml::detail::is_mapped_range<Discovery>,
        traits::is_optional<Discovery>>::value,
    int> = 0>
::ryml::Tree as_owning_tree_dispatch(const T &value) {
  return as_owning_tree_dispatch<T,
    typename omni::ryml::detail::discovered_range_type<Discovery>::type>(value);
}

template <typename T,
  typename Discovery = compat::remove_cvref_t<T>,
  compat::enable_if_t<cp::reflect_as_root<Discovery>::value, int> = 0>
::ryml::Tree as_ref_tree_dispatch(T &value) {
  return omni::reflected_call(
    cpp11_lambda_write</*BorrowStrings=*/true, T>{/*value=*/value},
    omni::type_t<Discovery>{});
}

template <typename T,
  typename Discovery = compat::remove_cvref_t<T>,
  compat::enable_if_t<!cp::reflect_as_root<Discovery>::value
      && !compat::disjunction<omni::ryml::detail::is_mapped_range<Discovery>,
        traits::is_optional<Discovery>>::value,
    int> = 0>
::ryml::Tree as_ref_tree_dispatch(T &value) {
  ::ryml::Tree tree;
  const config<> options{};
  const omni::ryml::detail::mapping_context context{
    /*options=*/options,
    /*path=*/{},
    /*error_count=*/0,
    /*warning_count=*/0,
    /*borrow_strings=*/true,
  };

  write_node(value, tree.rootref(), context);
  return tree;
}

template <typename T,
  typename Discovery = compat::remove_cvref_t<T>,
  compat::enable_if_t<!cp::reflect_as_root<Discovery>::value
      && compat::disjunction<omni::ryml::detail::is_mapped_range<Discovery>,
        traits::is_optional<Discovery>>::value,
    int> = 0>
::ryml::Tree as_ref_tree_dispatch(T &value) {
  return as_ref_tree_dispatch<T,
    typename omni::ryml::detail::discovered_range_type<Discovery>::type>(value);
}

} // namespace detail

template <typename T>
::ryml::Tree as_owning_tree(const T &value) {
  return detail::as_owning_tree_dispatch(value);
}

template <typename T>
::ryml::Tree as_ref_tree(T &value) {
  return detail::as_ref_tree_dispatch(value);
}

} // namespace core

namespace mapping {

namespace detail {

template <typename T>
auto is_null(const T &)
  -> compat::enable_if_t<!traits::is_optional<T>::value, bool> {
  return false;
}

template <typename T>
auto is_null(const T &value)
  -> compat::enable_if_t<traits::is_optional<T>::value, bool> {
  return !adapt::has_value(value) || is_null(adapt::get_value(value));
}

} // namespace detail

template <typename T>
auto from_string_like::operator()(const T &value,
  ::ryml::NodeRef node,
  const omni::ryml::detail::mapping_context &context) const
  -> compat::enable_if_t<supports<T>()> {
  const auto &text = adapt::as_string(value);
  using storage_type = decltype(adapt::as_string(value));
  using text_type = compat::remove_cvref_t<storage_type>;
  static_assert(std::is_same<char, typename text_type::value_type>::value,
    "Ryml string mapping requires char text; provide as_string and "
    "assign_string conversions for non-char strings.");

  using view_type = compat::basic_string_view<typename text_type::value_type,
    typename text_type::traits_type>;

  // Owning conversions are temporary: copy them even in a reference tree.
  constexpr bool borrows_source = std::is_lvalue_reference<storage_type>::value
    || std::is_same<text_type, view_type>::value;

  const c4::csubstr scalar{text.data(), text.size()};
  if (context.borrow_strings && borrows_source)
    node.set_val(scalar);
  else
    node.set_serialized(scalar);

  node.set_val_style(::ryml::VAL_DQUO);
}

template <typename T>
auto to_string_like::operator()(::ryml::ConstNodeRef node,
  T *into,
  const omni::ryml::detail::mapping_context &context) const
  -> compat::enable_if_t<supports<T>(),
    with_diagnostics<compat::monostate, diagnostics>> {
  auto result = omni::ryml::detail::make_node_accumulator(context, node);
  if (node.is_container()) {
    result.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      json::value_kind::string,
      node,
      context.path));

    return std::move(result).finish();
  }

  const auto scalar = node.val();
  using storage_type = compat::remove_cvref_t<decltype(adapt::as_string(
    std::declval<const T &>()))>;

  static_assert(std::is_same<char, typename storage_type::value_type>::value,
    "Ryml string mapping requires char text; provide as_string and "
    "assign_string conversions for non-char strings.");

  using view_type = compat::basic_string_view<typename storage_type::value_type,
    typename storage_type::traits_type>;

  adapt::assign_string(into, view_type{scalar.str, scalar.len});
  return std::move(result).finish();
}

template <typename T>
auto from_arithmetic::operator()(const T &value,
  ::ryml::NodeRef node,
  const omni::ryml::detail::mapping_context &) const
  -> compat::enable_if_t<std::is_arithmetic<T>::value> {
  const auto write = typename traits::select<
    traits::case_<std::is_same<bool, T>::value,
      omni::ryml::detail::write_boolean>,
    traits::case_<std::is_floating_point<T>::value,
      omni::ryml::detail::write_real>,
    omni::ryml::detail::write_integer>::type{};

  write(value, &node);
}

template <typename T>
auto to_arithmetic::operator()(::ryml::ConstNodeRef node,
  T *into,
  const omni::ryml::detail::mapping_context &context) const
  -> compat::enable_if_t<supports<T>(),
    with_diagnostics<compat::monostate, diagnostics>> {
  auto result = omni::ryml::detail::make_node_accumulator(context, node);
  const auto expected = std::is_same<bool, T>::value
    ? json::value_kind::boolean
    : std::is_unsigned<T>::value
      ? json::value_kind::unsigned_integer
      : std::is_integral<T>::value
        ? json::value_kind::integer
        : json::value_kind::number;

  if (node.is_container()) {
    result.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      expected,
      node,
      context.path));

    return std::move(result).finish();
  }

  const auto scalar = node.val();
  const bool valid = std::is_same<bool, T>::value
    ? !node.is_val_quoted()
      && ("true" == scalar
        || "false" == scalar
        || "True" == scalar
        || "False" == scalar
        || "TRUE" == scalar
        || "FALSE" == scalar)
    : std::is_unsigned<T>::value
      ? scalar.is_unsigned_integer()
      : std::is_integral<T>::value
        ? scalar.is_integer()
        : scalar.is_real();

  if (!valid) {
    result.report(omni::ryml::detail::make_issue(
      json::issue_code::invalid_scalar,
      expected,
      node,
      context.path));

    return std::move(result).finish();
  }

  const auto converted = omni::ryml::detail::from_chars<T>(scalar);
  if (!converted) {
    result.report(omni::ryml::detail::make_issue(
      converted.error(),
      expected,
      node,
      context.path));

    return std::move(result).finish();
  }

  *into = *converted;
  return std::move(result).finish();
}

template <typename T>
auto from_optional_like::operator()(const T &value,
  ::ryml::NodeRef node,
  const omni::ryml::detail::mapping_context &context) const
  -> compat::enable_if_t<traits::is_optional<T>::value> {
  if (adapt::has_value(value)) {
    core::write_node(adapt::get_value(value), node, context);
  } else {
    node.set_val("null");
    node.set_val_style(::ryml::VAL_PLAIN);
  }
}

template <typename T>
auto to_optional_like::operator()(::ryml::ConstNodeRef node,
  T *into,
  const omni::ryml::detail::mapping_context &context) const
  -> compat::enable_if_t<traits::is_optional<T>::value,
    with_diagnostics<compat::monostate, diagnostics>> {
  if (!node.is_container() && node.val_is_null()
    && (!node.has_val_tag()
      || ::ryml::TAG_NULL == ::ryml::to_tag(node.val_tag()))) {
    adapt::reset(into);
    return omni::ryml::detail::make_node_accumulator(context, node).finish();
  }

  typename T::value_type value{};
  auto result = core::read_node(node, &value, context);
  if (result.value)
    adapt::emplace(into, std::move(value));

  return result;
}

namespace detail {

template <typename T>
void write_range(const T &value, ::ryml::NodeRef node,
  const omni::ryml::detail::mapping_context &context, std::false_type) {
  using element_type = compat::range_value_t<T>;
  node.set_seq();
  // Separate iterator/sentinel declarations also support non-common ranges
  // before C++17 permitted them in range-based for loops.
  using std::begin;
  using std::end;
  auto current = begin(value);
  const auto last = end(value);
  for (; current != last; ++current) {
    const element_type &element = *current;
    if (!is_null(element))
      core::write_node(element, node.append_child(), context);
  }
}

template <typename T>
void write_range(const T &value, ::ryml::NodeRef node,
  const omni::ryml::detail::mapping_context &context, std::true_type) {
  static_assert(traits::is_string<traits::key_type_t<T>>::value,
    "from_range requires string-adaptable keys for JSON/YAML objects");

  node.set_map();
  using std::begin;
  using std::end;
  auto current = begin(value);
  const auto last = end(value);
  for (; current != last; ++current) {
    const auto &entry = *current;
    const auto &key = adapt::as_string(entry.first);
    using storage_type = decltype(adapt::as_string(entry.first));
    using text_type = compat::remove_cvref_t<storage_type>;
    static_assert(std::is_same<char, typename text_type::value_type>::value,
      "Ryml object keys require char text from the string adapter");

    using view_type = compat::basic_string_view<typename text_type::value_type,
      typename text_type::traits_type>;

    constexpr bool borrows_source =
      std::is_lvalue_reference<storage_type>::value
      || std::is_same<text_type, view_type>::value;

    const c4::csubstr text{key.data(), key.size()};
    auto child = node.append_child();
    // Owning key conversions are temporary, even for a reference tree.
    if (context.borrow_strings && borrows_source)
      child.set_key(text);
    else
      child.set_key_serialized(text);

    child.set_key_style(::ryml::KEY_DQUO);
    core::write_node(entry.second, child, context);
  }
}

} // namespace detail

template <typename T>
auto from_range_t::operator()(const T &value,
  ::ryml::NodeRef node,
  const omni::ryml::detail::mapping_context &context) const
  -> compat::enable_if_t<supports<T>()> {
  using element_type = compat::range_value_t<T>;
  using element_reference = compat::range_reference_t<const T>;

  // Materialized non-scalar elements may own strings borrowed by the tree.
  static_assert(compat::disjunction<std::is_scalar<element_type>,
    compat::conjunction<std::is_lvalue_reference<element_reference>,
      std::is_same<element_type,
        compat::remove_cvref_t<element_reference>>>>::value,
    "Default range serialization requires lvalue references to value_type "
    "for non-scalar C++ elements; specialize cp::from_type for value-producing "
    "or converting-proxy iterators.");

  detail::write_range(value, node, context, traits::is_key_value<T>{});

  // TODO: [medium] Consider supporting generators and temporary-producing
  // iterators by copying materialized elements' strings into tree storage.
}

} // namespace mapping

namespace detail {

struct cpp11_write_member {
  ::ryml::NodeRef &node;
  const omni::ryml::detail::mapping_context &context;

  template <typename Record, typename _F>
  void operator()(omni::data_member_binding_t<Record, _F> field) const {
    auto child = this->node.append_child();
    child.set_key_serialized(field.name());
    core::write_node(field.value(), child, this->context);
  }
};

} // namespace detail

namespace reflected_scope {

template <typename T>
auto from_enum::operator()(T value,
  ::ryml::NodeRef node,
  const omni::ryml::detail::mapping_context &) const
  -> compat::enable_if_t<supports<T>()> {
  const auto enumerators = omni::meta_for<T>::enumerators();
  using enumerator_type = typename decltype(enumerators)::value_type;
  const auto found = std::find_if(enumerators.begin(),
    enumerators.end(),
    [value](const enumerator_type &enumerator) {
      return value == enumerator.first;
    });

  if (found != enumerators.end()) {
    node.set_serialized(c4::to_csubstr(found->second));
    node.set_val_style(::ryml::VAL_DQUO);
  }
}

template <typename T>
auto from_record::operator()(const T &value,
  ::ryml::NodeRef node,
  const omni::ryml::detail::mapping_context &context) const
  -> compat::enable_if_t<omni::is_reflected_record<T>::value> {
  node.set_map();
  const auto binding = omni::meta_for<T>::bind(value);
  binding.accessible_data_members()
    | fn::filter<fn::non_static>()
    | fn::each(omni::ryml::detail::cpp11_write_member{
      /*node=*/node,
      /*context=*/context,
    });
}

} // namespace reflected_scope

inline std::string render_diagnostics_t::operator()(const ::ryml::Tree &tree,
  const diagnostics &d) const {
  std::string message;
  for (const auto &entry : d.issues) {
    if (!message.empty())
      message += '\n';

    if (json::issue_code::parse_error == entry.reason) {
      message += d.parse_error_message;
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

    case json::issue_code::duplicate_element:
      message += "duplicate element";
      break;

    case json::issue_code::unexpected_kind: {
      const auto expected = detail::kind_name(entry.expected);
      const auto actual = detail::kind_name(entry.actual);
      message += "expected ";
      message.append(expected.data(), expected.size());
      message += ", found ";
      message.append(actual.data(), actual.size());
      break;
    }

    default: {
      const auto value = detail::to_string_view(
        entry.node_id != ::ryml::NONE && tree.has_val(entry.node_id)
          ? tree.val(entry.node_id) : c4::csubstr{});

      message += '"';
      message.append(value.data(), value.size());
      message += "\" ";
      if (json::issue_code::out_of_range == entry.reason) {
        message += "could not be converted to the destination scalar";
      } else {
        message += "is not ";
        message += json::value_kind::integer == entry.expected
            || json::value_kind::unsigned_integer == entry.expected
          ? "an " : "a ";

        const auto expected = detail::kind_name(entry.expected);
        message.append(expected.data(), expected.size());
      }

      break;
    }
    }
  }

  return message;
}

template <typename Value>
std::string serialize_t::operator()(const Value &value) const {
  const auto tree = core::as_ref_tree(value);

  // Reserve an initial estimate without a sizing traversal. Borrowed strings,
  // escaping, or deeper YAML indentation may require an emitter resize/retry.
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
