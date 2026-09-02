#pragma once

#include "data.hpp"

#include <benchmark/benchmark.h>
#include <ryml.hpp>

#include <cstddef>
#include <cstdint>

namespace serialization_benchmark {

template <typename T, typename Deserialize, std::size_t Size>
void deserialize_preparsed(benchmark::State &state,
  Deserialize deserialize,
  const char (&input)[Size]) {
  const ryml::Tree tree = ryml::parse_in_arena(c4::to_csubstr(input));

  for (auto _ : state) {
    auto result = deserialize.template to<T>(tree.rootref());
    if (!result) {
      state.SkipWithError(result.error().c_str());
      break;
    }

    benchmark::DoNotOptimize(result);
  }

  state.SetBytesProcessed(
    state.iterations() * static_cast<std::int64_t>(Size - 1));
}

template <typename T, typename Deserialize, std::size_t Size>
void parse_and_deserialize(benchmark::State &state,
  Deserialize deserialize,
  const char (&input)[Size]) {
  for (auto _ : state) {
    const ryml::Tree tree = ryml::parse_in_arena(c4::to_csubstr(input));
    auto result = deserialize.template to<T>(tree.rootref());
    if (!result) {
      state.SkipWithError(result.error().c_str());
      break;
    }

    benchmark::DoNotOptimize(result);
  }

  state.SetBytesProcessed(
    state.iterations() * static_cast<std::int64_t>(Size - 1));
}

} // namespace serialization_benchmark
