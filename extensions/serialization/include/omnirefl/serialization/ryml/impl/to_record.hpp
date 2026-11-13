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

struct cpp11_require_member {
  const std::vector<record_entry> &pending;
  node_accumulator &result;

  template <typename Record, typename _F>
  void operator()(omni::data_member_binding_t<Record, _F> field) const {
    if (this->result.stopped())
      return;

    const auto name = c4::to_csubstr(field.name());
    if (std::any_of(this->pending.begin(),
          this->pending.end(),
          [name](const record_entry &entry) {
            return compat::string_view{name.str, name.len} == entry.name
              && !entry.node.invalid();
          }))
      return;

    const auto recovery = this->result.context.options.recovery;
    if (recovery
      && (*recovery & json::recovery_policy::field_defaults)
        == json::recovery_policy::field_defaults
      && field.has_default_value_access())
      return;

    using member_type = typename omni::data_member_meta_t<_F>::type;
    if ((traits::is_optional<member_type>::value
          || (recovery
            && (*recovery & json::recovery_policy::default_initializable)
              == json::recovery_policy::default_initializable))
      && std::is_default_constructible<member_type>::value) {
      initialize_missing(field);
      return;
    }

    const auto context = this->result.child_context({
      /*field=*/name,
      /*index=*/compat::nullopt,
    });

    this->result.report(make_issue(json::issue_code::missing_field,
      json::value_kind::unknown,
      ::ryml::ConstNodeRef{},
      context.path));
  }
};

struct cpp11_read_member {
  std::vector<record_entry> &pending;
  node_accumulator &result;

  template <typename Record, typename _F>
  void operator()(omni::data_member_binding_t<Record, _F> field) const {
    // Ad hoc: fn::each cannot short-circuit member traversal.
    // TODO: Let core traversal check stopping before invoking adapters.
    if (this->result.stopped())
      return;

    const compat::string_view name{field.name()};
    const auto entry = std::find_if(this->pending.begin(),
      this->pending.end(),
      [name](const record_entry &entry) { return name == entry.name; });

    // The preceding presence pass already recovered or reported missing fields.
    if (entry == this->pending.end() || entry->node.invalid())
      return;

    const auto child = entry->node;
    const auto context = this->result.child_context({
      /*field=*/child.key(),
      /*index=*/compat::nullopt,
    });

    // A temporary also supports bitfields, which cannot expose references.
    typename omni::data_member_meta_t<_F>::type value = field.value();
    auto mapped = core::read_node(child, &value, context);
    if (mapped.value)
      field.set_value(std::move(value));

    this->result.append(std::move(mapped));
    // Presence does not depend on decoding success; consume invalid input too.
    this->pending.erase(entry);
  }
};

} // namespace detail

namespace reflected_scope {

template <typename AliasByName>
template <typename T>
auto to_record<AliasByName>::operator()(::ryml::ConstNodeRef node,
  T *into,
  const omni::ryml::detail::mapping_context &context) const
  -> compat::enable_if_t<omni::is_reflected_record<T>::value, node_result> {
  auto result = detail::make_node_accumulator(context, node);
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
      if (const auto canonical = this->alias_by_name(input_name)) {
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

    const auto duplicates = context.options.error_policy.duplicates;
    if (duplicate
      && (duplicates & json::duplicate_policy::last)
        == json::duplicate_policy::last)
      entry->node = child;

    const auto unknown = context.options.error_policy.unknown_fields;
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

    const auto child_context = result.child_context({
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

  const auto binding = omni::meta_for<T>::bind(*into);
  const auto members = binding.accessible_data_members()
    | fn::filter<fn::non_static>();

  members | fn::each(detail::cpp11_require_member{
    /*pending=*/pending,
    /*result=*/result,
  });

  members | fn::each(detail::cpp11_read_member{
    /*pending=*/pending,
    /*result=*/result,
  });

  return std::move(result).finish();
}

} // namespace reflected_scope
} // namespace ryml
} // namespace omni
