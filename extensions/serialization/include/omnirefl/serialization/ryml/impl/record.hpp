#pragma once

#include <omnirefl/functional.hpp>
#include <omnirefl/extensions/static_string.hpp>
#include <omnirefl/serialization/ryml/core.hpp>
#include <omnirefl/serialization/ryml/impl/compose.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>

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
// Inheriting the metadata wrapper supplies aggregate_into's named-field
// protocol.
template <typename Field>
struct record_value_slot: Field {
  // Match the metadata identity used by explicit state.field(meta) traversal.
  using field_type = Field;
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
    static_assert(std::is_default_constructible<T>::value,
      "Filtered value mapping requires a default-constructible record; "
      "use the T* overload for an existing root");
    frame->values | omni::fn::each(assign_record_fields<T>{
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
    frame->values | omni::fn::each(assign_record_fields<T>{this->into});
    frame->value->emplace();
  }
};

} // namespace detail
} // namespace reflected_scope

namespace detail {

// Validation is part of the active node mapping, so nested failures and
// warning/error budgets are resolved before the parent proceeds.
template <typename State, typename T>
struct validation_frame {
  using value_type = T;
  node_accumulator<State> result;
  compat::optional<T> value;
  ::ryml::ConstNodeRef node;

  template <typename Check>
  void check(const Check &validate, std::true_type) {
    if (!this->value || !this->result.result.value || this->result.stopped())
      return;

    auto checked = node_result(this->result.state,
      compat::invoke(validate, std::move(*this->value)), this->node);
    static_assert(std::is_same<T,
        typename decltype(checked)::value_type>::value,
      "Validators must preserve the value type");
    for (auto &entry : checked.diagnostics.issues)
      entry = node_function_error(std::move(entry), this->node);
    this->value.reset();
    if (checked.value)
      this->value.emplace(std::move(*checked.value));
    this->result.append(std::move(checked).transform(
      omni::fn::default_value<compat::monostate>{}));
  }

  template <typename Check>
  void check(const Check &, std::false_type) {
    if (!this->value || !this->result.result.value || this->result.stopped())
      return;

    // Only a matching name reaches here: this is invalid configuration,
    // rather than probing an unrelated field's validator during dispatch.
    assert(false && "Field validator cannot accept the configured field type");
    this->value.reset();
    this->result.report(core::make_issue(json::issue_code::invalid_scalar,
      json::value_kind::unknown, this->node));
  }

  auto finish() && -> with_diagnostics_t<T> {
    return {
      this->result.result.value ? std::move(this->value) : compat::nullopt,
      std::move(this->result.result.diagnostics),
    };
  }
};

// A callable may accept an unrelated scalar by implicit conversion. Its
// result must also preserve this field's type before its body is instantiated.
template <typename Check, typename T, typename = void>
struct is_validator_for: std::false_type {};

template <typename Check, typename T>
struct is_validator_for<Check, T, compat::void_t<typename compat::decay_t<
    compat::invoke_result_t<Check, T &&>>::value_type>>:
    std::is_same<T, typename compat::decay_t<
      compat::invoke_result_t<Check, T &&>>::value_type> {};

template <typename Frame, typename Field>
struct validate_field_group {
  Frame *frame;
  Field field;

  template <typename Group, std::size_t... Index>
  void apply(const Group &group, compat::index_sequence<Index...>) const {
    // Initializer-list evaluation preserves validator order before C++17.
    const int checked[] = {0,
      (this->frame->check(std::get<Index + 1>(group),
        is_validator_for<decltype(std::get<Index + 1>(group)),
          typename Frame::value_type>{}), 0)...};
    (void)checked;
  }

  template <typename Group>
  void apply_selected(const Group &, std::false_type) const {}

  template <typename Group>
  void apply_selected(const Group &group, std::true_type) const {
    this->apply(group,
      compat::make_index_sequence<std::tuple_size<Group>::value - 1>{});
  }

  template <typename Group>
  void select(const Group &group, std::true_type) const {
    using key_type = typename std::tuple_element<0, Group>::type;
    this->apply_selected(group,
      compat::bool_constant<key_type::equals(Field{}.name())>{});
  }

  template <typename Group>
  void select(const Group &group, std::false_type) const {
    if (compat::string_view{this->field.name()}
      == compat::string_view{std::get<0>(group)})
      this->apply_selected(group, std::true_type{});
  }

  template <typename Group>
  void operator()(const Group &group) const {
    this->select(group, compat::bool_constant<traits::is_specialization_of<
      omni::cpp11_static_string, decltype(std::get<0>(group))>()>{});
  }
};

template <typename State, typename Field, typename T>
auto validate_record_field(State, ::ryml::ConstNodeRef,
  Field, with_diagnostics_t<T> mapped, const std::tuple<> &)
  -> with_diagnostics_t<T> {
  return mapped;
}

template <typename State, typename Field, typename T, typename Groups>
auto validate_record_field(State state, ::ryml::ConstNodeRef node,
  Field field, with_diagnostics_t<T> mapped,
  const Groups &groups) -> with_diagnostics_t<T> {
  validation_frame<State, T> frame{
    make_node_accumulator(std::move(state)), std::move(mapped.value), node};
  frame.result.append(std::move(mapped).transform(
    omni::fn::default_value<compat::monostate>{}));
  groups | omni::fn::each(
    validate_field_group<decltype(frame), Field>{&frame, field});
  return std::move(frame).finish();
}

template <typename State, typename T>
auto finalize_record(State, ::ryml::ConstNodeRef,
  with_diagnostics_t<T> mapped, const omni::fn::identity &)
  -> with_diagnostics_t<T> {
  return mapped;
}

template <typename State, typename T, typename Finalize>
auto finalize_record(State state, ::ryml::ConstNodeRef node,
  with_diagnostics_t<T> mapped, const Finalize &finalize)
  -> with_diagnostics_t<T> {
  validation_frame<State, T> frame{
    make_node_accumulator(std::move(state)), std::move(mapped.value), node};
  frame.result.append(std::move(mapped).transform(
    omni::fn::default_value<compat::monostate>{}));
  frame.check(finalize, std::true_type{});
  return std::move(frame).finish();
}

template <typename State, typename T>
auto finalize_record_into(State, ::ryml::ConstNodeRef,
  with_diagnostics_t<compat::monostate> mapped, T *,
  const omni::fn::identity &) -> with_diagnostics_t<compat::monostate> {
  return mapped;
}

template <typename State, typename T, typename Finalize>
auto finalize_record_into(State state, ::ryml::ConstNodeRef node,
  with_diagnostics_t<compat::monostate> mapped, T *into,
  const Finalize &finalize) -> with_diagnostics_t<compat::monostate> {
  static_assert(std::is_move_constructible<T>::value
      && std::is_assignable<T &, T &&>::value,
    "Finalizing an existing record requires move construction and assignment");
  if (!mapped.value)
    return mapped;

  auto checked = finalize_record(std::move(state), node,
    with_diagnostics_t<T>{compat::make_optional<T>(std::move(*into)),
      std::move(mapped.diagnostics)}, finalize);
  return std::move(checked).transform([into](T value) -> compat::monostate {
    *into = std::move(value);
    return {};
  });
}

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
template <typename Record, std::size_t Index, bool CheckPresence,
  typename Frame>
struct cpp11_record_member_event {
  Frame &frame;

  // Defined after type-mapping selection is available.
  bool operator()() const;
};

// Tuple indices keep field values and selected nodes paired without a search.
template <typename Record, typename State, typename Values,
  typename Strategy, typename Finalizer, std::size_t... Index>
auto process_record_members(node_accumulator<State> *result,
  ::ryml::ConstNodeRef object, Values *values,
  const std::array<::ryml::id_type, sizeof...(Index)> &pending,
  const Strategy &strategy, Finalizer finalizer,
  compat::index_sequence<Index...>)
  -> typename Finalizer::result_type {
  struct {
    node_accumulator<State> *result;
    ::ryml::ConstNodeRef object;
    Values &values;
    const std::array<::ryml::id_type, sizeof...(Index)> &pending;
    const Strategy &strategy;
    compat::optional<typename Finalizer::result_type::value_type> *value;
  } frame{result, object, *values, pending, strategy, nullptr};

  const auto events = std::tuple_cat(
    std::make_tuple(
      cpp11_record_member_event<Record, Index, true,
        decltype(frame)>{frame}...),
    std::make_tuple(
      cpp11_record_member_event<Record, Index, false,
        decltype(frame)>{frame}...));

  auto folded = events | omni::fn::try_fold(cpp11_process_record_event{},
    compat::expected<compat::monostate, compat::monostate>{
      compat::monostate{}});
  if (!result->result.value)
    return {compat::nullopt, std::move(result->result.diagnostics)};

  // The output starts after traversal and remains unaliased during the fold.
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
    core::write_node(
      this->context.template field<compat::remove_cvref_t<Record>>(
        omni::data_member_meta_t<_F>{}),
      field.value(),
      child);
  }
};

} // namespace detail

namespace reflected_scope {
namespace detail {

struct record_field_names {
  template <typename... Fields>
  constexpr auto operator()(const Fields &...fields) const
    -> std::array<compat::string_view, sizeof...(Fields)> {
    return {{compat::string_view{fields.name()}...}};
  }
};

template <typename T, typename Strategy>
constexpr auto readable_record_fields()
  -> decltype(omni::meta_for<T>::accessible_data_members()
    | omni::fn::filter(omni::fn::is_non_static)
    | omni::fn::filter(decltype(std::declval<Strategy>().filter_fields){})) {
  using filter_type = decltype(std::declval<Strategy>().filter_fields);
  return omni::meta_for<T>::accessible_data_members()
    | omni::fn::filter(omni::fn::is_non_static)
    | omni::fn::filter(filter_type{});
}

template <typename T, typename Strategy>
constexpr bool selects_all_record_fields() {
  return std::tuple_size<decltype(readable_record_fields<T, Strategy>())>::value
    == std::tuple_size<decltype(omni::meta_for<T>::accessible_data_members()
      | omni::fn::filter(omni::fn::is_non_static))>::value;
}

template <typename T, typename State, typename Strategy, typename Finalizer>
auto map_record(State context, ::ryml::ConstNodeRef node,
  const Strategy &strategy, Finalizer finalizer)
  -> typename Finalizer::result_type {
  auto result = omni::ryml::detail::make_node_accumulator(
    std::move(context));
  assert(node.is_map());
  assert(mapping::valid_record_strategy(compat::apply(record_field_names{},
    omni::meta_for<T>::accessible_data_members()), strategy));

#if 201402L <= OMNI_CPLUSPLUS
  constexpr auto fields = readable_record_fields<T, Strategy>();
  constexpr auto table = omni::fn::lookup_table(fields);
#else
  const auto fields = readable_record_fields<T, Strategy>();
  const auto table = omni::fn::lookup_table(fields);
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
      ? compat::decay_t<decltype(mapping::resolve_alias(
          strategy.aliases_by_name, input_name))>{}
      : mapping::resolve_alias(strategy.aliases_by_name, input_name);
    const auto field = std::move(direct).or_else([&table, &canonical](
      omni::fn::not_found_t<compat::string_view> missing) {
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
  return omni::ryml::detail::process_record_members<T>(&result, node, &values,
    pending, strategy, std::move(finalizer),
    compat::make_index_sequence<std::tuple_size<decltype(fields)>::value>{});
}

} // namespace detail

template <typename Strategy, typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (omni::is_reflected_record<T>::value
    && std::is_move_constructible<T>::value
    && std::is_default_constructible<T>::value
    && is_read_state<State>::value))
auto map_object_node_t<Strategy, WithNodeCheck>::operator()(State context,
  ::ryml::ConstNodeRef node, omni::type_t<T>) const
  -> with_diagnostics_t<T, diagnostics> {
  if (context.limit_exceeded())
    return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      context.config.error_policy, {}}, {}, {}}};

  if (this->with_node_check) {
    auto checked = core::expect_object_node(node);
    if (!checked)
      return {compat::nullopt,
        {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
          /*base=*/{},
#endif
          context.config.error_policy, {}}, {std::move(checked.error())}, {}}};
  }

  // Before C++17, aggregate_into's return requires a usable move constructor.
  const auto finalize = typename traits::select<
    traits::case_<omni::refl::is_aggregatable<T>::value
        && detail::selects_all_record_fields<T, Strategy>()
        && std::is_move_constructible<T>::value,
      detail::aggregate_record_value<T>>,
    detail::bind_record_value<T>>::type{};
  return omni::ryml::detail::finalize_record(context, node,
    detail::map_record<T>(context, node, this->strategy, finalize),
    this->strategy.finalize);
}

template <typename Strategy, typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (omni::is_reflected_record<T>::value
    && is_read_state<State>::value))
auto map_object_node_t<Strategy, WithNodeCheck>::operator()(State context,
  ::ryml::ConstNodeRef node, T *into) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  assert(into);
  if (context.limit_exceeded())
    return {compat::nullopt, {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
      /*base=*/{},
#endif
      context.config.error_policy, {}}, {}, {}}};

  if (this->with_node_check) {
    auto checked = core::expect_object_node(node);
    if (!checked)
      return {compat::nullopt,
        {{
#if defined(__cpp_aggregate_bases) && 201603L <= __cpp_aggregate_bases
          /*base=*/{},
#endif
          context.config.error_policy, {}}, {std::move(checked.error())}, {}}};
  }

  // Before C++17, aggregate_into's return requires a usable move constructor.
  const auto finalize = typename traits::select<
    traits::case_<omni::refl::is_aggregatable<T>::value
        && detail::selects_all_record_fields<T, Strategy>()
        && std::is_move_constructible<T>::value
        && std::is_assignable<T &, T &&>::value,
      detail::aggregate_record_into<T>>,
    detail::bind_record_into<T>>::type{into};
  return omni::ryml::detail::finalize_record_into(context, node,
    detail::map_record<T>(context, node, this->strategy, finalize), into,
    this->strategy.finalize);
}

template <typename Strategy>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_record<T>,
    is_write_state<State>>::value))
auto map_record_t<Strategy>::operator()(State context,
  const T &value,
  ::ryml::NodeRef node) const
  -> void {
  assert(mapping::valid_record_strategy(
    compat::apply(detail::record_field_names{},
      omni::meta_for<T>::accessible_data_members()), this->strategy));
  node.set_map();
  const auto binding = omni::meta_for<T>::bind(value);
  binding.accessible_data_members()
    | omni::fn::filter(this->strategy.filter_fields)
    | omni::fn::each(omni::ryml::detail::cpp11_write_member<State>{
      /*node=*/node,
      /*context=*/std::move(context),
    });
}

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
