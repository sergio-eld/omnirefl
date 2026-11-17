#pragma once

#include <omnirefl/functional.hpp>

#include <algorithm>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace omni {
namespace ryml {
namespace detail {

// Pending members retain their selected input node. Canonical presence is
// known before classification, so aliases can be ignored regardless of order.
struct record_entry {
  compat::string_view name;
  ::ryml::ConstNodeRef node;
  bool has_canonical;
};

// C++11 lambda equivalent: prepare each member's canonical candidate.
struct cpp11_collect_members {
  const ::ryml::ConstNodeRef &node;
  std::vector<record_entry> &pending;

  template <typename _F>
  void operator()(omni::data_member_meta_t<_F> field) const {
    const auto canonical = this->node.find_child(c4::to_csubstr(field.name()));
    this->pending.push_back({
      /*name=*/field.name(),
      /*node=*/canonical,
      /*has_canonical=*/!canonical.invalid(),
    });
  }
};

// SFINAE keeps T{} out of instantiations for non-default-constructible fields.
OMNI_CONSTRAINED_TEMPLATE((typename Record, typename _F),
  (std::is_default_constructible<
    typename omni::data_member_meta_t<_F>::type>::value))
void initialize_missing(omni::data_member_binding_t<Record, _F> field) {
  field.set_value(typename omni::data_member_meta_t<_F>::type{});
}

OMNI_CONSTRAINED_TEMPLATE((typename Record, typename _F),
  (!std::is_default_constructible<
    typename omni::data_member_meta_t<_F>::type>::value))
void initialize_missing(omni::data_member_binding_t<Record, _F>) {}

// A bound member operation is inert until the fold supplies its current state.
template <typename Record, typename _F>
struct record_member_event {
  omni::data_member_binding_t<Record, _F> field;
  const std::vector<record_entry> &pending;
  bool check_presence;

  OMNI_CONSTRAINED_TEMPLATE((typename State), (is_read_state<State>::value))
  auto operator()(State state) const
    -> with_diagnostics_t<compat::monostate, diagnostics> {
    const compat::string_view name{this->field.name()};
    const auto entry = std::find_if(this->pending.begin(), this->pending.end(),
      [name](const record_entry &entry) { return name == entry.name; });

    const auto node = entry == this->pending.end()
      ? ::ryml::ConstNodeRef{} : entry->node;

    if (!this->check_presence && !node.invalid()) {
      const auto child = state.child({
        /*field=*/node.key(),
        /*index=*/compat::nullopt,
      });

      // A temporary also supports bitfields, which cannot expose references.
      typename omni::data_member_meta_t<_F>::type value = this->field.value();
      auto mapped = core::read_node(node, &value, child);
      if (mapped.value) {
        // Setters need a mutable binding; its copy refers to the same record.
        auto field = this->field;
        field.set_value(std::move(value));
      }

      return mapped;
    }

    auto result = make_node_accumulator(state);
    if (!this->check_presence || !node.invalid())
      return std::move(result).finish();

    using member_type = typename omni::data_member_meta_t<_F>::type;
    if (traits::is_optional<member_type>::value
      && std::is_default_constructible<member_type>::value) {
      initialize_missing(this->field);
      return std::move(result).finish();
    }

    const auto child = state.child({
      /*field=*/c4::to_csubstr(this->field.name()),
      /*index=*/compat::nullopt,
    });

    result.report(make_issue(json::issue_code::missing_field,
      json::value_kind::unknown, ::ryml::ConstNodeRef{}, child.path));
    return std::move(result).finish();
  }
};

// C++11 lambda equivalent: defer node selection until the fold runs the event.
struct cpp11_defer_member {
  const std::vector<record_entry> &pending;
  bool check_presence;

  template <typename Record, typename _F>
  record_member_event<Record, _F> operator()(
    omni::data_member_binding_t<Record, _F> field) const {
    return {
      /*field=*/field,
      /*pending=*/this->pending,
      /*check_presence=*/this->check_presence,
    };
  }
};

// C++11 lambda equivalent: own diagnostics and budget accounting in the fold.
struct cpp11_fold_member {
  template <typename State, typename Event>
  fn::fold_control<node_accumulator<State>> operator()(
    node_accumulator<State> result, const Event &event) const {
    result.append(event(result.state));
    return result.stopped() ? fn::stop_with(std::move(result))
                            : fn::continue_with(std::move(result));
  }
};

template <typename State>
struct cpp11_write_member {
  ::ryml::NodeRef &node;
  State context;

  template <typename Record, typename _F>
  void operator()(omni::data_member_binding_t<Record, _F> field) const {
    auto child = this->node.append_child();
    child.set_key_serialized(field.name());
    core::write_node(field.value(), child, this->context);
  }
};

} // namespace detail

namespace reflected_scope {

template <typename NameByAlias>
OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_record<T>,
    is_read_state<State>>::value))
auto to_record_t<NameByAlias>::operator()(::ryml::ConstNodeRef node,
  T *into,
  State context) const
  -> with_diagnostics_t<compat::monostate, diagnostics> {
  auto result = omni::ryml::detail::make_node_accumulator(context);
  if (!node.is_map()) {
    result.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      json::value_kind::mapping,
      node,
      context.path));

    return std::move(result).finish();
  }

  // C++11 tuple forwarding constructors need not support constant evaluation.
#if 201402L <= OMNI_CPLUSPLUS
  constexpr auto fields = omni::meta_for<T>::accessible_data_members()
    | fn::filter(fn::is_non_static);
#else
  const auto fields = omni::meta_for<T>::accessible_data_members()
    | fn::filter(fn::is_non_static);
#endif

  std::vector<omni::ryml::detail::record_entry> pending;
  pending.reserve(std::tuple_size<decltype(fields)>::value);
  fields | fn::each(omni::ryml::detail::cpp11_collect_members{
    /*node=*/node,
    /*pending=*/pending,
  });

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
      if (entry != pending.end() && entry->has_canonical)
        continue;
    }

    const bool known = entry != pending.end();
    const bool duplicate = known && !entry->node.invalid()
      && entry->node.id() != child.id();

    if (known && !duplicate) {
      entry->node = child;
      continue;
    }

    const auto duplicates = context.config.error_policy.duplicates;
    if (duplicate
      && (duplicates & json::duplicate_policy::last)
        == json::duplicate_policy::last)
      entry->node = child;

    const auto unknown = context.config.error_policy.unknown_fields;
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

    const auto child_context = result.state.child({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });

    result.report(omni::ryml::detail::make_issue(
      duplicate ? json::issue_code::duplicate_field
                : json::issue_code::unknown_field,
      json::value_kind::unknown,
      child,
      child_context.path,
      /*warning=*/allowed));
  }

  if (result.stopped())
    return std::move(result).finish();

  const auto binding = omni::meta_for<T>::bind(*into);
  const auto members = binding.accessible_data_members()
    | fn::filter(fn::is_non_static);

  const auto events = fn::concat(
    members | fn::map(omni::ryml::detail::cpp11_defer_member{
      /*pending=*/pending,
      /*check_presence=*/true,
    }),

    members | fn::map(omni::ryml::detail::cpp11_defer_member{
      /*pending=*/pending,
      /*check_presence=*/false,
    }));

  auto folded = events | fn::try_fold(omni::ryml::detail::cpp11_fold_member{},
    fn::continue_with(std::move(result)));

  return std::move(folded.value).finish();
}

OMNI_CONSTRAINED_TEMPLATE_REDECL((typename T, typename State),
  (compat::conjunction<omni::is_reflected_record<T>,
    is_write_state<State>>::value))
auto from_record_t::operator()(const T &value,
  ::ryml::NodeRef node,
  State context) const
  -> void {
  node.set_map();
  const auto binding = omni::meta_for<T>::bind(value);
  binding.accessible_data_members()
    | fn::filter(fn::is_non_static)
    | fn::each(omni::ryml::detail::cpp11_write_member<State>{
      /*node=*/node,
      /*context=*/context,
    });
}

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
