#include "serialization/benchmark.hpp"
#include "serialization/deserialize_if_constexpr.hpp"

namespace {

void if_constexpr_nested_json(benchmark::State &state) {
  serialization_benchmark::deserialize_preparsed<
    serialization_data::nested_record>(state,
    serialization::if_constexpr::deserialize,
    serialization_data::nested_json);
}

void if_constexpr_document_json(benchmark::State &state) {
  serialization_benchmark::deserialize_preparsed<serialization_data::document>(
    state,
    serialization::if_constexpr::deserialize,
    serialization_data::representative_json);
}

void if_constexpr_document_yaml(benchmark::State &state) {
  serialization_benchmark::deserialize_preparsed<serialization_data::document>(
    state,
    serialization::if_constexpr::deserialize,
    serialization_data::representative_yaml);
}

void if_constexpr_parse_document_json(benchmark::State &state) {
  serialization_benchmark::parse_and_deserialize<serialization_data::document>(
    state,
    serialization::if_constexpr::deserialize,
    serialization_data::representative_json);
}

void if_constexpr_parse_document_yaml(benchmark::State &state) {
  serialization_benchmark::parse_and_deserialize<serialization_data::document>(
    state,
    serialization::if_constexpr::deserialize,
    serialization_data::representative_yaml);
}

BENCHMARK(if_constexpr_nested_json);
BENCHMARK(if_constexpr_document_json);
BENCHMARK(if_constexpr_document_yaml);
BENCHMARK(if_constexpr_parse_document_json);
BENCHMARK(if_constexpr_parse_document_yaml);

} // namespace
