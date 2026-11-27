#pragma once

#include <omnirefl/extensions/static_string.hpp>
#include <omnirefl/functional.hpp>
#include <omnirefl/reflected_scope.hpp>
#include <omnirefl/serialization/ryml/core.hpp>
#include <omnirefl/serialization/ryml/impl/compose.hpp>
#include <omnirefl/serialization/ryml/impl/shared.hpp>
#include <omnirefl/serialization/ryml/with_diagnostics.hpp>

#include <array>
#include <cassert>
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

// Flat slots provide lookup by metadata type; index tags select their bases
// without instantiating recursive tuple_element chains.
template <std::size_t Index, typename Field>
struct record_field_index {};

template <std::size_t Index, typename Field>
omni::type_t<Field> record_field_at(record_field_index<Index, Field>);

template <typename Indices, typename... Field>
struct record_value_slots_base;

template <std::size_t... Index, typename... Field>
struct record_value_slots_base<compat::index_sequence<Index...>, Field...>:
    record_value_slot<Field>..., record_field_index<Index, Field>... {};

template <typename... Field>
struct record_value_slots: record_value_slots_base<
    compat::make_index_sequence<sizeof...(Field)>, Field...> {
  template <typename Tag>
  typename omni::data_member_meta_t<Tag>::type take() {
    using field = omni::data_member_meta_t<Tag>;
    return static_cast<typename field::type>(std::move(static_cast<
      record_value_slot<field> &>(*this)).value());
  }
};

template <typename Values>
struct record_aggregate_source {
  Values *values;
};

// Ad hoc: aggregate_into's existing lookup accepts tuples only. One pointer
// adapts private flat slots without extending the public get overload set.
// Replace this carrier when an internal aggregate-storage protocol is agreed.
template <typename Values>
auto record_aggregate_fields(Values *values)
  -> std::tuple<record_aggregate_source<Values>> {
  return {record_aggregate_source<Values>{values}};
}

template <typename T, typename... Field>
void assign_record_fields(T *into, record_value_slots<Field...> *values) {
  // Initializer-list evaluation preserves field order before C++17.
  const int assigned[] = {0,
    (bind_record_member(into, static_cast<Field>(
        static_cast<record_value_slot<Field> &>(*values)))
      .set_value(std::move(
        static_cast<record_value_slot<Field> &>(*values)).value()), 0)...};
  static_cast<void>(assigned);
}

// A conversion result lets optional construct T directly; forwarding an
// already-constructed T through its constructor would require another move.
template <typename T, typename Frame>
struct aggregate_record_constructor {
  Frame *frame;

  operator T() && {
    return omni::refl::aggregate_into<T>(record_aggregate_fields(
      std::addressof(this->frame->values)));
  }
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
    assign_record_fields(std::addressof(frame->value->emplace()),
      std::addressof(frame->values));
  }
};

template <typename T>
struct aggregate_record_into {
  using result_type = with_diagnostics_t<compat::monostate>;
  T *into;

  template <typename Frame>
  void operator()(Frame *frame) const {
    *this->into = omni::refl::aggregate_into<T>(record_aggregate_fields(
      std::addressof(frame->values)));
    frame->value->emplace();
  }
};

template <typename T>
struct bind_record_into {
  using result_type = with_diagnostics_t<compat::monostate>;
  T *into;

  template <typename Frame>
  void operator()(Frame *frame) const {
    assign_record_fields(this->into, std::addressof(frame->values));
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

template <std::size_t Index, typename Field, typename Frame>
bool process_record_presence_event(Frame *frame) {
  return process_record_presence(frame->result, frame->object,
    frame->pending[Index], std::addressof(static_cast<
      reflected_scope::detail::record_value_slot<Field> &>(frame->values)));
}

// Defined after type-mapping selection is available.
template <typename Record, std::size_t Index, typename Field, typename Frame>
bool process_record_member(Frame *frame);

// Index tags retain the expected-like fold without indexing a field tuple.
template <typename Record, typename Frame, bool CheckPresence>
struct cpp11_process_record_event {
  Frame *frame;

  template <std::size_t Index>
  auto operator()(compat::monostate,
    std::integral_constant<std::size_t, Index>) const
    -> compat::expected<compat::monostate, compat::monostate> {
    if (this->template process<Index>(compat::bool_constant<CheckPresence>{}))
      return compat::monostate{};

    return compat::unexpected<compat::monostate>{compat::monostate{}};
  }

  template <std::size_t Index>
  bool process(std::true_type) const {
    using field = typename decltype(reflected_scope::detail::record_field_at<
      Index>(this->frame->values))::type;
    return process_record_presence_event<Index, field>(this->frame);
  }

  template <std::size_t Index>
  bool process(std::false_type) const {
    using field = typename decltype(reflected_scope::detail::record_field_at<
      Index>(this->frame->values))::type;
    return process_record_member<Record, Index, field>(this->frame);
  }
};

// Field types select value slots; indices pair them with the selected nodes.
template <typename Record, typename State, typename... Field,
  typename Policy, typename Finalizer, std::size_t... Index>
auto process_record_members(node_accumulator<State> *result,
  ::ryml::ConstNodeRef object,
  reflected_scope::detail::record_value_slots<Field...> *values,
  const std::array<::ryml::id_type, sizeof...(Index)> &pending,
  const Policy &policy, Finalizer finalizer,
  compat::index_sequence<Index...>)
  -> typename Finalizer::result_type {
  struct {
    node_accumulator<State> *result;
    ::ryml::ConstNodeRef object;
    reflected_scope::detail::record_value_slots<Field...> &values;
    const std::array<::ryml::id_type, sizeof...(Index)> &pending;
    const Policy &policy;
    compat::optional<typename Finalizer::result_type::value_type> *value;
  } frame{result, object, *values, pending, policy, nullptr};

  // Index-tag tuple types are shared by records of the same width.
  constexpr std::tuple<std::integral_constant<std::size_t, Index>...> events{};

  // Separate passes bound each fold's instantiation depth to the field count.
  auto checked = events | omni::fn::try_fold(
    cpp11_process_record_event<Record, decltype(frame), true>{&frame},
    compat::expected<compat::monostate, compat::monostate>{
      compat::monostate{}});
  auto folded = events | omni::fn::try_fold(
    cpp11_process_record_event<Record, decltype(frame), false>{&frame},
    std::move(checked));
  if (!result->result.value)
    return {compat::nullopt, std::move(result->result.diagnostics)};

  // The output starts after traversal and remains unaliased during the fold.
  compat::optional<typename Finalizer::result_type::value_type> value{};
  frame.value = &value;
  static_cast<void>(std::move(folded).transform(
    [&frame, &finalizer](compat::monostate) { finalizer(&frame); }));
  return {std::move(value), std::move(result->result.diagnostics)};
}

} // namespace detail

namespace reflected_scope {
namespace detail {

template <typename T>
using non_static_record_fields_t = typename filter_fields<
  compat::decay_t<decltype(omni::fn::is_non_static)>,
  typename omni::meta_for<T>::accessible_data_members_t>::type;

template <typename T, typename Policy>
using readable_record_fields_t = typename filter_fields<
  decltype(std::declval<Policy>().filter_fields),
  non_static_record_fields_t<T>>::type;

template <typename T, typename Policy>
constexpr bool selects_all_record_fields() {
  return std::tuple_size<readable_record_fields_t<T, Policy>>::value
    == std::tuple_size<non_static_record_fields_t<T>>::value;
}

template <typename T, typename State, typename Policy, typename Finalizer,
  typename... Field>
auto map_record_fields(State context, ::ryml::ConstNodeRef node,
  const Policy &policy, Finalizer finalizer,
  omni::type_t<std::tuple<Field...>>)
  -> typename Finalizer::result_type {
  auto result = omni::ryml::detail::make_node_accumulator(
    omni::ryml::detail::state_schema<T>::bind(context));
  assert(node.is_map());
  assert(mapping::valid_record_policy(compat::apply(record_field_names{},
    omni::meta_for<T>::accessible_data_members()), policy));

  // Ad hoc: retain the old predicate construction and filter constraints
  // without materializing every field. Readers require constexpr construction
  // from C++14; an internal type-filter protocol should replace this check.
  using filter_type = decltype(policy.filter_fields);
#if 201402L <= OMNI_CPLUSPLUS
  constexpr auto checked_filter = std::tuple<>{}
#else
  const auto checked_filter = std::tuple<>{}
#endif
    | omni::fn::filter(omni::fn::is_non_static)
    | omni::fn::filter(filter_type{});
  static_cast<void>(checked_filter);

#if 201402L <= OMNI_CPLUSPLUS
  constexpr auto table = omni::fn::lookup_table(Field{}...);
#else
  const auto table = omni::fn::lookup_table(Field{}...);
#endif

  // Selected children share this object's tree; only IDs need storage.
  std::array<::ryml::id_type, sizeof...(Field)> pending;
  pending.fill(::ryml::NONE);

  // The default no-alias resolver makes this cache unused and removable.
  std::array<compat::optional<bool>, sizeof...(Field)> canonical_presence{};

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
          policy.aliases_by_name, input_name))>{}
      : mapping::resolve_alias(policy.aliases_by_name, input_name);
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

  record_value_slots<Field...> values{};
  return omni::ryml::detail::process_record_members<T>(&result, node, &values,
    pending, policy, std::move(finalizer),
    compat::make_index_sequence<sizeof...(Field)>{});
}

template <typename T, typename State, typename Policy, typename Finalizer>
auto map_record(State context, ::ryml::ConstNodeRef node,
  const Policy &policy, Finalizer finalizer)
  -> typename Finalizer::result_type {
  return map_record_fields<T>(std::move(context), node, policy,
    std::move(finalizer),
    omni::type_t<readable_record_fields_t<T, Policy>>{});
}

} // namespace detail

template <typename Policy, typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (omni::is_reflected_record<T>::value
    && std::is_move_constructible<T>::value
    && std::is_default_constructible<T>::value
    && is_read_state<State>::value))
auto map_object_node_t<Policy, WithNodeCheck>::operator()(State context,
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
        && detail::selects_all_record_fields<T, Policy>()
        && std::is_move_constructible<T>::value,
      detail::aggregate_record_value<T>>,
    detail::bind_record_value<T>>::type{};
  return omni::ryml::detail::finalize_record(context, node,
    detail::map_record<T>(context, node, this->policy, finalize),
    this->policy.finalize);
}

template <typename Policy, typename WithNodeCheck>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (omni::is_reflected_record<T>::value
    && is_read_state<State>::value))
auto map_object_node_t<Policy, WithNodeCheck>::operator()(State context,
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
        && detail::selects_all_record_fields<T, Policy>()
        && std::is_move_constructible<T>::value
        && std::is_assignable<T &, T &&>::value,
      detail::aggregate_record_into<T>>,
    detail::bind_record_into<T>>::type{into};
  return omni::ryml::detail::finalize_record_into(context, node,
    detail::map_record<T>(context, node, this->policy, finalize), into,
    this->policy.finalize);
}

} // namespace reflected_scope
} // namespace ryml
} // namespace omni

namespace omni {
namespace refl {
namespace detail {

// Keep the private flat-slot adapter behind the existing tuple lookup.
template <typename TargetField, typename Values, typename Target>
struct _get_t<0, TargetField,
  std::tuple<ryml::reflected_scope::detail::record_aggregate_source<Values>>,
  Target, false> {
  static Target from(std::tuple<
    ryml::reflected_scope::detail::record_aggregate_source<Values>> &fields) {
    return std::get<0>(fields).values->template take<TargetField>();
  }
};

} // namespace detail
} // namespace refl
} // namespace omni
