#pragma once

#include <omnirefl/functional.hpp>

#include <algorithm>
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

using omni::ryml::detail::node_call_priority;

// Prefer value production and field assignment. The trailing decltype
// requires a native type-tag call; assignment also needs a writable value.
OMNI_CONSTRAINED_TEMPLATE((typename Mapping, typename Field, typename State),
  (std::is_assignable<typename Field::type &,
    typename Field::type &&>::value))
auto process_node_to_field_using(
  omni::ryml::detail::node_accumulator<State> *result,
  const Mapping &mapping,
  ::ryml::ConstNodeRef node,
  Field field,
  node_call_priority<2>)
  -> decltype(field.set_value(std::move(*mapping(std::declval<State>(),
    node, omni::type_t<typename Field::type>{}).value))) {
  const auto error_policy = result->state.config.error_policy;
  auto state = result->state;
  auto mapped = mapping(std::move(state),
    node, omni::type_t<typename Field::type>{});

  if (mapped.diagnostics.issues.empty()) {
    const bool succeeded = mapped.value && !result->stopped();
    if (succeeded)
      field.set_value(std::move(*mapped.value));

    result->append(std::move(mapped.diagnostics), succeeded);
    return;
  }

  auto checked = omni::ryml::detail::make_node_accumulator(result->state);
  checked.result.diagnostics.config.error_policy = error_policy;
  checked.append({
    /*value=*/mapped.value ? compat::make_optional(compat::monostate{})
      : compat::nullopt,
    /*diagnostics=*/std::move(mapped.diagnostics),
  });
  if (checked.result.value)
    field.set_value(std::move(*mapped.value));

  result->append(std::move(checked).finish());
}

// Pointer-only mappings use the existing member when it has a safe reference.
// This avoids copying containers and retains inputs bound in custom members.
template <typename Mapping, typename Field, typename State>
auto map_node_to_field_using(State state,
  const Mapping &mapping,
  ::ryml::ConstNodeRef node,
  Field field,
  node_call_priority<1>)
  -> decltype(mapping(std::move(state), node, std::addressof(field.ref()))) {
  auto result = omni::ryml::detail::make_node_accumulator(state);
  result.append(mapping(std::move(state), node, std::addressof(field.ref())));
  return std::move(result).finish();
}

template <typename Mapping, typename Field, typename State>
auto map_node_to_field_using(State state,
  const Mapping &mapping,
  ::ryml::ConstNodeRef node,
  Field field,
  node_call_priority<0>) -> with_diagnostics_t<compat::monostate> {
  // Bitfields and unaligned packed fields cannot supply a reference.
  typename Field::type value = field.value();
  auto result = omni::ryml::detail::make_node_accumulator(state);
  result.append(omni::ryml::detail::read_into(std::move(state),
    mapping,
    node,
    &value,
    /*priority=*/0));

  if (result.result.value)
    field.set_value(std::move(value));

  return std::move(result).finish();
}

// Mutation-only mappings retain their existing reference/copy adaptation.
template <typename Mapping, typename Field, typename State>
void process_node_to_field_using(
  omni::ryml::detail::node_accumulator<State> *result,
  const Mapping &mapping, ::ryml::ConstNodeRef node, Field field,
  node_call_priority<0>) {
  result->append(map_node_to_field_using(result->state,
    mapping, node, field, node_call_priority<1>{}));
}

} // namespace detail
} // namespace reflected_scope

namespace detail {

// The document scan selects nodes directly. Only aliases need a separate
// canonical-presence query, cached so repeated aliases do not repeat lookup.
struct record_entry {
  compat::string_view name;
  ::ryml::ConstNodeRef node;
  compat::optional<bool> canonical_presence;
};

// C++11 lambda equivalent: initialize the fixed field table in schema order.
struct cpp11_collect_members {
  template <typename... Fields>
  std::array<record_entry, sizeof...(Fields)> operator()(
    const Fields &...fields) const {
    return {{record_entry{
      /*name=*/fields.name(),
      /*node=*/{},
      /*canonical_presence=*/compat::nullopt,
    }...}};
  }
};

OMNI_CONSTRAINED_TEMPLATE((typename Field, typename T = typename Field::type),
  (traits::is_optional<T>::value && std::is_default_constructible<T>::value
    && std::is_assignable<T &, T &&>::value))
void initialize_missing(Field field) {
  field.set_value(T{});
}

// Nonassignable optionals can still be emptied through their reset adapter.
OMNI_CONSTRAINED_TEMPLATE((typename Field, typename T = typename Field::type),
  (traits::is_optional<T>::value && std::is_default_constructible<T>::value
    && !std::is_assignable<T &, T &&>::value))
void initialize_missing(Field field) {
  adapt::reset(std::addressof(field.ref()));
}

OMNI_CONSTRAINED_TEMPLATE((typename Field, typename T = typename Field::type),
  (!traits::is_optional<T>::value || !std::is_default_constructible<T>::value))
void initialize_missing(Field) {}

template <typename State, typename Field>
bool process_record_presence(node_accumulator<State> *result,
  ::ryml::ConstNodeRef object, const record_entry &entry, Field field) {
  if (!entry.node.invalid())
    return true;

  using member_type = typename Field::type;
  if (traits::is_optional<member_type>::value
    && std::is_default_constructible<member_type>::value) {
    initialize_missing(field);
    return !result->stopped();
  }

  // The absent field has no node; derive its path from the object.
  auto missing = core::make_issue(json::issue_code::missing_field,
    json::value_kind::unknown, ::ryml::ConstNodeRef{});

  missing.path = core::node_path(object);
  missing.path.push_back({{entry.name.data(), entry.name.size()},
    compat::nullopt});
  result->report(std::move(missing));
  return !result->stopped();
}

// Only the stop flag crosses the fold; diagnostics stay in the local frame.
struct cpp11_process_record_event {
  template <typename Event>
  fn::fold_control<bool> operator()(bool, const Event &event) const {
    return event() ? fn::continue_with(true) : fn::stop_with(false);
  }
};

// Tuple indices keep bindings and selected nodes paired without another name
// search. Parameterless reference-capturing lambdas also work in C++11.
template <typename State, typename Members, std::size_t... Index>
void process_record_members(node_accumulator<State> *result,
  ::ryml::ConstNodeRef object, const Members &members,
  const std::array<record_entry, sizeof...(Index)> &pending,
  compat::index_sequence<Index...>) {
  // Every closure references one shared frame instead of repeating its four
  // bindings. This also keeps empty-record instantiations warning-free.
  struct {
    node_accumulator<State> *result;
    ::ryml::ConstNodeRef object;
    const Members &members;
    const std::array<record_entry, sizeof...(Index)> &pending;
  } frame{result, object, members, pending};
  (void)frame;

  const auto events = std::tuple_cat(
    std::make_tuple([&frame]() -> bool {
      return process_record_presence(frame.result, frame.object,
        frame.pending[Index], std::get<Index>(frame.members));
    }...),

    std::make_tuple([&frame]() -> bool {
      if (!frame.pending[Index].node.invalid())
        reflected_scope::detail::map_node_to_field(frame.result,
          frame.pending[Index].node, std::get<Index>(frame.members));

      return !frame.result->stopped();
    }...));

  events | fn::try_fold(cpp11_process_record_event{}, fn::continue_with(true));
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

template <typename NameByAlias>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_record<T>,
    is_read_state<State>>::value))
auto to_record_t<NameByAlias>::operator()(State context,
  ::ryml::ConstNodeRef node,
  T *into) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  auto result = omni::ryml::detail::make_node_accumulator(
    std::move(context));
  assert(into && node.is_map());

  // C++11 tuple forwarding constructors need not support constant evaluation.
#if 201402L <= OMNI_CPLUSPLUS
  constexpr auto fields = omni::meta_for<T>::accessible_data_members()
    | fn::filter(fn::is_non_static);
#else
  const auto fields = omni::meta_for<T>::accessible_data_members()
    | fn::filter(fn::is_non_static);
#endif

  auto pending = compat::apply(omni::ryml::detail::cpp11_collect_members{},
    fields);

  // Structural issues precede missing fields and value mapping, preserving
  // diagnostic order regardless of the document's member order.
  for (const auto child : node.children()) {
    if (result.stopped())
      break;

    const auto name = child.key();
    const compat::string_view input_name{name.str, name.len};
    auto entry = std::find_if(pending.begin(),
      pending.end(),
      [input_name](const omni::ryml::detail::record_entry &entry) {
        return input_name == entry.name;
      });

    if (entry == pending.end()) {
      if (const auto canonical =
        compat::invoke(this->name_by_alias, input_name)) {
        entry = std::find_if(pending.begin(),
          pending.end(),
          [canonical](const omni::ryml::detail::record_entry &entry) {
            return *canonical == entry.name;
          });
      }
      if (entry != pending.end()) {
        if (!entry->canonical_presence)
          entry->canonical_presence = !node.find_child(
            {entry->name.data(), entry->name.size()}).invalid();

        if (*entry->canonical_presence)
          continue;
      }
    }

    const bool known = entry != pending.end();
    const bool duplicate = known && !entry->node.invalid();

    if (known && !duplicate) {
      entry->node = child;
      continue;
    }

    const auto duplicates = result.state.config.error_policy.duplicates;
    if (duplicate
      && (duplicates & json::duplicate_policy::last)
        == json::duplicate_policy::last)
      entry->node = child;

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
    return std::move(result).finish();

  const auto binding = omni::meta_for<T>::bind(*into);
  const auto members = binding.accessible_data_members()
    | fn::filter(fn::is_non_static);

  omni::ryml::detail::process_record_members(&result, node, members, pending,
    compat::make_index_sequence<std::tuple_size<decltype(fields)>::value>{});

  return std::move(result).finish();
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
