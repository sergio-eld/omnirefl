#pragma once

// FIXME: [low] [external] Freestanding: RapidYAML documents setjmp/longjmp
// for parse-error recovery without exceptions; freestanding implementations
// need not provide these functions.

#include <omnirefl/extensions/compat.hpp>

// RapidYAML: third-party JSON/YAML backend.
#include <ryml.hpp>

#include <csetjmp>
#include <string>
#include <utility>

namespace omni {
namespace ryml {
namespace core {

inline compat::expected<::ryml::Tree, std::string> parse(std::string source,
  ::ryml::ParserOptions options) {
  // Static storage lets the parser callback use the signal without captures.
  static constexpr int parse_failed = 1;

  struct _error_t {
    std::jmp_buf jump;
    std::string message;
    ::ryml::Location location;
  } error{};

  // RapidYAML Quickstart, sample_error_handler(): non-returning error recovery.
  // https://rapidyaml.readthedocs.io/latest/doxygen/group__doc__quickstart.html
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
        message += "is out of range";
      else {
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

} // namespace ryml
} // namespace omni
