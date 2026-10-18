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

void serialize_text_json(benchmark::State &state) {
  const std::vector<std::string> source(
    static_cast<std::size_t>(state.range(0)),
    state.range(1) ? "\"\\\n\t\001\037" : "abcdefghijklmno");

  std::int64_t bytes = 0;
  for (auto _ : state) {
    auto result = omni::ryml::as_json(source);
    bytes += static_cast<std::int64_t>(result.size());
    benchmark::DoNotOptimize(result);
  }

  state.SetBytesProcessed(bytes);
}

// Isolate RapidYAML emission from reflection, tree construction and JSON repair.
void render_document_json(benchmark::State &state) {
  const auto tree = ::ryml::parse_in_arena(
    c4::to_csubstr(serialization_data::representative_json));

  std::int64_t bytes = 0;
  for (auto _ : state) {
    auto result = ::ryml::emitrs_json<std::string>(tree);
    bytes += static_cast<std::int64_t>(result.size());
    benchmark::DoNotOptimize(result);
  }

  state.SetBytesProcessed(bytes);
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
BENCHMARK(serialize_text_json)
  ->ArgNames({"fragments", "escaped"})
  ->Args({8, 0})
  ->Args({128, 0})
  ->Args({8, 1})
  ->Args({128, 1});
BENCHMARK(render_document_json);

} // namespace
