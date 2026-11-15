#pragma once

// FIXME: [low] Freestanding: parse-error recovery requires setjmp/longjmp,
// which are not guaranteed by a freestanding implementation.

#include <c4/std/string.hpp>

#include <algorithm>
#include <csetjmp>
#include <iterator>
#include <type_traits>

namespace omni {
namespace ryml {
namespace detail {

// Built-ins stay separate from registrations so a user override never competes
// with a matching default partial specialization.
template <typename T, typename>
struct default_from_type {
  using type = typename std::conditional<omni::is_reflected_record<T>::value,
    reflected_scope::from_record,
    traits::cp::unavailable>::type;
};

template <typename T>
struct default_from_type<T,
  compat::enable_if_t<from_string_like::supports<T>()>> {
  using type = from_string_like;
};

template <typename T>
struct default_from_type<T,
  compat::enable_if_t<from_arithmetic::supports<T>()>> {
  using type = from_arithmetic;
};

template <typename T>
struct default_from_type<T,
  compat::enable_if_t<from_optional_like::supports<T>()>> {
  using type = from_optional_like;
};

template <typename T>
struct default_from_type<T,
  compat::enable_if_t<from_range_t::supports<T>()>> {
  using type = from_range_t;
};

template <typename T>
struct default_from_type<T,
  compat::enable_if_t<reflected_scope::from_enum::supports<T>()>> {
  using type = reflected_scope::from_enum;
};

// Built-ins stay separate from registrations so a user override never competes
// with a matching default partial specialization.
template <typename T, typename>
struct default_to_type {
  using type = typename std::conditional<omni::is_reflected_record<T>::value,
    reflected_scope::to_record_t<>,
    traits::cp::unavailable>::type;
};

template <typename T>
struct default_to_type<T,
  compat::enable_if_t<to_string_like::supports<T>()>> {
  using type = to_string_like;
};

template <typename T>
struct default_to_type<T,
  compat::enable_if_t<to_arithmetic::supports<T>()>> {
  using type = to_arithmetic;
};

template <typename T>
struct default_to_type<T,
  compat::enable_if_t<to_optional_like::supports<T>()>> {
  using type = to_optional_like;
};

template <typename T>
struct default_to_type<T,
  compat::enable_if_t<to_range_t::supports<T>()>> {
  using type = to_range_t;
};

template <typename T>
struct default_to_type<T,
  compat::enable_if_t<reflected_scope::to_enum_t<>::supports<T>()>> {
  using type = reflected_scope::to_enum_t<>;
};

template <typename T, bool Struct, bool Variable>
struct from_type_case {
  using type = cp::from_type_t<T>;
  static constexpr type get() { return {}; }
};

// Read the registered object so its configured state is preserved.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <typename T>
struct from_type_case<T, false, true> {
  using type = compat::remove_cvref_t<decltype(cp::from_type<T>)>;
  static constexpr type get() { return cp::from_type<T>; }
};
#endif

template <typename T>
struct from_type_case<T, false, false> {
  using type = default_from_type_t<T>;
  static constexpr type get() { return {}; }
};

template <typename T>
struct from_type_selection_t {
  static constexpr bool has_struct =
    !std::is_base_of<traits::cp::unavailable, cp::from_type_t<T>>::value;

// C++11 registers structs; C++14 also allows callable variable templates.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
  static constexpr bool has_variable = !std::is_same<traits::cp::unavailable,
    compat::remove_cvref_t<decltype(cp::from_type<T>)>>::value;
#else
  static constexpr bool has_variable = false;
#endif

  using selected = from_type_case<T, has_struct, has_variable>;
  using type = typename selected::type;
};

template <typename T, bool Struct, bool Variable>
struct to_type_case {
  using type = cp::to_type_t<T>;
  static constexpr type get() { return {}; }
};

// Read the registered object so its configured state is preserved.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
template <typename T>
struct to_type_case<T, false, true> {
  using type = compat::remove_cvref_t<decltype(cp::to_type<T>)>;
  static constexpr type get() { return cp::to_type<T>; }
};
#endif

template <typename T>
struct to_type_case<T, false, false> {
  using type = default_to_type_t<T>;
  static constexpr type get() { return {}; }
};

template <typename T>
struct to_type_selection_t {
  static constexpr bool has_struct =
    !std::is_base_of<traits::cp::unavailable, cp::to_type_t<T>>::value;

// C++11 registers structs; C++14 also allows callable variable templates.
#if defined(__cpp_variable_templates) && 201304L <= __cpp_variable_templates
  static constexpr bool has_variable = !std::is_same<traits::cp::unavailable,
    compat::remove_cvref_t<decltype(cp::to_type<T>)>>::value;
#else
  static constexpr bool has_variable = false;
#endif

  using selected = to_type_case<T, has_struct, has_variable>;
  using type = typename selected::type;
};

} // namespace detail

namespace core {

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (is_write_state<State>::value))
void write_node(const T &value, ::ryml::NodeRef node, State context) {
  omni::ryml::from_type(omni::type_t<T>{})(value, node, context);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(::ryml::ConstNodeRef node, T *into, State context)
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  auto result = omni::ryml::detail::make_node_accumulator(context);
  if (context.limit_exceeded())
    result.result.value.reset();
  else
    result.append(omni::ryml::to_type(omni::type_t<T>{})(node, into, context));

  return std::move(result).finish();
}

namespace detail {

template <typename State, typename T>
struct cpp11_lambda_write {
  const T &value;

  template <typename _M, omni::reflected_entity Entity>
  ::ryml::Tree operator()(omni::meta_t<_M, Entity>) const {
    ::ryml::Tree tree;
    const State context{};

    core::write_node(this->value, tree.rootref(), context);
    return tree;
  }
};

template <typename T, typename State>
struct cpp11_lambda_read {
  const ::ryml::ConstNodeRef &node;
  T &into;
  State context;

  template <typename _M, omni::reflected_entity Entity>
  auto operator()(omni::meta_t<_M, Entity>) const
    -> with_diagnostics_t<compat::monostate, diagnostics> {
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
  return compat::invoke( //<
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

// The discovery cases keep reflected_call confined to types that need metadata.
// Wrappers recurse through their element type without changing the destination.
template <typename Discovery,
  bool Reflect = cp::reflect_as_root<Discovery>::value,
  bool Wrapper = compat::disjunction<
    omni::ryml::detail::is_mapped_range<Discovery>,
    traits::is_optional<Discovery>>::value>
struct map_by_schema_case {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_read_state<State>::value))
  static auto map(::ryml::ConstNodeRef node, T *into,
    State context)
    -> with_diagnostics_t<compat::monostate, diagnostics> {
    return read_node(node, into, context);
  }
};

template <typename Discovery, bool Wrapper>
struct map_by_schema_case<Discovery, true, Wrapper> {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_read_state<State>::value))
  static auto map(::ryml::ConstNodeRef node, T *into,
    State context)
    -> with_diagnostics_t<compat::monostate, diagnostics> {
    return omni::reflected_call(
      cpp11_lambda_read<T, State>{
        /*node=*/node,
        /*into=*/ *into,
        /*context=*/context,
      },
      omni::type_t<Discovery>{});
  }
};

template <typename Discovery>
struct map_by_schema_case<Discovery, false, true> {
  OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
    (is_read_state<State>::value))
  static auto map(::ryml::ConstNodeRef node, T *into,
    State context)
    -> with_diagnostics_t<compat::monostate, diagnostics> {
    return map_by_schema_case<
      typename omni::ryml::detail::discovered_range_type<Discovery>::type>::map(
        node, into, context);
  }
};

} // namespace detail

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename Config, typename T),
  (is_config<Config>::value))
auto map_by_schema(const Config &options, omni::type_t<T>,
  ::ryml::ConstNodeRef node) -> with_diagnostics_t<T> {
  auto value = compat::make_optional<T>();
  const read_state_t<Config> context{
    /*config=*/options,
    /*path=*/{},
    /*error_count=*/0,
    /*warning_count=*/0,
  };

  auto mapped = detail::map_by_schema_case<compat::remove_cvref_t<T>>::map(
    node, &*value, context);
  return {
    /*value=*/mapped.value ? std::move(value) : compat::nullopt,
    /*diagnostics=*/std::move(mapped.diagnostics),
  };
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename Config, typename T),
  (is_config<Config>::value))
auto map_by_schema(const Config &options, omni::type_t<T> type,
  const ::ryml::Tree &tree) -> with_diagnostics_t<T> {
  return map_by_schema(options, type, tree.crootref());
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename Config, typename Record),
  (is_config<Config>::value))
auto deserialize(Config &&options, omni::type_t<Record> record_type,
  std::string source)
  -> with_diagnostics_t<Record, diagnostics_result_t<Config>> {
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

  auto mapped = map_by_schema(options, record_type, parsed->crootref());
  return {
    /*value=*/std::move(mapped.value),
    /*diagnostics=*/compat::invoke(
      std::forward<Config>(options).transform_diagnostics,
      static_cast<const ::ryml::Tree &>(*parsed),
      std::move(mapped.diagnostics)),
  };
}

namespace detail {

template <typename Discovery,
  bool Reflect = cp::reflect_as_root<Discovery>::value,
  bool Wrapper = compat::disjunction<
    omni::ryml::detail::is_mapped_range<Discovery>,
    traits::is_optional<Discovery>>::value>
struct tree_builder_case {
  OMNI_CONSTRAINED_TEMPLATE((typename State, typename T),
    (is_write_state<State>::value))
  static ::ryml::Tree build(T &value) {
    ::ryml::Tree tree;
    const State context{};

    write_node(value, tree.rootref(), context);
    return tree;
  }
};

template <typename Discovery, bool Wrapper>
struct tree_builder_case<Discovery, true, Wrapper> {
  OMNI_CONSTRAINED_TEMPLATE((typename State, typename T),
    (is_write_state<State>::value))
  static ::ryml::Tree build(T &value) {
    return omni::reflected_call(
      cpp11_lambda_write<State, T>{/*value=*/value},
      omni::type_t<Discovery>{});
  }
};

template <typename Discovery>
struct tree_builder_case<Discovery, false, true> {
  OMNI_CONSTRAINED_TEMPLATE((typename State, typename T),
    (is_write_state<State>::value))
  static ::ryml::Tree build(T &value) {
    return tree_builder_case<
      typename omni::ryml::detail::discovered_range_type<Discovery>::type>
        ::template build<State>(value);
  }
};

} // namespace detail

template <typename T>
::ryml::Tree as_owning_tree(const T &value) {
  return detail::tree_builder_case<compat::remove_cvref_t<T>>
    ::template build<write_state_t<false>>(value);
}

template <typename T>
::ryml::Tree as_ref_tree(T &value) {
  return detail::tree_builder_case<compat::remove_cvref_t<T>>
    ::template build<write_state_t<true>>(value);
}

} // namespace core

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
      if (json::issue_code::out_of_range == entry.reason)
        message += "could not be converted to the destination scalar"; else {
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
