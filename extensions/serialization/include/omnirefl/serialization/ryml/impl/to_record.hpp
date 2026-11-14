#pragma once

#include <omnirefl/functional.hpp>
#include <omnirefl/serialization/ryml/to_record.hpp>

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
template <typename Record, typename _F>
auto initialize_missing(omni::data_member_binding_t<Record, _F> field)
  -> compat::enable_if_t<std::is_default_constructible<
    typename omni::data_member_meta_t<_F>::type>::value> {
  field.set_value(typename omni::data_member_meta_t<_F>::type{});
}

template <typename Record, typename _F>
auto initialize_missing(omni::data_member_binding_t<Record, _F>)
  -> compat::enable_if_t<!std::is_default_constructible<
    typename omni::data_member_meta_t<_F>::type>::value> {}

// A bound member operation is inert until the fold supplies its current state.
template <typename Record, typename _F>
struct record_member_event {
  omni::data_member_binding_t<Record, _F> field;
  const std::vector<record_entry> &pending;
  bool check_presence;

  template <typename Config>
  auto operator()(mapping_state<Config> state) const
    -> with_diagnostics<compat::monostate, diagnostics> {
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
  template <typename Config, typename Event>
  fn::fold_control<node_accumulator<Config>> operator()(
    node_accumulator<Config> result, const Event &event) const {
    result.append(event(result.state));
    return result.stopped() ? fn::stop_with(std::move(result))
                            : fn::continue_with(std::move(result));
  }
};

} // namespace detail

namespace reflected_scope {

template <typename NameByAlias>
template <typename T,
  typename Config,
  compat::enable_if_t<omni::is_reflected_record<T>::value, int>>
auto to_record_t<NameByAlias>::operator()(::ryml::ConstNodeRef node,
  T *into,
  mapping_state<Config> context) const
  -> with_diagnostics<compat::monostate, diagnostics> {
  auto result = detail::make_node_accumulator(context);
  if (!node.is_map()) {
    result.report(omni::ryml::detail::make_issue(
      json::issue_code::unexpected_kind,
      json::value_kind::mapping,
      node,
      context.path));

    return std::move(result).finish();
  }

  const auto fields = omni::meta_for<T>::accessible_data_members()
    | fn::filter<fn::non_static>();

  std::vector<detail::record_entry> pending;
  pending.reserve(std::tuple_size<decltype(fields)>::value);
  fields | fn::each(detail::cpp11_collect_members{
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
      [input_name](const detail::record_entry &entry) {
        return input_name == entry.name;
      });

    if (entry == pending.end()) {
      if (const auto canonical = this->name_by_alias(input_name)) {
        entry = std::find_if(pending.begin(),
          pending.end(),
          [canonical](const detail::record_entry &entry) {
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
    | fn::filter<fn::non_static>();

  const auto events = fn::concat(
    members | fn::map(detail::cpp11_defer_member{
      /*pending=*/pending,
      /*check_presence=*/true,
    }),

    members | fn::map(detail::cpp11_defer_member{
      /*pending=*/pending,
      /*check_presence=*/false,
    }));

  auto folded = events | fn::try_fold(detail::cpp11_fold_member{},
    fn::continue_with(std::move(result)));

  return std::move(folded.value).finish();
}

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
