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

using omni::ryml::detail::node_call_priority;

// Deduce the opaque field tag through its public metadata wrapper.
template <typename Record, typename _F>
constexpr omni::data_member_binding_t<Record, _F> bind_record_member(
  Record *into, omni::data_member_meta_t<_F>) {
  return omni::data_member_binding_t<Record, _F>{*into};
}

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

    if (!succeeded || !mapped.diagnostics.parse_error_message.empty())
      result->append({
        /*value=*/succeeded ? compat::make_optional(compat::monostate{})
          : compat::nullopt,
        /*diagnostics=*/std::move(mapped.diagnostics),
      });
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

// Reference mutations need no subsequent setter, so the parent can normalize
// their report directly rather than repeating it in an intermediate frame.
template <typename Mapping, typename Field, typename State>
auto process_node_to_field_using(
  omni::ryml::detail::node_accumulator<State> *result,
  const Mapping &mapping,
  ::ryml::ConstNodeRef node,
  Field field,
  node_call_priority<1>)
  -> decltype(result->append(mapping(std::declval<State>(), node,
    std::addressof(field.ref())))) {
  auto state = result->state;
  auto mapped = mapping(std::move(state), node, std::addressof(field.ref()));
  if (!mapped.value || !mapped.diagnostics.issues.empty()
    || !mapped.diagnostics.parse_error_message.empty() || result->stopped())
    result->append(std::move(mapped));
}

// Bitfields and unaligned packed fields cannot supply a safe reference.
// Normalize before the setter so warnings and errors still control assignment.
template <typename Mapping, typename Field, typename State>
void process_node_to_field_using(
  omni::ryml::detail::node_accumulator<State> *result,
  const Mapping &mapping, ::ryml::ConstNodeRef node, Field field,
  node_call_priority<0>) {
  auto state = result->state;
  typename Field::type value = field.value();
  auto checked = omni::ryml::detail::make_node_accumulator(state);
  checked.append(omni::ryml::detail::read_into(std::move(state),
    mapping,
    node,
    &value,
    /*priority=*/0));

  if (checked.result.value)
    field.set_value(std::move(value));

  result->append(std::move(checked).finish());
}

} // namespace detail
} // namespace reflected_scope

namespace detail {

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
  ::ryml::ConstNodeRef object, ::ryml::id_type node_id, Field field) {
  if (node_id != ::ryml::NONE)
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
  missing.path.push_back({c4::to_csubstr(field.name()), compat::nullopt});
  result->report(std::move(missing));
  return !result->stopped();
}

// Only continuation crosses the fold; diagnostics stay in the local frame.
struct cpp11_process_record_event {
  template <typename Event>
  auto operator()(compat::monostate, const Event &event) const
    -> compat::expected<compat::monostate, compat::monostate> {
    if (event())
      return {};

    return compat::unexpected<compat::monostate>{compat::monostate{}};
  }
};

// GCC 7 rejects pack expansions found only inside lambda bodies. Indexed
// callables retain the same reference capture without that expansion.
template <std::size_t Index, bool CheckPresence, typename Frame>
struct cpp11_record_member_event {
  Frame &frame;

  bool operator()() const {
    if (CheckPresence)
      return process_record_presence(this->frame.result, this->frame.object,
        this->frame.pending[Index], reflected_scope::detail::bind_record_member(
          this->frame.into, std::get<Index>(this->frame.fields)));

    if (this->frame.pending[Index] != ::ryml::NONE)
      reflected_scope::detail::map_node_to_field(this->frame.result,
        {this->frame.object.tree(), this->frame.pending[Index]},
        reflected_scope::detail::bind_record_member(
          this->frame.into, std::get<Index>(this->frame.fields)));

    return !this->frame.result->stopped();
  }
};

// Tuple indices keep bindings and selected nodes paired without another search.
template <typename State, typename Record, typename Fields, std::size_t... Index>
void process_record_members(node_accumulator<State> *result,
  ::ryml::ConstNodeRef object, Record *into, const Fields &fields,
  const std::array<::ryml::id_type, sizeof...(Index)> &pending,
  compat::index_sequence<Index...>) {
  // Each event references one shared frame instead of repeating its captures.
  // This also keeps empty-record instantiations warning-free.
  struct {
    node_accumulator<State> *result;
    ::ryml::ConstNodeRef object;
    Record *into;
    const Fields &fields;
    const std::array<::ryml::id_type, sizeof...(Index)> &pending;
  } frame{result, object, into, fields, pending};
  (void)frame;

  const auto events = std::tuple_cat(
    std::make_tuple(
      cpp11_record_member_event<Index, true, decltype(frame)>{frame}...),

    std::make_tuple(
      cpp11_record_member_event<Index, false, decltype(frame)>{frame}...));

  // The frame retains failures; this result only controls further traversal.
  static_cast<void>(events | fn::try_fold(cpp11_process_record_event{},
    compat::expected<compat::monostate, compat::monostate>{}));
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
      ? compat::optional<compat::string_view>{}
      : compat::invoke(this->name_by_alias, input_name);
    const auto field = std::move(direct).or_else([&table, &canonical](
      fn::not_found_t<compat::string_view> missing) {
      if (canonical)
        return table.lookup(*canonical);

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
    return std::move(result).finish();

  omni::ryml::detail::process_record_members(&result, node, into, fields, pending,
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
