#pragma once

#include <array>
#include <cassert>
#include <cstddef>

namespace omni {
namespace ryml {
namespace core {
namespace reflected_scope {

template <typename Strategy>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_enum<T>,
    is_read_state<State>>::value))
auto map_node_to_enum_t<Strategy>::operator()(State context,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>) const
  -> with_diagnostics_t<T, diagnostics> {
  assert(core::expect_string_node(node));

  constexpr auto enumerators = omni::meta_for<T>::enumerators();
#if !defined(NDEBUG)
  std::array<compat::string_view, enumerators.size()> names{};
  for (std::size_t index = 0; index < enumerators.size(); ++index)
    names[index] = enumerators[index].second;
  assert(mapping::valid_aliases(names, this->strategy.aliases_by_name));
#endif
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

  if (const auto canonical = mapping::resolve_alias(this->strategy.aliases_by_name, value))
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

template <typename Strategy>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_enum<T>,
    is_write_state<State>>::value))
auto from_enum_t<Strategy>::operator()(State,
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
