#pragma once

#include "data.hpp"

#include <omnirefl/reflection.hpp>

#include <benchmark/benchmark.h>
#include <ryml.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace serialization_benchmark {

// Results remain mutable because Google Benchmark deprecates its const-reference
// `DoNotOptimize` overload.
template <typename T, typename Deserialize, std::size_t Size>
void deserialize_preparsed(benchmark::State &state,
  Deserialize deserialize,
  const char (&input)[Size]) {
  const ryml::Tree tree = ryml::parse_in_arena(c4::to_csubstr(input));

  for (auto _ : state) {
    auto result = omni::compat::invoke(
      deserialize, tree.rootref(), omni::type_t<T>{});
    if (!result) {
      state.SkipWithError(result.error().c_str());
      break;
    }

    benchmark::DoNotOptimize(result);
  }

  state.SetBytesProcessed(
    state.iterations() * static_cast<std::int64_t>(Size - 1));
}

template <typename Deserialize, std::size_t Size>
void deserialize_owned(benchmark::State &state,
  Deserialize deserialize,
  const char (&input)[Size]) {
  for (auto _ : state) {
    auto result = omni::compat::invoke(deserialize, std::string{input});
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
