#pragma once

#include <omnirefl/serialization/ryml.hpp>

#include <utility>

namespace {

namespace local_mapping {

struct record { int value; };

struct add_integer {
  int increment;

  template <typename State>
  auto operator()(State state, ::ryml::ConstNodeRef node,
    omni::type_t<int> type) const -> omni::ryml::with_diagnostics_t<int> {
    return omni::ryml::map_integer_node(std::move(state), node, type)
      .transform([this](int value) { return value + this->increment; });
  }
};

struct select_mapping {
  int increment;

  template <typename Path, typename T>
  auto operator()(const Path &path, omni::type_t<T> type) const
    -> decltype(omni::ryml::reflected_scope::fn::mapping_to_type(path, type)) {
    return omni::ryml::reflected_scope::fn::mapping_to_type(path, type);
  }

  template <typename Path>
  add_integer operator()(Path path, omni::type_t<int>) const {
    constexpr bool selected = path.str().equals("root.value");
    return {selected ? this->increment : 0};
  }
};

} // namespace local_mapping

} // namespace
