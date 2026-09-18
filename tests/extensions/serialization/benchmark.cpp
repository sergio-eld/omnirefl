#include "benchmark.hpp"

namespace {

void tree_mapping_nested_json(benchmark::State &state) {
  serialization_benchmark::deserialize_preparsed(state,
    omni::fn::partial(omni::ryml::map_tree,
      omni::type_t<serialization_data::nested_record>{}),
    serialization_data::nested_json);
}

void tree_mapping_document_json(benchmark::State &state) {
  serialization_benchmark::deserialize_preparsed(state,
    omni::fn::partial(omni::ryml::map_tree,
      omni::type_t<serialization_data::document>{}),
    serialization_data::representative_json);
}

void tree_mapping_document_yaml(benchmark::State &state) {
  serialization_benchmark::deserialize_preparsed(state,
    omni::fn::partial(omni::ryml::map_tree,
      omni::type_t<serialization_data::document>{}),
    serialization_data::representative_yaml);
}

void deserialize_document_json(benchmark::State &state) {
  serialization_benchmark::deserialize_owned(state,
    omni::fn::partial(omni::ryml::deserialize,
      omni::type_t<serialization_data::document>{}),
    serialization_data::representative_json);
}

void deserialize_document_yaml(benchmark::State &state) {
  serialization_benchmark::deserialize_owned(state,
    omni::fn::partial(omni::ryml::deserialize,
      omni::type_t<serialization_data::document>{}),
    serialization_data::representative_yaml);
}

void serialize_nested_json(benchmark::State &state) {
  serialization_benchmark::serialize_owned<serialization_data::nested_record>(
    state, omni::ryml::as_json, serialization_data::nested_json);
}

void serialize_nested_yaml(benchmark::State &state) {
  serialization_benchmark::serialize_owned<serialization_data::nested_record>(
    state, omni::ryml::as_yaml, serialization_data::nested_json);
}

void serialize_document_json(benchmark::State &state) {
  serialization_benchmark::serialize_owned<serialization_data::document>(
    state, omni::ryml::as_json, serialization_data::representative_json);
}

void serialize_document_yaml(benchmark::State &state) {
  serialization_benchmark::serialize_owned<serialization_data::document>(
    state, omni::ryml::as_yaml, serialization_data::representative_json);
}

BENCHMARK(tree_mapping_nested_json);
BENCHMARK(tree_mapping_document_json);
BENCHMARK(tree_mapping_document_yaml);
BENCHMARK(deserialize_document_json);
BENCHMARK(deserialize_document_yaml);
BENCHMARK(serialize_nested_json);
BENCHMARK(serialize_nested_yaml);
BENCHMARK(serialize_document_json);
BENCHMARK(serialize_document_yaml);

} // namespace
