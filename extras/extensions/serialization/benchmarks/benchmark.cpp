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

struct diagnostic_record {
  std::vector<int> values;
};

void tree_mapping_invalid_elements(benchmark::State &state) {
  const auto count = static_cast<std::size_t>(state.range(0));
  const bool partial = 0 != state.range(1);
  ::ryml::Tree tree;
  auto root = tree.rootref();
  root |= ::ryml::MAP;
  auto values = root.append_child();
  values.set_key_serialized(std::string{"values"});
  values |= ::ryml::SEQ;
  for (std::size_t i = 0; i < count; ++i) {
    auto value = values.append_child();
    value.set_val_serialized(std::string{"invalid"});
    value |= ::ryml::VAL_DQUO;
  }

  const auto map = omni::ryml::map_tree_t{
    omni::ryml::default_strategy()
      .use_tolerance(partial ? 0 : static_cast<int>(count))
      .allow_partial(partial),
  };

  const auto probe = map(omni::type_t<diagnostic_record>{}, tree.crootref());
  if (count != probe.diagnostics.issues.size()
    || partial != static_cast<bool>(probe.value)) {
    state.SkipWithError("invalid-element workload did not map as expected");
    return;
  }

  for (auto _ : state) {
    auto result = map(omni::type_t<diagnostic_record>{}, tree.crootref());
    benchmark::DoNotOptimize(result);
  }

  state.SetItemsProcessed(
    state.iterations() * static_cast<std::int64_t>(count));
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
BENCHMARK(tree_mapping_invalid_elements)
  ->ArgNames({"elements", "partial"})
  ->Args({16, 0})
  ->Args({16, 1})
  ->Args({256, 0})
  ->Args({256, 1})
  ->Args({4096, 0})
  ->Args({4096, 1});
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
