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
auto map_node_to_enum_t<ValueByAlias>::operator()(State context,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>) const
  -> with_diagnostics_t<T, diagnostics> {
  assert(core::expect_string_node(node));

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
      /*issues=*/{omni::ryml::core::make_issue(
        json::issue_code::invalid_scalar,
        json::value_kind::string,
        node)},
      /*parse_error_message=*/{},
    },
  };
}

} // namespace reflected_scope
} // namespace core

namespace reflected_scope {

template <typename Default>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_enum<T>,
    is_write_state<State>>::value))
auto from_enum_t<Default>::operator()(State,
  T value,
  ::ryml::NodeRef node) const -> void {
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

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
namespace omni {
namespace ryml {
namespace core {
namespace reflected_scope {

template <typename ValueByAlias>
constexpr map_node_to_enum_t<ValueByAlias>
  map_node_to_enum_t<ValueByAlias>::_default{};

} // namespace reflected_scope
} // namespace core
} // namespace ryml
} // namespace omni
#endif

#if !defined(__cpp_inline_variables) || 201606L > __cpp_inline_variables
namespace omni {
namespace ryml {
namespace reflected_scope {

template <typename Default>
constexpr from_enum_t<Default> from_enum_t<Default>::_default{};

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
#endif
