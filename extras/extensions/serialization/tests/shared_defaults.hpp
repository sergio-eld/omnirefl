#pragma once

#include <omnirefl/functional.hpp>
#include <omnirefl/serialization/ryml.hpp>

#include <map>
#include <string>

namespace serialization_shared_defaults {

inline int identity(int value) { return value; }

// Each translation unit must observe the objects directly: merging an external
// inline sampler could hide translation-unit-local default objects.
static std::map<std::string, const void *> local_addresses() {
  return {
    {"reflected_call", &omni::reflected_call},
    {"fn::branch", &omni::fn::branch},
    {"fn::cond", &omni::fn::cond},
    {"fn::always_true", &omni::fn::always_true},
    {"fn::always_false", &omni::fn::always_false},
    {"fn::is_static", &omni::fn::is_static},
    {"fn::is_non_static", &omni::fn::is_non_static},
    {"fn::has_callable_metadata", &omni::fn::has_callable_metadata},
    {"fn::is_non_functional", &omni::fn::is_non_functional},
    {"fn::is_assignable", &omni::fn::is_assignable},
    {"map_by_schema", &omni::ryml::map_by_schema},
    {"deserialize", &omni::ryml::deserialize},
    {"as_owning_tree", &omni::ryml::as_owning_tree},
    {"as_ref_tree", &omni::ryml::as_ref_tree},
    {"as_yaml", &omni::ryml::as_yaml},
    {"as_json", &omni::ryml::as_json},
    {"render_diagnostics", &omni::ryml::render_diagnostics},
    {"core::map_to_fundamental", &omni::ryml::core::map_to_fundamental},
    {"core::map_to_string_like", &omni::ryml::core::map_to_string_like},
    {"core::map_to_reflected", &omni::ryml::core::map_to_reflected},
    {"core::map_to_sequence", &omni::ryml::core::map_to_sequence},
    {"core::map_to_optional", &omni::ryml::core::map_to_optional},
    {"core::map_node_to_string_like",
      &omni::ryml::core::map_node_to_string_like},
    {"core::map_node_to_boolean", &omni::ryml::core::map_node_to_boolean},
    {"core::map_node_to_integral", &omni::ryml::core::map_node_to_integral},
    {"core::map_node_to_floating_point",
      &omni::ryml::core::map_node_to_floating_point},
    {"core::map_node_to_arithmetic", &omni::ryml::core::map_node_to_arithmetic},
    {"core::map_node_to_enum",
      &omni::ryml::core::reflected_scope::map_node_to_enum},
    {"default_to_string_like", &omni::ryml::default_to_string_like},
    {"from_string_like", &omni::ryml::from_string_like},
    {"default_to_boolean", &omni::ryml::default_to_boolean},
    {"from_boolean", &omni::ryml::from_boolean},
    {"default_to_integral", &omni::ryml::default_to_integral},
    {"from_integral", &omni::ryml::from_integral},
    {"default_to_floating_point", &omni::ryml::default_to_floating_point},
    {"from_floating_point", &omni::ryml::from_floating_point},
    {"default_to_arithmetic", &omni::ryml::default_to_arithmetic},
    {"from_arithmetic", &omni::ryml::from_arithmetic},
    {"from_optional_like", &omni::ryml::from_optional_like},
    {"from_range", &omni::ryml::from_range},
    {"from_record", &omni::ryml::reflected_scope::from_record},
    {"from_enum", &omni::ryml::reflected_scope::from_enum},
#if defined(__cpp_variable_templates)
    {"type<int>", &omni::type<int>},
#endif
#if defined(__cpp_nontype_template_parameter_auto)
    {"function<identity>", &omni::function<identity>},
#endif
  };
}

std::map<std::string, const void *> other_addresses();

} // namespace serialization_shared_defaults
