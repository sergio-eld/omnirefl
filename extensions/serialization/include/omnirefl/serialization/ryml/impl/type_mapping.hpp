#pragma once

#include <c4/format.hpp>

#include <cassert>
#include <limits>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (traits::is_string_like<T>::value && is_read_state<State>::value))
auto map_string_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type) const -> with_diagnostics_t<T> {
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

  return core::map_node_to_string_like(std::move(state), node, type);
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
  (compat::conjunction<traits::is_string_like<T>,
    is_write_state<State>>::value))
auto map_string_like_t::operator()(State context,
  const T &value,
  ::ryml::NodeRef node) const
  -> void {
  const auto &text = adapt::as_string(value);
  using storage_type = decltype(adapt::as_string(value));
  using text_type = compat::remove_cvref_t<storage_type>;
  static_assert(std::is_same<char, typename text_type::value_type>::value,
    "Ryml string mapping currently supports only char text.");

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

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_same<bool, compat::remove_cv_t<T>>::value
    && !std::is_const<T>::value && is_read_state<State>::value))
auto map_boolean_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type) const -> with_diagnostics_t<T> {
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

  return core::map_node_to_boolean(std::move(state), node, type);
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

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_write_state<State>::value))
void map_boolean_t::operator()(State,
  const T &value,
  ::ryml::NodeRef node) const {
  // C4's plain bool conversion emits 0/1 rather than document booleans.
  node.set_serialized(c4::fmt::boolalpha(value));
}

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_integral<T>::value
    && !std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_read_state<State>::value))
auto map_integer_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type) const -> with_diagnostics_t<T> {
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

  return core::map_node_to_integral(std::move(state), node, type);
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

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_integral<T>::value
    && !std::is_same<bool, compat::remove_cv_t<T>>::value
    && is_write_state<State>::value))
void map_integral_t::operator()(State,
  const T &value,
  ::ryml::NodeRef node) const {
  node.set_serialized(value);
}

template <typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_floating_point<T>::value && is_read_state<State>::value))
auto map_number_node_t<WithNodeCheck>::operator()(State state,
  ::ryml::ConstNodeRef node,
  omni::type_t<T> type) const -> with_diagnostics_t<T> {
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

  return core::map_node_to_floating_point(std::move(state), node, type);
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

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_floating_point<T>::value && is_write_state<State>::value))
void map_floating_point_t::operator()(State,
  const T &value,
  ::ryml::NodeRef node) const {
  // Preserve enough significant digits to recover the original value.
  node.set_serialized(c4::fmt::real(value,
    std::numeric_limits<T>::max_digits10, c4::FTOA_FLEX));
}

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

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (std::is_arithmetic<T>::value && is_write_state<State>::value))
auto map_arithmetic_t::operator()(State context,
  const T &value,
  ::ryml::NodeRef node) const -> void {
  const auto write = typename traits::select<
    traits::case_<std::is_same<bool, compat::remove_cv_t<T>>::value,
      map_boolean_t>,
    traits::case_<std::is_integral<T>::value, map_integral_t>,
    map_floating_point_t>::type{};

  write(std::move(context), value, node);
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

  return core::read_node(std::move(context), node,
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

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<traits::is_optional<T>, is_write_state<State>>::value))
auto map_optional_like_t::operator()(State context,
  const T &value,
  ::ryml::NodeRef node) const
  -> void {
  if (adapt::has_value(value))
    core::write_node(std::move(context), adapt::get_value(value), node);
  else {
    node.set_val("null");
    node.set_val_style(::ryml::VAL_PLAIN);
  }
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
  return core::read_node(stopped, node, type);
}

template <typename Record, std::size_t Index, bool CheckPresence,
  typename Frame>
bool cpp11_record_member_event<Record, Index, CheckPresence, Frame>
  ::operator()() const {
  auto &slot = std::get<Index>(this->frame.values);
  if (CheckPresence)
    return process_record_presence(this->frame.result, this->frame.object,
      this->frame.pending[Index], &slot);

  const bool present = this->frame.pending[Index] != ::ryml::NONE;
  if (present || (slot.stored && std::tuple_size<decltype(
      this->frame.strategy.validate_fields)>::value != 0)) {
    constexpr auto type = omni::type_t<
      typename compat::remove_cvref_t<decltype(slot)>::type>{};
    constexpr auto field = typename compat::remove_cvref_t<
      decltype(slot)>::field_type{};
    const auto &state = this->frame.result->state.template field<Record>(field);
    const auto node = present
      ? ::ryml::ConstNodeRef{this->frame.object.tree(),
          this->frame.pending[Index]}
      : ::ryml::ConstNodeRef{};
    // Validation needs the enclosing policy/progress, not the child's path.
    // A terminal child has no state, so keep the parent for validation.
    auto mapped = validate_record_field(this->frame.result->state, node, field,
      compat::invoke([this, &slot, &state, &node, &type, present]()
        -> with_diagnostics_t<typename decltype(type)::type> {
        if (!present)
          return {std::move(slot.stored),
            {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
              /*base=*/{},
#endif
              this->frame.result->state.config.error_policy, {}}, {}, {}}};
        return map_record_field_value(state, node, type);
      }), this->frame.strategy.validate_fields);
    if (!present)
      for (auto &entry : mapped.diagnostics.issues) {
        if (entry.node_id != ::ryml::NONE)
          continue;
        auto path = core::node_path(this->frame.object);
        path.push_back({c4::to_csubstr(field.name()), compat::nullopt});
        path.insert(path.end(), entry.path.begin(), entry.path.end());
        entry.path = std::move(path);
      }
    if (mapped.value)
      slot.stored.emplace(std::move(*mapped.value));
    if (!mapped.value || !mapped.diagnostics.issues.empty()
      || !mapped.diagnostics.parse_error_message.empty())
      this->frame.result->append(std::move(mapped).transform(
        omni::fn::default_value<compat::monostate>{}));
  }
  return !this->frame.result->stopped();
}

} // namespace detail
} // namespace ryml
} // namespace omni
