#include "serialization/benchmark.hpp"
#include "serialization/map_tree.hpp"

namespace {

struct cpp11_lambda_map_tree {
  template <typename To>
  auto operator()(const ryml::ConstNodeRef &from, omni::type_t<To> to) const
    -> decltype(omni::ryml::reflected_scope::map_tree(from, to)) {
    return omni::ryml::reflected_scope::map_tree(from, to);
  }
};

void tree_mapping_nested_json(benchmark::State &state) {
  serialization_benchmark::deserialize_preparsed<
    serialization_data::nested_record>(state,
    cpp11_lambda_map_tree{},
    serialization_data::nested_json);
}

void tree_mapping_document_json(benchmark::State &state) {
  serialization_benchmark::deserialize_preparsed<serialization_data::document>(
    state,
    cpp11_lambda_map_tree{},
    serialization_data::representative_json);
}

void tree_mapping_document_yaml(benchmark::State &state) {
  serialization_benchmark::deserialize_preparsed<serialization_data::document>(
    state,
    cpp11_lambda_map_tree{},
    serialization_data::representative_yaml);
}

void tree_mapping_parse_document_json(benchmark::State &state) {
  serialization_benchmark::deserialize_owned(state,
    omni::fn::partial(omni::ryml::deserialize,
      omni::ryml::strategy{},
      omni::type_t<serialization_data::document>{}),
    serialization_data::representative_json);
}

void tree_mapping_parse_document_yaml(benchmark::State &state) {
  serialization_benchmark::deserialize_owned(state,
    omni::fn::partial(omni::ryml::deserialize,
      omni::ryml::strategy{},
      omni::type_t<serialization_data::document>{}),
    serialization_data::representative_yaml);
}

BENCHMARK(tree_mapping_nested_json);
BENCHMARK(tree_mapping_document_json);
BENCHMARK(tree_mapping_document_yaml);
BENCHMARK(tree_mapping_parse_document_json);
BENCHMARK(tree_mapping_parse_document_yaml);

} // namespace
