#pragma once

#include <omnirefl/functional.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace omni {
namespace ryml {
namespace reflected_scope {
namespace detail {

// Deduce the opaque field tag through its public metadata wrapper.
template <typename Record, typename _F>
constexpr omni::data_member_binding_t<Record, _F> bind_record_member(
  Record *into, omni::data_member_meta_t<_F>) {
  return omni::data_member_binding_t<Record, _F>{*into};
}

// Optional storage does not default-construct field values before mapping.
// Inheriting the metadata wrapper supplies aggregate_into's named-field protocol.
template <typename Field>
struct record_value_slot: Field {
  compat::optional<typename Field::type> stored;

  typename Field::type &&value() && { return std::move(*this->stored); }
};

template <typename... Field>
auto record_value_slots(const std::tuple<Field...> &)
  -> std::tuple<record_value_slot<Field>...> {
  return {};
}

template <typename T>
struct assign_record_fields {
  T *into;

  template <typename Field>
  void operator()(record_value_slot<Field> &field) const {
    bind_record_member(this->into, static_cast<Field>(field))
      .set_value(std::move(field).value());
  }
};

// A conversion result lets optional construct T directly; forwarding an
// already-constructed T through its constructor would require another move.
template <typename T, typename Frame>
struct aggregate_record_constructor {
  Frame *frame;

  operator T() && { return omni::refl::aggregate_into<T>(this->frame->values); }
};

template <typename T>
struct aggregate_record_value {
  using result_type = with_diagnostics_t<T>;

  template <typename Frame>
  void operator()(Frame *frame) const {
    frame->value->emplace(aggregate_record_constructor<T, Frame>{frame});
  }
};

template <typename T>
struct bind_record_value {
  using result_type = with_diagnostics_t<T>;

  template <typename Frame>
  void operator()(Frame *frame) const {
    frame->values | fn::each(assign_record_fields<T>{
      std::addressof(frame->value->emplace())});
  }
};

template <typename T>
struct aggregate_record_into {
  using result_type = with_diagnostics_t<compat::monostate>;
  T *into;

  template <typename Frame>
  void operator()(Frame *frame) const {
    *this->into = omni::refl::aggregate_into<T>(frame->values);
    frame->value->emplace();
  }
};

template <typename T>
struct bind_record_into {
  using result_type = with_diagnostics_t<compat::monostate>;
  T *into;

  template <typename Frame>
  void operator()(Frame *frame) const {
    frame->values | fn::each(assign_record_fields<T>{this->into});
    frame->value->emplace();
  }
};

} // namespace detail
} // namespace reflected_scope

namespace detail {

template <typename Slot>
void initialize_missing_record_slot(Slot *slot, std::true_type) {
  slot->stored.emplace();
}

template <typename Slot>
void initialize_missing_record_slot(Slot *, std::false_type) {}

template <typename State, typename Slot>
bool process_record_presence(node_accumulator<State> *result,
  ::ryml::ConstNodeRef object, ::ryml::id_type node_id, Slot *slot) {
  if (node_id != ::ryml::NONE)
    return true;

  using member_type = typename Slot::type;
  using absent_optional = compat::conjunction<traits::is_optional<member_type>,
    std::is_default_constructible<member_type>>;
  if (absent_optional::value) {
    initialize_missing_record_slot(slot, absent_optional{});
    return !result->stopped();
  }

  // The absent field has no node; derive its path from the object.
  auto missing = core::make_issue(json::issue_code::missing_field,
    json::value_kind::unknown, ::ryml::ConstNodeRef{});
  missing.path = core::node_path(object);
  missing.path.push_back({c4::to_csubstr(slot->name()), compat::nullopt});
  result->report(std::move(missing));
  return !result->stopped();
}

// Events capture their frame; the fold carries only continuation success.
struct cpp11_process_record_event {
  template <typename Event>
  auto operator()(compat::monostate, const Event &event) const
    -> compat::expected<compat::monostate, compat::monostate> {
    if (event())
      return compat::monostate{};

    return compat::unexpected<compat::monostate>{compat::monostate{}};
  }
};

// GCC 7 rejects pack expansions found only inside lambda bodies. Indexed
// callables retain the same reference capture without that expansion.
template <std::size_t Index, bool CheckPresence, typename Frame>
struct cpp11_record_member_event {
  Frame &frame;

  // Defined after type-mapping selection is available.
  bool operator()() const;
};

// Tuple indices keep field values and selected nodes paired without a search.
template <typename State, typename Values, typename Finalizer,
  std::size_t... Index>
auto process_record_members(node_accumulator<State> *result,
  ::ryml::ConstNodeRef object, Values *values,
  const std::array<::ryml::id_type, sizeof...(Index)> &pending,
  Finalizer finalizer, compat::index_sequence<Index...>)
  -> typename Finalizer::result_type {
  struct {
    node_accumulator<State> *result;
    ::ryml::ConstNodeRef object;
    Values &values;
    const std::array<::ryml::id_type, sizeof...(Index)> &pending;
    compat::optional<typename Finalizer::result_type::value_type> *value;
  } frame{result, object, *values, pending, nullptr};

  const auto events = std::tuple_cat(
    std::make_tuple(
      cpp11_record_member_event<Index, true, decltype(frame)>{frame}...),
    std::make_tuple(
      cpp11_record_member_event<Index, false, decltype(frame)>{frame}...));

  auto folded = events | fn::try_fold(cpp11_process_record_event{},
    compat::expected<compat::monostate, compat::monostate>{
      compat::monostate{}});
  if (!result->result.value)
    return {compat::nullopt, std::move(result->result.diagnostics)};

  // The output starts after traversal, keeping it unaliased throughout the fold.
  compat::optional<typename Finalizer::result_type::value_type> value{};
  frame.value = &value;
  static_cast<void>(std::move(folded).transform(
    [&frame, &finalizer](compat::monostate) { finalizer(&frame); }));
  return {std::move(value), std::move(result->result.diagnostics)};
}

template <typename State>
struct cpp11_write_member {
  ::ryml::NodeRef &node;
  State context;

  template <typename Record, typename _F>
  void operator()(omni::data_member_binding_t<Record, _F> field) const {
    auto child = this->node.append_child();
    child.set_key_serialized(field.name());
    core::write_node(this->context, field.value(), child);
  }
};

} // namespace detail

namespace reflected_scope {
namespace detail {

template <typename T, typename State, typename NameByAlias, typename Finalizer>
auto map_record(State context, ::ryml::ConstNodeRef node,
  const NameByAlias &name_by_alias, Finalizer finalizer)
  -> typename Finalizer::result_type {
  auto result = omni::ryml::detail::make_node_accumulator(
    std::move(context));
  assert(node.is_map());

  // C++11 tuple operations need not support constant evaluation.
#if 201402L <= OMNI_CPLUSPLUS
  constexpr auto fields = omni::meta_for<T>::accessible_data_members()
    | fn::filter(fn::is_non_static);
  constexpr auto table = fn::lookup_table(fields);
#else
  const auto fields = omni::meta_for<T>::accessible_data_members()
    | fn::filter(fn::is_non_static);
  const auto table = fn::lookup_table(fields);
#endif

  // Selected children share this object's tree; only IDs need storage.
  std::array<::ryml::id_type,
    std::tuple_size<decltype(fields)>::value> pending;
  pending.fill(::ryml::NONE);

  // The default no-alias resolver makes this cache unused and removable.
  std::array<compat::optional<bool>,
    std::tuple_size<decltype(fields)>::value> canonical_presence{};

  // Structural issues precede missing fields and value mapping, preserving
  // diagnostic order regardless of the document's member order.
  for (const auto child : node.children()) {
    if (result.stopped())
      break;

    const auto name = child.key();
    const compat::string_view input_name{name.str, name.len};
    auto direct = table.lookup(input_name);
    const auto canonical = direct
      ? compat::decay_t<decltype(compat::invoke(name_by_alias, input_name))>{}
      : compat::invoke(name_by_alias, input_name);
    const auto field = std::move(direct).or_else([&table, &canonical](
      fn::not_found_t<compat::string_view> missing) {
      if (canonical)
        return table.lookup(compat::string_view{*canonical});

      return decltype(table.lookup(missing.name))::failure(std::move(missing));
    });

    if (canonical && field) {
      auto &presence = canonical_presence[field.value().index];
      if (!presence)
        presence = !node.find_child(
          {canonical->data(), canonical->size()}).invalid();

      if (*presence)
        continue;
    }

    const auto entry = field ? pending.begin() + field.value().index
      : pending.end();

    const bool known = entry != pending.end();
    const bool duplicate = known && *entry != ::ryml::NONE;

    if (known && !duplicate) {
      *entry = child.id();
      continue;
    }

    const auto duplicates = result.state.config.error_policy.duplicates;
    if (duplicate
      && (duplicates & json::duplicate_policy::last)
        == json::duplicate_policy::last)
      *entry = child.id();

    const auto unknown = result.state.config.error_policy.unknown_fields;
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

    result.report(omni::ryml::core::make_issue(
      duplicate ? json::issue_code::duplicate_field
                : json::issue_code::unknown_field,
      json::value_kind::unknown,
      child,
      /*warning=*/allowed));
  }

  if (result.stopped())
    return {compat::nullopt, std::move(result.result.diagnostics)};

  auto values = record_value_slots(fields);
  return omni::ryml::detail::process_record_members(&result, node, &values,
    pending, std::move(finalizer),
    compat::make_index_sequence<std::tuple_size<decltype(fields)>::value>{});
}

} // namespace detail

template <typename NameByAlias>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_record<T>,
    compat::disjunction<std::is_default_constructible<T>,
      omni::refl::is_aggregatable<T>>, is_read_state<State>>::value))
auto to_record_t<NameByAlias>::operator()(State context,
  ::ryml::ConstNodeRef node, omni::type_t<T>) const
  -> with_diagnostics_t<T, diagnostics> {
  // Before C++17, aggregate_into's return requires a usable move constructor.
  const auto finalize = typename traits::select<
    traits::case_<omni::refl::is_aggregatable<T>::value
        && std::is_move_constructible<T>::value,
      detail::aggregate_record_value<T>>,
    detail::bind_record_value<T>>::type{};
  return detail::map_record<T>(std::move(context), node, this->name_by_alias,
    finalize);
}

template <typename NameByAlias>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_record<T>,
    is_read_state<State>>::value))
auto to_record_t<NameByAlias>::operator()(State context,
  ::ryml::ConstNodeRef node, T *into) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  // Before C++17, aggregate_into's return requires a usable move constructor.
  const auto finalize = typename traits::select<
    traits::case_<omni::refl::is_aggregatable<T>::value
        && std::is_move_constructible<T>::value
        && std::is_assignable<T &, T &&>::value,
      detail::aggregate_record_into<T>>,
    detail::bind_record_into<T>>::type{into};
  return detail::map_record<T>(std::move(context), node, this->name_by_alias,
    finalize);
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_record<T>,
    is_write_state<State>>::value))
auto from_record_t::operator()(State context,
  const T &value,
  ::ryml::NodeRef node) const
  -> void {
  node.set_map();
  const auto binding = omni::meta_for<T>::bind(value);
  binding.accessible_data_members()
    | fn::filter(fn::is_non_static)
    | fn::each(omni::ryml::detail::cpp11_write_member<State>{
      /*node=*/node,
      /*context=*/std::move(context),
    });
}

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
