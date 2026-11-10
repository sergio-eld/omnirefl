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
#include <set>
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

inline const std::string &kind_name(json::value_kind k) {
  switch (k) {
  case json::value_kind::mapping: {
    static const std::string name{"object"};
    return name;
  }
  case json::value_kind::sequence: {
    static const std::string name{"array"};
    return name;
  }
  case json::value_kind::string: {
    static const std::string name{"string"};
    return name;
  }
  case json::value_kind::boolean: {
    static const std::string name{"boolean"};
    return name;
  }
  case json::value_kind::integer: {
    static const std::string name{"integer"};
    return name;
  }
  case json::value_kind::unsigned_integer: {
    static const std::string name{"unsigned integer"};
    return name;
  }
  case json::value_kind::number: {
    static const std::string name{"real number"};
    return name;
  }
  case json::value_kind::null: {
    static const std::string name{"null"};
    return name;
  }
  default: {
    static const std::string name{"scalar"};
    return name;
  }
  }
}

struct mapping_context {
  const config &options;
  omni::ryml::diagnostics</*owning=*/false> *diagnostics;
  std::vector<issue::path_segment> path;

  // Keep stop checks constant-time; counters can exceed the 16-bit limits.
  std::size_t error_count;
  std::size_t warning_count;

  // Reference-tree builders borrow source strings; owning builders copy them.
  bool borrow_strings;

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

struct scalar_conversion {
  // Overflow checking applies to integers, excluding bool.
  template <typename ScalarValue>
  static auto from_chars(c4::csubstr scalar, ScalarValue *value)
    -> compat::enable_if_t<std::is_integral<ScalarValue>::value
        && !std::is_same<bool, ScalarValue>::value,
      bool> {
    return c4::from_chars(scalar, c4::fmt::overflow_checked(*value));
  }

  template <typename ScalarValue>
  static auto from_chars(c4::csubstr scalar, ScalarValue *value)
    -> compat::enable_if_t<!std::is_integral<ScalarValue>::value
        || std::is_same<bool, ScalarValue>::value,
      bool> {
    return c4::from_chars(scalar, value);
  }

};

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

template <bool OwnsStrings>
struct write_string {
  template <typename Value>
  void operator()(const Value &value, ::ryml::NodeRef *node) const {
    assert(node);
    static_assert(std::is_same<char,
                    compat::remove_cvref_t<typename Value::value_type>>::value,
      "serialization requires char-based strings");

    const c4::csubstr text{value.data(), value.size()};
    if (OwnsStrings)
      node->set_serialized(text);
    else
      node->set_val(text);
    // Strings such as "true", "12", and "null" must remain strings.
    node->set_val_style(::ryml::VAL_DQUO);
  }
};

} // namespace detail

namespace cp {

template <typename Container, typename Enable>
void emplace<Container, Enable>::operator()(Container *into,
  typename Container::value_type &&value) const {
  into->emplace_back(std::move(value));
}

} // namespace cp

namespace core {

// Core algorithm

template <typename T>
void write_node(const T &value,
  ::ryml::NodeRef node,
  omni::ryml::detail::mapping_context &diagnostics) {
  if (!diagnostics.stopped())
    cp::from_type<T>{}(value, node, diagnostics);
}

template <typename T>
bool read_node(::ryml::ConstNodeRef node,
  T *into,
  omni::ryml::detail::mapping_context &diagnostics) {
  if (diagnostics.stopped())
    return false;

  const bool success = cp::to_type<T>{}(node, into, diagnostics);
  return success && !diagnostics.stopped();
}

namespace detail {

// FIXME: [high] Generalize container discovery without concrete-container
// routing or inferring reflection requirements from adapter inheritance.
template <typename T>
struct reflection_root {
  using type = T;
  static constexpr bool needs_reflection =
    !compat::disjunction<std::is_arithmetic<T>, is_string_like<T>>::value;
};

template <typename T, typename Allocator>
struct reflection_root<std::vector<T, Allocator>>: reflection_root<T> {};

template <typename T>
struct reflection_root<compat::optional<T>>: reflection_root<T> {};

template <typename T, typename Compare, typename Allocator>
struct reflection_root<std::set<T, Compare, Allocator>>: reflection_root<T> {};

template <bool BorrowStrings, typename T>
struct cpp11_lambda_write {
  const T &value;

  template <typename _M, omni::reflected_entity Entity>
  ::ryml::Tree operator()(omni::meta_t<_M, Entity>) const {
    ::ryml::Tree tree;
    diagnostics<false> report{};
    omni::ryml::detail::mapping_context context{
      report.config, &report, {}, 0, 0, BorrowStrings,
    };
    core::write_node(this->value, tree.rootref(), context);
    return tree;
  }
};

template <typename T>
struct cpp11_lambda_read {
  const ::ryml::ConstNodeRef &node;
  T &into;
  omni::ryml::detail::mapping_context &diagnostics;

  template <typename _M, omni::reflected_entity Entity>
  bool operator()(omni::meta_t<_M, Entity>) const {
    return core::read_node(this->node,
      &this->into,
      this->diagnostics);
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

template <typename T>
auto map_tree(::ryml::ConstNodeRef node,
  T *into,
  omni::ryml::detail::mapping_context &diagnostics)
  -> compat::enable_if_t<detail::reflection_root<T>::needs_reflection, bool> {
  return omni::reflected_call(
    detail::cpp11_lambda_read<T>{node, *into, diagnostics},
    omni::type_t<typename detail::reflection_root<T>::type>{});
}

template <typename T>
auto map_tree(::ryml::ConstNodeRef node,
  T *into,
  omni::ryml::detail::mapping_context &diagnostics)
  -> compat::enable_if_t<!detail::reflection_root<T>::needs_reflection, bool> {
  return read_node(node, into, diagnostics);
}

template <typename T>
with_diagnostics<T, diagnostics<false>> map_tree(const config &options,
  omni::type_t<T>,
  ::ryml::ConstNodeRef node) {
  auto value = compat::make_optional<T>();
  diagnostics<false> report{options, node, {}, {}};
  omni::ryml::detail::mapping_context context{
    options, &report, {}, 0, 0, false,
  };
  // FIXME: [high] Validate invalid/empty inputs at this public boundary before
  // dispatching to adapters, whose contract requires a valid node.
  const bool mapped = map_tree(node, &*value, context);
  return {mapped ? std::move(value) : compat::nullopt, std::move(report)};
}

template <typename T>
with_diagnostics<T, diagnostics<true>> map_tree(const config &options,
  omni::type_t<T> type,
  ::ryml::Tree tree) {
  // Move the owner only after mapping; NodeRefs refer to the tree object.
  return map_tree(options, type, tree.crootref())
    .transform_diagnostics(
      [&tree](diagnostics<false> report) -> diagnostics<true> {
        return {
          std::move(report.config),
          std::move(tree),
          std::move(report.issues),
          std::move(report.parse_message),
        };
      });
}

template <typename T>
with_diagnostics<T, diagnostics<true>> deserialize(const config &options,
  omni::type_t<T> type,
  std::string source) {
  auto parsed = omni::ryml::core::parse(std::move(source));
  if (!parsed) {
    return {
      compat::nullopt,
      {
        options,
        ::ryml::Tree{/*node_capacity=*/0, /*arena_capacity=*/0},
        {omni::ryml::detail::make_issue(json::issue_code::parse_error,
          json::value_kind::unknown,
          ::ryml::ConstNodeRef{},
          {})},
        std::move(parsed.error()),
      },
    };
  }

  return map_tree(options,
    type,
    options.transform_tree
      ? options.transform_tree(std::move(*parsed))
      : std::move(*parsed));
}

template <typename T>
auto as_owning_tree(const T &value)
  -> compat::enable_if_t<detail::reflection_root<T>::needs_reflection,
    ::ryml::Tree> {
  return omni::reflected_call(detail::cpp11_lambda_write<false, T>{value},
    omni::type_t<typename detail::reflection_root<T>::type>{});
}

template <typename T>
auto as_owning_tree(const T &value)
  -> compat::enable_if_t<!detail::reflection_root<T>::needs_reflection,
    ::ryml::Tree> {
  ::ryml::Tree tree;
  diagnostics<false> report{};
  omni::ryml::detail::mapping_context context{
    report.config, &report, {}, 0, 0, false,
  };
  write_node(value, tree.rootref(), context);
  return tree;
}

template <typename T>
auto as_ref_tree(T &value)
  -> compat::enable_if_t<
    detail::reflection_root<compat::remove_cvref_t<T>>::needs_reflection,
    ::ryml::Tree> {
  return omni::reflected_call(detail::cpp11_lambda_write<true, T>{value},
    omni::type_t<
      typename detail::reflection_root<compat::remove_cvref_t<T>>::type>{});
}

template <typename T>
auto as_ref_tree(T &value)
  -> compat::enable_if_t<
    !detail::reflection_root<compat::remove_cvref_t<T>>::needs_reflection,
    ::ryml::Tree> {
  ::ryml::Tree tree;
  diagnostics<false> report{};
  omni::ryml::detail::mapping_context context{
    report.config, &report, {}, 0, 0, true,
  };
  write_node(value, tree.rootref(), context);
  return tree;
}

} // namespace core

namespace mapping {

namespace detail {

template <typename T>
auto is_null(const T &)
  -> compat::enable_if_t<!is_optional_like<T>::value, bool> {
  return false;
}

template <typename T>
auto is_null(const T &value)
  -> compat::enable_if_t<is_optional_like<T>::value, bool> {
  return !value || is_null(*value);
}

} // namespace detail

template <typename T>
auto from_string_like::operator()(const T &value,
  ::ryml::NodeRef node,
  omni::ryml::detail::mapping_context &diagnostics) const
  -> compat::enable_if_t<supports<T>()> {
  if (diagnostics.borrow_strings)
    omni::ryml::detail::write_string<false>{}(value, &node);
  else
    omni::ryml::detail::write_string<true>{}(value, &node);
}

template <typename T>
auto to_string_like::operator()(::ryml::ConstNodeRef node,
  T *into,
  omni::ryml::detail::mapping_context &diagnostics) const
  -> compat::enable_if_t<supports<T>(), bool> {
  static_assert(std::is_same<char, typename T::value_type>::value,
    "Prototype string mapping requires char-based strings");
  if (node.is_container()) {
    diagnostics.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      json::value_kind::string,
      node,
      diagnostics.path));
    return false;
  }

  // Avoid constructing a string from a possibly null pointer when empty.
  const auto text = node.val();
  *into = text.empty() ? T{} : T{text.str, text.len};
  return true;
}

template <typename T>
auto from_arithmetic::operator()(const T &value,
  ::ryml::NodeRef node,
  omni::ryml::detail::mapping_context &) const
  -> compat::enable_if_t<supports<T>()> {
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
  omni::ryml::detail::mapping_context &diagnostics) const
  -> compat::enable_if_t<supports<T>(), bool> {
  const auto expected = std::is_same<bool, T>::value
    ? json::value_kind::boolean
    : std::is_unsigned<T>::value
      ? json::value_kind::unsigned_integer
      : std::is_integral<T>::value
        ? json::value_kind::integer
        : json::value_kind::number;

  if (node.is_container()) {
    diagnostics.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      expected,
      node,
      diagnostics.path));
    return false;
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
    diagnostics.report(omni::ryml::detail::make_issue(
      json::issue_code::invalid_scalar,
      expected,
      node,
      diagnostics.path));
    return false;
  }

  T converted{};
  if (!omni::ryml::detail::scalar_conversion::from_chars(scalar, &converted)) {
    diagnostics.report(omni::ryml::detail::make_issue(
      json::issue_code::out_of_range,
      expected,
      node,
      diagnostics.path));
    return false;
  }

  *into = converted;
  return true;
}

template <typename T>
auto from_optional_like::operator()(const T &value,
  ::ryml::NodeRef node,
  omni::ryml::detail::mapping_context &diagnostics) const
  -> compat::enable_if_t<supports<T>()> {
  if (value) {
    core::write_node(*value, node, diagnostics);
  } else {
    node.set_val("null");
    node.set_val_style(::ryml::VAL_PLAIN);
  }
}

template <typename T>
auto to_optional_like::operator()(::ryml::ConstNodeRef node,
  T *into,
  omni::ryml::detail::mapping_context &diagnostics) const
  -> compat::enable_if_t<supports<T>(), bool> {
  if (!node.is_container() && node.val_is_null()
    && (!node.has_val_tag()
      || ::ryml::TAG_NULL == ::ryml::to_tag(node.val_tag()))) {
    into->reset();
    return true;
  }

  typename T::value_type value{};
  if (!core::read_node(node, &value, diagnostics))
    return false;

  // FIXME: [high] [external] tl::optional 1.1.0 emplace() calls value(), whose
  // explicit throw fails under -fno-exceptions even for non-throwing T. Recheck
  // with an upstream fix; std::optional::emplace does not have this issue.
  into->emplace(std::move(value));
  return true;
}

template <typename T>
auto from_container::operator()(const T &value,
  ::ryml::NodeRef node,
  omni::ryml::detail::mapping_context &diagnostics) const
  -> compat::enable_if_t<supports<T>()> {
  node.set_seq();
  for (const auto &element : value) {
    if (diagnostics.stopped())
      break;

    if (!detail::is_null(element))
      core::write_node(element, node.append_child(), diagnostics);
  }
}

template <typename T>
auto to_container::operator()(::ryml::ConstNodeRef node,
  T *into,
  omni::ryml::detail::mapping_context &diagnostics) const
  -> compat::enable_if_t<supports<T>(), bool> {
  if (!node.is_seq()) {
    diagnostics.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      json::value_kind::sequence,
      node,
      diagnostics.path));
    return false;
  }

  into->clear();
  bool success = true;
  std::size_t index = 0;
  for (const auto child : node.children()) {
    if (diagnostics.stopped())
      return false;

    diagnostics.path.push_back({{}, index++});
    typename T::value_type element{};
    const bool mapped = core::read_node(child, &element, diagnostics);
    // TODO: Detect and report rejected duplicates in unique associative
    // containers instead of silently discarding them during emplacement.
    if (mapped)
      cp::emplace<T>{}(into, std::move(element));
    success = mapped && success;
    diagnostics.path.pop_back();
  }

  return success;
}

namespace reflected_scope {

namespace detail {

struct cpp11_write_member {
  ::ryml::NodeRef &node;
  omni::ryml::detail::mapping_context &diagnostics;

  template <typename Record, typename _F>
  void operator()(omni::data_member_binding_t<Record, _F> field) const {
    // Ad hoc: fn::each cannot short-circuit member traversal.
    // TODO: Let core traversal check stopping before invoking adapters.
    if (this->diagnostics.stopped())
      return;

    auto child = this->node.append_child();
    child.set_key_serialized(field.name());
    core::write_node(field.value(), child, this->diagnostics);
  }
};

struct cpp11_read_member {
  const ::ryml::ConstNodeRef &node;
  omni::ryml::detail::mapping_context &diagnostics;

  template <typename Record, typename _F>
  bool operator()(bool success,
    omni::data_member_binding_t<Record, _F> field) const {
    // Ad hoc: fn::foldl cannot short-circuit member traversal.
    // TODO: Let core traversal check stopping before invoking adapters.
    if (this->diagnostics.stopped())
      return false;

    const auto name = c4::to_csubstr(field.name());
    this->diagnostics.path.push_back({name, compat::nullopt});
    const auto child = this->node.find_child(name);
    if (child.invalid()) {
      this->diagnostics.report(
        omni::ryml::detail::make_issue(json::issue_code::missing_field,
          json::value_kind::unknown,
          child,
          this->diagnostics.path));
      this->diagnostics.path.pop_back();
      return false;
    }

    // A temporary also supports bitfields, which cannot expose references.
    typename omni::data_member_meta_t<_F>::type value = field.value();
    const bool mapped = core::read_node(child, &value, this->diagnostics);
    if (mapped)
      field.set_value(std::move(value));
    this->diagnostics.path.pop_back();
    return mapped && success;
  }
};

} // namespace detail

template <typename T>
auto from_enum::operator()(T value,
  ::ryml::NodeRef node,
  omni::ryml::detail::mapping_context &) const
  -> compat::enable_if_t<supports<T>()> {
  const auto enumerators = omni::meta_for<T>::enumerators();
  const auto found = std::find_if(enumerators.begin(),
    enumerators.end(),
    [value](const typename decltype(enumerators)::value_type &enumerator) {
      return value == enumerator.first;
    });
  if (found != enumerators.end()) {
    node.set_serialized(c4::to_csubstr(found->second));
    node.set_val_style(::ryml::VAL_DQUO);
  }
}

template <typename T>
auto to_enum::operator()(::ryml::ConstNodeRef node,
  T *into,
  omni::ryml::detail::mapping_context &diagnostics) const
  -> compat::enable_if_t<supports<T>(), bool> {
  // RapidYAML retains scalar text. Honor !!str and quotes; otherwise reject
  // plain null, numeric, and boolean forms before looking up the name.
  bool boolean{};
  const bool wrong_kind = node.is_container()
    || (node.has_val_tag()
      ? ::ryml::TAG_STR != ::ryml::to_tag(node.val_tag())
      : !node.is_val_quoted()
        && (node.val_is_null()
          || node.val().is_number()
          || c4::from_chars(node.val(), &boolean)));
  if (wrong_kind) {
    diagnostics.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      json::value_kind::string,
      node,
      diagnostics.path));
    return false;
  }

  const auto name = node.val();
  const auto enumerators = omni::meta_for<T>::enumerators();
  const auto found = std::find_if(enumerators.begin(),
    enumerators.end(),
    [name](const typename decltype(enumerators)::value_type &enumerator) {
      return name == c4::to_csubstr(enumerator.second);
    });
  if (found == enumerators.end()) {
    diagnostics.report(omni::ryml::detail::make_issue(
      json::issue_code::invalid_scalar,
      json::value_kind::string,
      node,
      diagnostics.path));
    return false;
  }

  *into = found->first;
  return true;
}

template <typename T>
auto from_record::operator()(const T &value,
  ::ryml::NodeRef node,
  omni::ryml::detail::mapping_context &diagnostics) const
  -> compat::enable_if_t<supports<T>()> {
  node.set_map();
  const auto binding = omni::meta_for<T>::bind(value);
  binding.accessible_data_members()
    | fn::filter<fn::non_static>()
    | fn::each(detail::cpp11_write_member{node, diagnostics});
}

// FIXME: [high] Restore duplicate/unknown-field policies and missing-field
// recovery through the new customization interface.
template <typename T>
auto to_record::operator()(::ryml::ConstNodeRef node,
  T *into,
  omni::ryml::detail::mapping_context &diagnostics) const
  -> compat::enable_if_t<supports<T>(), bool> {
  if (!node.is_map()) {
    diagnostics.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      json::value_kind::mapping,
      node,
      diagnostics.path));
    return false;
  }

  const auto binding = omni::meta_for<T>::bind(*into);
  return binding.accessible_data_members()
    | fn::filter<fn::non_static>()
    | fn::foldl(detail::cpp11_read_member{node, diagnostics}, true);
}

} // namespace reflected_scope

} // namespace mapping

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
