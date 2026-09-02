#include "serialization/benchmark.hpp"
#include "serialization/deserialize_composed.hpp"

namespace {

void composed_nested_json(benchmark::State &state) {
  serialization_benchmark::deserialize_preparsed<
    serialization_data::nested_record>(state,
    serialization::composed::deserialize,
    serialization_data::nested_json);
}

void composed_document_json(benchmark::State &state) {
  serialization_benchmark::deserialize_preparsed<serialization_data::document>(
    state,
    serialization::composed::deserialize,
    serialization_data::representative_json);
}

void composed_document_yaml(benchmark::State &state) {
  serialization_benchmark::deserialize_preparsed<serialization_data::document>(
    state,
    serialization::composed::deserialize,
    serialization_data::representative_yaml);
}

void composed_parse_document_json(benchmark::State &state) {
  serialization_benchmark::parse_and_deserialize<serialization_data::document>(
    state,
    serialization::composed::deserialize,
    serialization_data::representative_json);
}

void composed_parse_document_yaml(benchmark::State &state) {
  serialization_benchmark::parse_and_deserialize<serialization_data::document>(
    state,
    serialization::composed::deserialize,
    serialization_data::representative_yaml);
}

BENCHMARK(composed_nested_json);
BENCHMARK(composed_document_json);
BENCHMARK(composed_document_yaml);
BENCHMARK(composed_parse_document_json);
BENCHMARK(composed_parse_document_yaml);

} // namespace
