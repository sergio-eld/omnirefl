#pragma once

#include <cassert>

namespace omni {
namespace ryml {
namespace core {
namespace reflected_scope {

template <typename ValueByAlias>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_enum<T>,
    is_read_state<State>>::value))
auto map_node_to_enum_t<ValueByAlias>::operator()(::ryml::ConstNodeRef node,
  omni::type_t<T>,
  State context) const
  -> with_diagnostics_t<T, diagnostics> {
  assert(core::expect_string_node(node).value);

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

} // namespace reflected_scope
} // namespace core

namespace reflected_scope {

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
