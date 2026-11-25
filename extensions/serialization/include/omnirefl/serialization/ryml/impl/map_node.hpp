#pragma once

#include <omnirefl/serialization/ryml/impl/enum.hpp>
#include <omnirefl/serialization/ryml/impl/record.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>
#include <omnirefl/serialization/ryml/with_diagnostics.hpp>

#include <cassert>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace detail {

// Ad hoc: `read_node` accesses state before invoking the selected mapping,
// so its terminal overloads must intercept the sentinel here. Remove these
// duplicate guards when the dispatch boundary can forward it to the mapper.
/** Terminal read: reports the depth limit without selecting a mapping. */
template <typename T>
auto read_node(const depth_exceeded &stopped, ::ryml::ConstNodeRef node,
  omni::type_t<T> type) -> with_diagnostics_t<T> {
  return with_depth_exceeded_operator{}(
    stopped, node, type);
}

template <typename T>
auto read_node(const depth_exceeded &stopped, ::ryml::ConstNodeRef node,
  T *into)
  -> with_diagnostics_t<compat::monostate> {
  return with_depth_exceeded_operator{}(
    stopped, node, into);
}

// REFACTORME: [high] Reassess this state-dependent dispatch boundary and
// its overlap with mapping selection, adapters, and diagnostic limits.
/** Node-to-type mapping through a customization or its default.
 *
 * Pointer-only mappings are adapted when T is default-constructible.
 * @pre Required metadata is available in reflected scope. */
OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(State context, ::ryml::ConstNodeRef node, omni::type_t<T> type)
  -> with_diagnostics_t<T, diagnostics>;

// REFACTORME: [high] Review responsibility for diagnostic accumulation and
// limit checks between this dispatcher and the node mappers.
/** Dispatch one node through the selected to_type callable with diagnostics.
 *
 * Prefers its type<T> overload when the produced value can be assigned to
 * *into; otherwise uses its T* overload. A mapping error does not trigger
 * fallback to another overload. */
OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(State context, ::ryml::ConstNodeRef node, T *into)
  -> with_diagnostics_t<compat::monostate, diagnostics>;

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(State context, ::ryml::ConstNodeRef node, omni::type_t<T> type)
  -> with_diagnostics_t<T, diagnostics> {
  const auto mapping = compat::invoke(context.map_node, context, type);
  return map_node_to_value(std::move(context),
    mapping, node, type);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (is_read_state<State>::value))
auto read_node(State context, ::ryml::ConstNodeRef node, T *into)
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  auto result = make_node_accumulator(context);
  if (context.limit_exceeded())
    result.result.value.reset();
  else {
    const auto mapping =
      compat::invoke(context.map_node, context, omni::type_t<T>{});

    result.append(read_into(std::move(context),
      mapping,
      node,
      into,
      /*priority=*/0));
  }

  return std::move(result).finish();
}

} // namespace detail

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value
    && (!std::is_const<T>::value
      || !std::is_same<bool, compat::remove_cv_t<T>>::value)
    && is_read_state<State>::value))
auto map_arithmetic_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type) const -> with_diagnostics_t<T> {
  return mapping_type<T>{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
    /*tdm_base=*/{},
    /*depth_base=*/{},
#endif
    this->with_node_check}(std::move(state), node, type);
}

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value
    && std::is_assignable<T &, T &&>::value
    && is_read_state<State>::value))
auto map_arithmetic_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node, T *into) const
  -> with_diagnostics_t<compat::monostate> {
  return mapping_type<T>{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
    /*tdm_base=*/{},
    /*depth_base=*/{},
#endif
    this->with_node_check}(std::move(state), node, into);
}

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_integral<T>::value
    && !std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_read_state<State>::value))
auto map_integer_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>) const -> with_diagnostics_t<T> {
  if (state.limit_exceeded())
    return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      state.config.error_policy, {}}, {}, {}}};

  if (this->with_node_check) {
    auto checked = core::expect_integer_node(node);
    if (!checked)
      return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
        /*base=*/{},
#endif
        state.config.error_policy, {}}, {std::move(checked.error())}, {}}};
  }

  return detail::node_result(std::move(state),
    core::parse_as_integral<T>(node), node, omni::type_t<T>{});
}

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_integral<T>::value
    && !std::is_same<bool, compat::remove_cv_t<T>>::value
    && std::is_assignable<T &, T &&>::value
    && is_read_state<State>::value))
auto map_integer_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node, T *into) const
  -> with_diagnostics_t<compat::monostate> {
  assert(into);
  return (*this)(std::move(state), node, omni::type_t<T>{})
    .transform([into](T &&value) -> compat::monostate {
      *into = std::move(value);
      return {};
    });
}

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_floating_point<T>::value && is_read_state<State>::value))
auto map_number_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>) const -> with_diagnostics_t<T> {
  if (state.limit_exceeded())
    return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      state.config.error_policy, {}}, {}, {}}};

  if (this->with_node_check) {
    auto checked = core::expect_number_node(node);
    if (!checked)
      return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
        /*base=*/{},
#endif
        state.config.error_policy, {}}, {std::move(checked.error())}, {}}};
  }

  return detail::node_result(std::move(state),
    core::parse_as_floating_point<T>(node), node, omni::type_t<T>{});
}

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_floating_point<T>::value
    && std::is_assignable<T &, T &&>::value
    && is_read_state<State>::value))
auto map_number_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node, T *into) const
  -> with_diagnostics_t<compat::monostate> {
  assert(into);
  return (*this)(std::move(state), node, omni::type_t<T>{})
    .transform([into](T &&value) -> compat::monostate {
      *into = std::move(value);
      return {};
    });
}

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_same<bool, compat::remove_cv_t<T>>::value
    && !std::is_const<T>::value && is_read_state<State>::value))
auto map_boolean_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>) const -> with_diagnostics_t<T> {
  if (state.limit_exceeded())
    return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      state.config.error_policy, {}}, {}, {}}};

  if (this->with_node_check) {
    auto checked = core::expect_boolean_node(node);
    if (!checked)
      return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
        /*base=*/{},
#endif
        state.config.error_policy, {}}, {std::move(checked.error())}, {}}};
  }

  return detail::node_result(std::move(state),
    core::parse_as_boolean(node), node, omni::type_t<T>{});
}

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_same<bool, compat::remove_cv_t<T>>::value
    && !std::is_const<T>::value
    && std::is_assignable<T &, T &&>::value
    && is_read_state<State>::value))
auto map_boolean_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node, T *into) const
  -> with_diagnostics_t<compat::monostate> {
  assert(into);
  return (*this)(std::move(state), node, omni::type_t<T>{})
    .transform([into](T &&value) -> compat::monostate {
      *into = std::move(value);
      return {};
    });
}

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_string_like<T>::value && is_read_state<State>::value))
auto map_string_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T>) const -> with_diagnostics_t<T> {
  if (state.limit_exceeded())
    return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      state.config.error_policy, {}}, {}, {}}};

  if (this->with_node_check) {
    auto checked = core::expect_string_node(node);
    if (!checked)
      return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
        /*base=*/{},
#endif
        state.config.error_policy, {}}, {std::move(checked.error())}, {}}};
  }

  return detail::node_result(std::move(state),
    core::parse_as_string_like<T>(node), node, omni::type_t<T>{});
}

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_string_like<T>::value
    && std::is_assignable<T &, T &&>::value
    && is_read_state<State>::value))
auto map_string_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node, T *into) const
  -> with_diagnostics_t<compat::monostate> {
  assert(into);
  return (*this)(std::move(state), node, omni::type_t<T>{})
    .transform([into](T &&value) -> compat::monostate {
      *into = std::move(value);
      return {};
    });
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_optional<T>, is_read_state<State>>::value))
auto to_optional_like_t::operator()(State context,
  ::ryml::ConstNodeRef node,
  T *into) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  if (!node.is_container() && node.val_is_null()
    && (!node.has_val_tag()
      || ::ryml::TAG_NULL == ::ryml::to_tag(node.val_tag()))) {
    adapt::reset(into);
    return omni::ryml::detail::make_node_accumulator(
      std::move(context)).finish();
  }

  return detail::read_node(std::move(context), node,
    omni::type_t<typename T::value_type>{})
    .transform([into](typename T::value_type &&value) -> compat::monostate {
      adapt::emplace(into, std::move(value));
      return {};
    });
}

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_optional<T>::value && is_read_state<State>::value))
auto map_value_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node, T *into) const
  -> with_diagnostics_t<compat::monostate> {
  if (state.limit_exceeded())
    return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      state.config.error_policy, {}}, {}, {}}};

  if (this->with_node_check) {
    auto checked = core::expect_value_node(node);
    if (!checked)
      return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
        /*base=*/{},
#endif
        state.config.error_policy, {}}, {std::move(checked.error())}, {}}};
  }

  return to_optional_like_t{}(std::move(state), node, into);
}

namespace detail {

// The record frame applies diagnostic limits; retain its direct mapping path.
OMNI_CONSTRAINED_TEMPLATE((typename T, typename State),
  (is_read_state<State>::value))
auto map_record_field_value(State state, ::ryml::ConstNodeRef node,
  omni::type_t<T> type) -> with_diagnostics_t<T> {
  const auto mapping = compat::invoke(state.map_node, state, type);
  return map_node_to_value_using(std::move(state), mapping, node, type,
    /*priority=*/0);
}

template <typename T>
auto map_record_field_value(const depth_exceeded &stopped,
  ::ryml::ConstNodeRef node, omni::type_t<T> type) -> with_diagnostics_t<T> {
  return read_node(stopped, node, type);
}

template <typename Record, std::size_t Index, typename Field, typename Frame>
bool process_record_member(Frame *frame) {
  // REFACTORME: [high] Encapsulate these inheritance-layout casts, including
  // presence processing, behind metadata-keyed slot access. The direct casts
  // currently avoid recursive tuple indexing and its wide-record compile cost.
  auto &slot = static_cast<
    reflected_scope::detail::record_value_slot<Field> &>(frame->values);
  const bool present = frame->pending[Index] != ::ryml::NONE;
  if (present || (slot.stored && std::tuple_size<decltype(
      frame->strategy.validate_fields)>::value != 0)) {
    constexpr auto type = omni::type_t<
      typename compat::remove_cvref_t<decltype(slot)>::type>{};
    constexpr auto field = typename compat::remove_cvref_t<
      decltype(slot)>::field_type{};
    const auto &state = frame->result->state.template field<Record>(field);
    const auto node = present
      ? ::ryml::ConstNodeRef{frame->object.tree(),
          frame->pending[Index]}
      : ::ryml::ConstNodeRef{};
    // Validation needs the enclosing policy/progress, not the child's path.
    // A terminal child has no state, so keep the parent for validation.
    auto mapped = validate_record_field(frame->result->state, node, field,
      compat::invoke([frame, &slot, &state, &node, &type, present]()
        -> with_diagnostics_t<typename decltype(type)::type> {
        if (!present)
          return {std::move(slot.stored),
            {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
              /*base=*/{},
#endif
              frame->result->state.config.error_policy, {}}, {}, {}}};
        return map_record_field_value(state, node, type);
      }), frame->strategy.validate_fields);
    if (!present)
      for (auto &entry : mapped.diagnostics.issues) {
        if (entry.node_id != ::ryml::NONE)
          continue;
        auto path = core::node_path(frame->object);
        path.push_back({c4::to_csubstr(field.name()), compat::nullopt});
        path.insert(path.end(), entry.path.begin(), entry.path.end());
        entry.path = std::move(path);
      }
    if (mapped.value)
      slot.stored.emplace(std::move(*mapped.value));
    if (!mapped.value || !mapped.diagnostics.issues.empty()
      || !mapped.diagnostics.parse_error_message.empty())
      frame->result->append(std::move(mapped).transform(
        omni::fn::default_value<compat::monostate>{}));
  }
  return !frame->result->stopped();
}

} // namespace detail
} // namespace ryml
} // namespace omni

#include <omnirefl/serialization/ryml/impl/sequence.hpp>

// TODO: [medium] Consider recovery for missing data members in map_object_node,
// including exposed default member initializers.

// FIXME: [high] Separate optional unwrapping from fundamental mappings:
// these optional-like mappings recursively invoke the type-mapping
// dispatchers for the contained value; they are not non-recursive defaults.
