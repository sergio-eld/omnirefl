#pragma once

#include "data.hpp"

#include <omnirefl/serialization/ryml.hpp>

#include <omnirefl/reflected_scope.hpp>

#include <benchmark/benchmark.h>
#include <ryml.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace serialization_benchmark {

// Results remain mutable because Google Benchmark deprecates its
// const-reference `DoNotOptimize` overload.
template <typename Deserialize, std::size_t Size>
void deserialize_preparsed(benchmark::State &state,
  Deserialize deserialize,
  const char (&input)[Size]) {
  const ryml::Tree tree = ryml::parse_in_arena(c4::to_csubstr(input));

  for (auto _ : state) {
    auto result = omni::compat::invoke(deserialize, tree.crootref());
    if (!result) {
      state.SkipWithError(
        omni::ryml::render_diangostics(result.error()).c_str());
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
      state.SkipWithError(
        omni::ryml::render_diangostics(result.error()).c_str());
      break;
    }

    benchmark::DoNotOptimize(result);
  }

  state.SetBytesProcessed(
    state.iterations() * static_cast<std::int64_t>(Size - 1));
}

template <typename T, typename Serialize, std::size_t Size>
void serialize_owned(benchmark::State &state,
  Serialize serialize,
  const char (&input)[Size]) {
  const auto source =
    omni::ryml::deserialize(omni::type_t<T>{}, std::string{input});
  if (!source) {
    state.SkipWithError(omni::ryml::render_diangostics(source.error()).c_str());
    return;
  }

  std::int64_t bytes = 0;
  for (auto _ : state) {
    auto result = omni::compat::invoke(serialize, *source);
    bytes += static_cast<std::int64_t>(result.size());
    benchmark::DoNotOptimize(result);
  }

  state.SetBytesProcessed(bytes);
}

} // namespace serialization_benchmark
