#pragma once

#include <utility>

namespace omni {
namespace ryml {
namespace reflected_scope {

template <typename ValueByAlias>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_enum<T>,
    is_read_state<State>>::value))
auto to_enum_t<ValueByAlias>::operator()(::ryml::ConstNodeRef node,
  omni::type_t<T>,
  State context) const
  -> with_diagnostics_t<T, diagnostics> {
  // RapidYAML retains scalar text. Honor !!str and quotes; otherwise reject
  // plain null, numeric, and boolean forms before looking up the name.
  const bool wrong_kind = compat::invoke([node]() -> bool {
    bool boolean{}; // Discard the parsed value; only conversion success matters.
    return node.is_container()
      || (node.has_val_tag()
        ? ::ryml::TAG_STR != ::ryml::to_tag(node.val_tag())
        : !node.is_val_quoted()
          && (node.val_is_null()
            || node.val().is_number()
            || c4::from_chars(node.val(), &boolean)));
  });

  if (wrong_kind) {
    return {
      /*value=*/compat::nullopt,
      /*diagnostics=*/{
        /*config=*/{context.config.error_policy, {}},
        /*issues=*/{omni::ryml::detail::make_issue(
          json::issue_code::unexpected_kind,
          json::value_kind::string,
          node,
          context.path)},
        /*parse_error_message=*/{},
      },
    };
  }

  constexpr auto enumerators = omni::meta_for<T>::enumerators();
  const auto find_name = [enumerators](compat::string_view name)
    -> compat::optional<T> {
    for (const auto &enumerator : enumerators)
      if (name == compat::string_view{enumerator.second})
        return enumerator.first;

    return compat::nullopt;
  };

  const compat::string_view value{node.val().str, node.val().len};
  if (const auto found = find_name(value)) {
    return {
      /*value=*/found,
      /*diagnostics=*/{
        /*config=*/{context.config.error_policy, {}},
        /*issues=*/{},
        /*parse_error_message=*/{},
      },
    };
  }

  if (const auto canonical = compat::invoke(this->value_by_alias, value))
    if (const auto found = find_name(*canonical)) {
      return {
        /*value=*/found,
        /*diagnostics=*/{
          /*config=*/{context.config.error_policy, {}},
          /*issues=*/{},
          /*parse_error_message=*/{},
        },
      };
    }

  return {
    /*value=*/compat::nullopt,
    /*diagnostics=*/{
      /*config=*/{context.config.error_policy, {}},
      /*issues=*/{omni::ryml::detail::make_issue(
        json::issue_code::invalid_scalar,
        json::value_kind::string,
        node,
        context.path)},
      /*parse_error_message=*/{},
    },
  };
}

template <typename ValueByAlias>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_enum<T>,
    is_read_state<State>>::value))
auto to_enum_t<ValueByAlias>::operator()(::ryml::ConstNodeRef node,
  T *into,
  State context) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  auto result = (*this)(node, omni::type_t<T>{}, context);
  if (result.value)
    *into = *result.value;

  return {
    /*value=*/result.value
      ? compat::make_optional(compat::monostate{}) : compat::nullopt,
    /*diagnostics=*/std::move(result.diagnostics),
  };
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_enum<T>,
    is_write_state<State>>::value))
auto from_enum_t::operator()(T value, ::ryml::NodeRef node, State) const -> void {
  constexpr auto enumerators = omni::meta_for<T>::enumerators();
  for (const auto &enumerator : enumerators)
    if (value == enumerator.first) {
      node.set_serialized(c4::to_csubstr(enumerator.second));
      node.set_val_style(::ryml::VAL_DQUO);
      return;
    }
}

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
