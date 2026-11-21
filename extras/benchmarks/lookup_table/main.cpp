/** Internal name-lookup benchmark; excluded from the CI build.
 * Uses the field names and documents from the JSON/YAML mapping benchmark.
 * All language modes retain the same workloads and lookup strategies.
 *
 * Existing CMake targets: cpp_<std>.lookup_table.benchmark, or the
 * lookup_table.benchmark umbrella. For example:
 * cmake --build <build> --target cpp_11.lookup_table.benchmark
 * Select an optimized build for timing measurements.
 * Arguments: [iterations=2000000] [repetitions=7] [csv=lookup_table.csv].
 * Reports median ns/lookup; CSV retains every measurement and checksum. */
#include <omnirefl/functional/lookup_table.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <random>
#include <string>
#if 201703L <= OMNI_CPLUSPLUS
#  include <string_view>
#else
#  include <nonstd/string_view.hpp>
#endif
#include <tuple>
#include <utility>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#  define OMNI_BENCH_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#  define OMNI_BENCH_NOINLINE __declspec(noinline)
#else
#  define OMNI_BENCH_NOINLINE
#endif

namespace lookup_comparison {
#if 201703L <= OMNI_CPLUSPLUS
using key_view = std::string_view;
#else
using key_view = nonstd::string_view;
#endif

#define OMNI_BENCH_FIELD(Name) struct Name##_field { \
  static constexpr const char *name() { return #Name; } \
};
OMNI_BENCH_FIELD(id)
OMNI_BENCH_FIELD(name)
OMNI_BENCH_FIELD(retries)
OMNI_BENCH_FIELD(enabled)
OMNI_BENCH_FIELD(threshold)
OMNI_BENCH_FIELD(timeout)
OMNI_BENCH_FIELD(label)
OMNI_BENCH_FIELD(settings)
OMNI_BENCH_FIELD(version)
OMNI_BENCH_FIELD(revision)
OMNI_BENCH_FIELD(primary)
OMNI_BENCH_FIELD(backup)
OMNI_BENCH_FIELD(services)
#undef OMNI_BENCH_FIELD

// This is the former literal matcher, including by-value empty metadata
// parameters and tuple application. No table-of-views substitutes for it.
template <std::size_t Index>
std::size_t find_record_member_index(key_view) { return Index; }
template <std::size_t Index, typename Field, typename... Rest>
std::size_t find_record_member_index(key_view key,
  Field field, Rest... rest) {
  return key == key_view{field.name()} ? Index
    : find_record_member_index<Index + 1>(key, rest...);
}
struct find_record_member {
  key_view key;
  template <typename... Field>
  std::size_t operator()(Field... field) const {
    return find_record_member_index<0>(key, field...);
  }
};
template <typename Tuple>
std::size_t literal_lookup(const Tuple &fields, key_view key) {
  return omni::compat::apply(find_record_member{key}, fields);
}

template <typename... Field>
struct source {
  using table_type = decltype(omni::fn::lookup_table(Field{}...));
  static constexpr table_type table = omni::fn::lookup_table(Field{}...);
  static constexpr std::size_t count = sizeof...(Field);
  static const std::array<key_view, count> names;

  static OMNI_BENCH_NOINLINE std::size_t literal(key_view key) {
    return literal_lookup(table.entries, key);
  }

  static OMNI_BENCH_NOINLINE std::size_t loop(key_view key) {
    for (std::size_t index = 0; index != names.size(); ++index)
      if (key == names[index])
        return index;

    return count;
  }

  static OMNI_BENCH_NOINLINE std::size_t lookup(key_view key) {
    const auto match = table.lookup(key);
    return match ? match.value().index : count;
  }
};

#if OMNI_CPLUSPLUS < 201703L
// C++11/14 static constexpr members need storage when passed through apply.
template <typename... Field>
constexpr typename source<Field...>::table_type source<Field...>::table;
#endif

template <typename... Field>
const std::array<key_view, source<Field...>::count> source<Field...>::names{{
  key_view{Field{}.name()}...,
}};

using flat = source<id_field, name_field, retries_field, enabled_field,
  threshold_field, timeout_field>;
using nested = source<label_field, settings_field, version_field>;
using deep = source<revision_field, primary_field, backup_field>;
using sequence = source<name_field, services_field, enabled_field>;

// Values reproduce the comparison documents exactly. Only their object keys
// enter timed lookup, and document construction is outside every timed loop.
const std::string flat_document = R"({"id":42,"name":"gateway.example.com",)"
  R"("retries":3,"enabled":true,"threshold":0.75,"timeout":1500})";
const std::string nested_document =
  R"({"label":"primary","settings":)" + flat_document + R"(,"version":2})";
const std::string deep_document = R"({"revision":7,"primary":)"
  + nested_document + R"(,"backup":)" + nested_document + "}";
const std::string sequence_document = R"({"name":"cluster","services":[)"
  + nested_document + "," + nested_document + "," + nested_document + ","
  + nested_document + R"(],"enabled":true})";

using operation = std::size_t (*)(key_view);
struct trace {
  std::string label;
  std::vector<std::string> keys;
  std::array<std::size_t, 4096> order;
};

template <typename Source>
std::vector<trace> traces(std::mt19937 &random) {
  std::vector<trace> result;
  for (const std::string mode : {"hits", "misses", "mixed", "document",
      "unknown", "duplicate"}) {
    trace row{mode, {}, {}};
    if (mode != "misses")
      for (auto name : Source::names)
        row.keys.emplace_back(name.data(), name.size());
    if (mode == "misses" || mode == "mixed" || mode == "unknown")
      row.keys.emplace_back("extra");
    if (mode == "duplicate")
      row.keys.emplace_back(Source::names.front().data(),
        Source::names.front().size());
    const bool shuffled = mode == "hits" || mode == "misses" || mode == "mixed";
    for (std::size_t index = 0; index != row.order.size(); ++index)
      row.order[index] = shuffled ? random() % row.keys.size()
        : index % row.keys.size();
    result.push_back(std::move(row));
  }
  return result;
}

bool check(operation literal, operation loop, operation lookup,
  const std::vector<std::string> &keys, std::size_t count,
  const std::vector<key_view> &names) {
  for (const auto &key : keys) {
    const key_view view{key.data(), key.size()};
    std::size_t expected = count;
    for (std::size_t index = 0; index != names.size(); ++index)
      if (view == names[index]) {
        expected = index;
        break;
      }
    if (literal(view) != expected || loop(view) != expected
      || lookup(view) != expected) {
      std::cerr << "lookup mismatch for " << key << '\n';
      return false;
    }
  }
  return true;
}

// A signal fence is a compiler memory barrier, not CPU timing machinery. Keys
// and shuffled indices live in runtime vectors, so the loads and lookup cannot
// be folded to a compile-time checksum or hoisted out of the timed traversal.
struct measurement {
  double nanoseconds;
  std::size_t checksum;
};

measurement measure(operation find, const trace &input,
  std::size_t iterations) {
  std::size_t checksum = 0;
  const auto begin = std::chrono::steady_clock::now();
  for (std::size_t iteration = 0; iteration != iterations; ++iteration) {
    std::atomic_signal_fence(std::memory_order_seq_cst);
    const auto index = input.order[iteration & (input.order.size() - 1)];
    const auto &text = input.keys[index];
    const key_view key{text.data(), text.size()};
    checksum += find(key);
    std::atomic_signal_fence(std::memory_order_seq_cst);
  }
  const auto end = std::chrono::steady_clock::now();
  return {
    std::chrono::duration<double, std::nano>(end - begin).count() / iterations,
    checksum,
  };
}

// The reference reports common key-selection and indirect-call cost. Results
// below are raw measurements; subtracting this baseline can amplify noise.
OMNI_BENCH_NOINLINE std::size_t key_size(key_view key) {
  return key.size();
}

struct strategy {
  key_view name;
  operation find;
};

template <typename Source>
bool run(key_view name, const std::string &document,
  std::mt19937 &random, std::size_t iterations, std::size_t repetitions,
  std::ostream &csv) {
  const auto inputs = traces<Source>(random);
  const std::vector<key_view> names(Source::names.begin(),
    Source::names.end());
  for (const auto &input : inputs)
    if (!check(Source::literal, Source::loop, Source::lookup,
        input.keys, Source::count, names))
      return false;
  const std::vector<std::string> edges{"", "extra", std::string(80, 'x')};
  if (!check(Source::literal, Source::loop, Source::lookup, edges,
      Source::count, names))
    return false;
  std::cout << "# " << name << " document_bytes=" << document.size() << '\n';
  const std::array<strategy, 4> strategies{{
    {"literal", Source::literal}, {"loop", Source::loop},
    {"lookup", Source::lookup}, {"reference", key_size},
  }};
  constexpr auto strategy_count = std::tuple_size<decltype(strategies)>::value;
  for (const auto &input : inputs) {
    std::array<std::vector<double>, strategy_count> samples;
    std::array<std::size_t, strategy_count> order{{0, 1, 2, 3}};
    for (std::size_t repetition = 0; repetition != repetitions; ++repetition) {
      std::shuffle(order.begin(), order.end(), random);
      for (const auto index : order) {
        const auto measured = measure(strategies[index].find,
          input, iterations);
        samples[index].push_back(measured.nanoseconds);
        csv << name << ',' << input.label << ',' << strategies[index].name
          << ',' << repetition << ',' << measured.nanoseconds << ','
          << measured.checksum << '\n';
      }
    }
    std::cout << name << '/' << input.label;
    for (std::size_t index = 0; index != strategies.size(); ++index) {
      std::sort(samples[index].begin(), samples[index].end());
      std::cout << ' ' << strategies[index].name << '='
        << samples[index][samples[index].size() / 2];
    }
    std::cout << " ns/key (median)\n";
  }
  return true;
}
} // namespace lookup_comparison

extern "C" OMNI_BENCH_NOINLINE std::size_t flat_literal(
  lookup_comparison::key_view key) {
  return lookup_comparison::flat::literal(key);
}
extern "C" OMNI_BENCH_NOINLINE std::size_t flat_loop(
  lookup_comparison::key_view key) {
  return lookup_comparison::flat::loop(key);
}
extern "C" OMNI_BENCH_NOINLINE std::size_t flat_lookup(
  lookup_comparison::key_view key) {
  return lookup_comparison::flat::lookup(key);
}

int main(int argc, char **argv) {
  const auto iterations = argc > 1
    ? std::strtoull(argv[1], nullptr, 10) : 2000000ULL;
  const auto repetitions = argc > 2
    ? std::strtoull(argv[2], nullptr, 10) : 7ULL;
  if (!iterations || repetitions < 7)
    return 2;
  const std::string output = argc > 3 ? argv[3] : "lookup_table.csv";
  std::ofstream csv(output);
  if (!csv) {
    std::cerr << "cannot write " << output << '\n';
    return 2;
  }
  csv << "schema,trace,strategy,repetition,ns_per_lookup,checksum\n";
  csv << std::setprecision(12);
  std::mt19937 random{0x12345678u};
  std::cout << std::fixed << std::setprecision(3);
  using namespace lookup_comparison;
  const bool passed = run<flat>("flat", flat_document, random,
      iterations, repetitions, csv)
    && run<nested>("nested", nested_document, random,
      iterations, repetitions, csv)
    && run<deep>("deep", deep_document, random,
      iterations, repetitions, csv)
    && run<sequence>("sequence", sequence_document, random,
      iterations, repetitions, csv);
  std::cout << "# raw measurements: " << output << '\n';
  return passed ? 0 : 1;
}
