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
  omni::ryml::diagnostics</*OwnsTree=*/false> *diagnostics;
  std::vector<issue::path_segment> path;

  // Keep stop checks constant-time; counters can exceed the 16-bit limits.
  std::size_t error_count;
  std::size_t warning_count;

  // FIXME: [high] Ad hoc: builders erase their compile-time string-ownership
  // policy into this runtime flag because context stores diagnostics<false>.
  // Decouple reporting from diagnostics storage through a non-template report
  // interface and preserve the builder's policy in string writing.
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

// Preserve emplace_back support; packed containers may offer only push_back.
// The int overload takes priority when both operations are available.
template <typename Container>
auto append(Container *into, typename Container::value_type &&value, int)
  -> decltype(into->emplace_back(std::move(value)), void()) {
  into->emplace_back(std::move(value));
}

template <typename Container>
auto append(Container *into, typename Container::value_type &&value, long)
  -> decltype(into->push_back(std::move(value)), void()) {
  into->push_back(std::move(value));
}

} // namespace detail

namespace cp {

template <typename Container, typename Enable>
void emplace<Container, Enable>::operator()(Container *into,
  typename Container::value_type &&value) const {
  detail::append(into, std::move(value), /*priority=*/0);
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

template <bool BorrowStrings, typename T>
struct cpp11_lambda_write {
  const T &value;

  template <typename _M, omni::reflected_entity Entity>
  ::ryml::Tree operator()(omni::meta_t<_M, Entity>) const {
    ::ryml::Tree tree;
    diagnostics<false> report{};
    omni::ryml::detail::mapping_context context{
      /*options=*/report.config,
      /*diagnostics=*/&report,
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

template <typename T,
  typename Discovery,
  compat::enable_if_t<cp::reflect_as_root<Discovery>::value, int>>
bool map_tree(::ryml::ConstNodeRef node,
  T *into,
  omni::ryml::detail::mapping_context &diagnostics) {
  return omni::reflected_call(
    detail::cpp11_lambda_read<T>{
      /*node=*/node,
      /*into=*/ *into,
      /*diagnostics=*/diagnostics,
    },
    omni::type_t<Discovery>{});
}

template <typename T,
  typename Discovery,
  compat::enable_if_t<!cp::reflect_as_root<Discovery>::value
      && compat::disjunction<is_container<Discovery>,
        is_optional_like<Discovery>>::value,
    int>>
bool map_tree(::ryml::ConstNodeRef node,
  T *into,
  omni::ryml::detail::mapping_context &diagnostics) {
  return map_tree<T, typename Discovery::value_type>(node, into, diagnostics);
}

template <typename T,
  typename Discovery,
  compat::enable_if_t<!cp::reflect_as_root<Discovery>::value
      && !compat::disjunction<is_container<Discovery>,
        is_optional_like<Discovery>>::value,
    int>>
bool map_tree(::ryml::ConstNodeRef node,
  T *into,
  omni::ryml::detail::mapping_context &diagnostics) {
  return read_node(node, into, diagnostics);
}

template <typename T>
with_diagnostics<T, diagnostics<false>> map_tree(const config &options,
  omni::type_t<T>,
  ::ryml::ConstNodeRef node) {
  auto value = compat::make_optional<T>();
  diagnostics<false> report{
    /*config=*/options,
    /*tree=*/node,
    /*issues=*/{},
    /*parse_message=*/{},
  };
  omni::ryml::detail::mapping_context context{
    /*options=*/options,
    /*diagnostics=*/&report,
    /*path=*/{},
    /*error_count=*/0,
    /*warning_count=*/0,
    /*borrow_strings=*/false,
  };
  const bool mapped = map_tree(node, &*value, context);
  return {
    /*value=*/mapped ? std::move(value) : compat::nullopt,
    /*diagnostics=*/std::move(report),
  };
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
          /*config=*/std::move(report.config),
          /*tree=*/std::move(tree),
          /*issues=*/std::move(report.issues),
          /*parse_message=*/std::move(report.parse_message),
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
      /*value=*/compat::nullopt,
      /*diagnostics=*/{
        /*config=*/options,
        /*tree=*/::ryml::Tree{/*node_capacity=*/0, /*arena_capacity=*/0},
        /*issues=*/{
          omni::ryml::detail::make_issue(json::issue_code::parse_error,
            json::value_kind::unknown,
            ::ryml::ConstNodeRef{},
            /*path=*/{}),
        },
        /*parse_message=*/std::move(parsed.error()),
      },
    };
  }

  return map_tree(options, type, std::move(*parsed));
}

template <typename T,
  typename Discovery,
  compat::enable_if_t<cp::reflect_as_root<Discovery>::value, int>>
::ryml::Tree as_owning_tree(const T &value) {
  return omni::reflected_call(
    detail::cpp11_lambda_write</*BorrowStrings=*/false, T>{/*value=*/value},
    omni::type_t<Discovery>{});
}

template <typename T,
  typename Discovery,
  compat::enable_if_t<!cp::reflect_as_root<Discovery>::value
      && compat::disjunction<is_container<Discovery>,
        is_optional_like<Discovery>>::value,
    int>>
::ryml::Tree as_owning_tree(const T &value) {
  return as_owning_tree<T, typename Discovery::value_type>(value);
}

template <typename T,
  typename Discovery,
  compat::enable_if_t<!cp::reflect_as_root<Discovery>::value
      && !compat::disjunction<is_container<Discovery>,
        is_optional_like<Discovery>>::value,
    int>>
::ryml::Tree as_owning_tree(const T &value) {
  ::ryml::Tree tree;
  diagnostics<false> report{};
  omni::ryml::detail::mapping_context context{
    /*options=*/report.config,
    /*diagnostics=*/&report,
    /*path=*/{},
    /*error_count=*/0,
    /*warning_count=*/0,
    /*borrow_strings=*/false,
  };
  write_node(value, tree.rootref(), context);
  return tree;
}

template <typename T,
  typename Discovery,
  compat::enable_if_t<cp::reflect_as_root<Discovery>::value, int>>
::ryml::Tree as_ref_tree(T &value) {
  return omni::reflected_call(
    detail::cpp11_lambda_write</*BorrowStrings=*/true, T>{/*value=*/value},
    omni::type_t<Discovery>{});
}

template <typename T,
  typename Discovery,
  compat::enable_if_t<!cp::reflect_as_root<Discovery>::value
      && compat::disjunction<is_container<Discovery>,
        is_optional_like<Discovery>>::value,
    int>>
::ryml::Tree as_ref_tree(T &value) {
  return as_ref_tree<T, typename Discovery::value_type>(value);
}

template <typename T,
  typename Discovery,
  compat::enable_if_t<!cp::reflect_as_root<Discovery>::value
      && !compat::disjunction<is_container<Discovery>,
        is_optional_like<Discovery>>::value,
    int>>
::ryml::Tree as_ref_tree(T &value) {
  ::ryml::Tree tree;
  diagnostics<false> report{};
  omni::ryml::detail::mapping_context context{
    /*options=*/report.config,
    /*diagnostics=*/&report,
    /*path=*/{},
    /*error_count=*/0,
    /*warning_count=*/0,
    /*borrow_strings=*/true,
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
  const auto text = cp::read_string<T>{}(value);
  if (diagnostics.borrow_strings)
    node.set_val(text);
  else
    node.set_serialized(text);
  node.set_val_style(::ryml::VAL_DQUO);
}

template <typename T>
auto to_string_like::operator()(::ryml::ConstNodeRef node,
  T *into,
  omni::ryml::detail::mapping_context &diagnostics) const
  -> compat::enable_if_t<supports<T>(), bool> {
  if (node.is_container()) {
    diagnostics.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      json::value_kind::string,
      node,
      diagnostics.path));
    return false;
  }

  cp::assign_string<T>{}(into, node.val());
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

  const auto converted = omni::ryml::detail::from_chars<T>(scalar);
  if (!converted) {
    diagnostics.report(omni::ryml::detail::make_issue(
      converted.error(),
      expected,
      node,
      diagnostics.path));
    return false;
  }

  *into = *converted;
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

  // FIXME: [high] [external] Remove tl_optional-emplace.patch after an upstream
  // fix. Unpatched tl::optional 1.1.0 calls value(), whose explicit throw fails
  // under -fno-exceptions even for non-throwing T; fetched copies are patched.
  into->emplace(std::move(value));
  return true;
}

template <typename T>
auto from_container::operator()(const T &value,
  ::ryml::NodeRef node,
  omni::ryml::detail::mapping_context &diagnostics) const
  -> compat::enable_if_t<supports<T>()> {
  using element_type = typename T::value_type;
  using element_reference = decltype(*value.begin());

  // Materialized non-scalar elements may own strings borrowed by the tree.
  static_assert(compat::disjunction<std::is_scalar<element_type>,
    compat::conjunction<std::is_lvalue_reference<element_reference>,
      std::is_same<element_type,
        compat::remove_cvref_t<element_reference>>>>::value,
    "Default container serialization requires lvalue references to value_type "
    "for non-scalar C++ elements; specialize cp::from_type for value-producing "
    "or converting-proxy iterators.");

  node.set_seq();
  // Use value_type so packed containers' proxy references map as their values.
  for (const element_type &element : value) {
    if (diagnostics.stopped())
      break;

    if (!detail::is_null(element))
      core::write_node(element, node.append_child(), diagnostics);
  }

  // TODO: [medium] Consider supporting generators and temporary-producing
  // iterators by copying materialized elements' strings into tree storage.
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

    diagnostics.path.push_back({
      /*field=*/{},
      /*index=*/index++,
    });
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

struct cpp11_member_named {
  c4::csubstr name;

  template <typename _F>
  bool operator()(omni::data_member_meta_t<_F> field) const {
    return this->name == c4::to_csubstr(field.name());
  }
};

// SFINAE keeps T{} out of instantiations for non-default-constructible fields.
template <typename Record, typename _F>
auto initialize_missing(omni::data_member_binding_t<Record, _F> field)
  -> compat::enable_if_t<std::is_default_constructible<
    typename omni::data_member_meta_t<_F>::type>::value> {
  field.set_value(typename omni::data_member_meta_t<_F>::type{});
}

template <typename Record, typename _F>
auto initialize_missing(omni::data_member_binding_t<Record, _F>)
  -> compat::enable_if_t<!std::is_default_constructible<
    typename omni::data_member_meta_t<_F>::type>::value> {}

struct cpp11_require_member {
  const ::ryml::ConstNodeRef &node;
  omni::ryml::detail::mapping_context &diagnostics;

  template <typename Record, typename _F>
  void operator()(omni::data_member_binding_t<Record, _F> field) const {
    if (this->diagnostics.stopped())
      return;

    const auto name = c4::to_csubstr(field.name());
    if (!this->node.find_child(name).invalid())
      return;

    const auto recovery = this->diagnostics.options.recovery;
    if (recovery
      && (*recovery & json::recovery_policy::field_defaults)
        == json::recovery_policy::field_defaults
      && field.has_default_value_access())
      return;

    if ((is_optional_like<typename omni::data_member_meta_t<_F>::type>::value
          || (recovery
            && (*recovery & json::recovery_policy::default_initializable)
              == json::recovery_policy::default_initializable))
      && std::is_default_constructible<
        typename omni::data_member_meta_t<_F>::type>::value) {
      initialize_missing(field);
      return;
    }

    this->diagnostics.path.push_back({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });
    this->diagnostics.report(
      omni::ryml::detail::make_issue(json::issue_code::missing_field,
        json::value_kind::unknown,
        ::ryml::ConstNodeRef{},
        this->diagnostics.path));
    this->diagnostics.path.pop_back();
  }
};

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
    const auto children = this->node.children();
    const auto child =
      (this->diagnostics.options.error_policy.duplicates
        & json::duplicate_policy::last) == json::duplicate_policy::last
      ? std::accumulate(children.begin(),
          children.end(),
          ::ryml::ConstNodeRef{},
          [name](::ryml::ConstNodeRef selected, ::ryml::ConstNodeRef entry) {
            return name == entry.key() ? entry : selected;
          })
      : this->node.find_child(name);
    // The preceding presence pass already recovered or reported missing fields.
    if (child.invalid())
      return success;

    this->diagnostics.path.push_back({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });
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
    | fn::each(detail::cpp11_write_member{
      /*node=*/node,
      /*diagnostics=*/diagnostics,
    });
}

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

  const auto errors = diagnostics.error_count;
  const auto fields = omni::meta_for<T>::accessible_data_members()
    | fn::filter<fn::non_static>();

  // Structural issues precede missing fields and value mapping, preserving
  // diagnostic order regardless of the document's member order.
  for (const auto child : node.children()) {
    if (diagnostics.stopped())
      break;

    const auto name = child.key();
    const bool known = fn::any_of(
      detail::cpp11_member_named{/*name=*/name}, fields);
    const bool duplicate = known && node.find_child(name).id() != child.id();
    if (known && !duplicate)
      continue;

    const auto duplicates = diagnostics.options.error_policy.duplicates;
    const auto unknown = diagnostics.options.error_policy.unknown_fields;
    const bool allowed = duplicate
      ? (duplicates
          & (json::duplicate_policy::allow | json::duplicate_policy::last))
        != json::duplicate_policy::reject
      : (unknown & json::unknown_policy::allow) == json::unknown_policy::allow;
    const bool warn = duplicate
      ? (duplicates & json::duplicate_policy::warn)
        == json::duplicate_policy::warn
      : (unknown & json::unknown_policy::warn) == json::unknown_policy::warn;
    if (allowed && !warn)
      continue;

    diagnostics.path.push_back({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });
    diagnostics.report(omni::ryml::detail::make_issue(
      duplicate ? json::issue_code::duplicate_field
                : json::issue_code::unknown_field,
      json::value_kind::unknown,
      child,
      diagnostics.path,
      /*warning=*/allowed));
    diagnostics.path.pop_back();
  }

  const auto binding = omni::meta_for<T>::bind(*into);
  const auto members = binding.accessible_data_members()
    | fn::filter<fn::non_static>();
  members | fn::each(detail::cpp11_require_member{
    /*node=*/node,
    /*diagnostics=*/diagnostics,
  });
  const bool mapped = members
    | fn::foldl(detail::cpp11_read_member{
      /*node=*/node,
      /*diagnostics=*/diagnostics,
    }, /*initial=*/true);
  return mapped && errors == diagnostics.error_count;
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
