#include <omnirefl/reflection.hpp>

#include <benchmark/benchmark.h>

#include "compact_report.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#  define RECORD_CONVERSION_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#  define RECORD_CONVERSION_NOINLINE __attribute__((noinline))
#else
#  define RECORD_CONVERSION_NOINLINE
#endif

namespace record_conversion_bench {

enum class conversion_type {
  aggregate_return, // Hand-written aggregate-initialization baseline.
  default_then_assign,
  reflected,
  aggregate_into,
};

namespace reflected_visitors {

inline constexpr auto benchmark_name = //
  [](omni::record_meta auto source,
    const omni::enum_binding auto conversion,
    omni::record_meta auto batch_convert) -> std::string {
  const std::string_view type = source.type_name();
  const std::string_view qualified_type = source.qualified_type_name();
  const auto type_separator = qualified_type.rfind("::");
  const auto namespace_separator =
    qualified_type.rfind("::", type_separator - 1);
  const auto enumerators = conversion.enumerators();
  const std::map enumerator_names{enumerators.begin(), enumerators.end()};

  std::string name{batch_convert.type_name()};
  name += '/';
  name += type.substr(0, type.rfind('_'));
  name += '/';
  name += qualified_type.substr(namespace_separator + 2,
    type_separator - namespace_separator - 2);
  name += '/';
  name += enumerator_names.at(conversion.enum_value);
  return name;
};

} // namespace reflected_visitors

// TODO: Consider exposing same-named reflected struct conversion as a QoL
// utility.
template <typename From, typename To>
To reflected_convert(From &&from, omni::type_t<To> target_type) {
  // GCC 15 does not count the dependent call from the nested generic visitor
  // when evaluating -Wunused-but-set-variable.
  [[maybe_unused]] static const auto assign_matching_field = //
    [](omni::field_binding auto to, omni::field_binding auto... from) -> void {
    static_assert(
      ((std::string_view(to.name()) == std::string_view(from.name())) || ...),
      "source has no same-named field");

    (std::invoke(
       [](omni::field_binding auto from, omni::field_binding auto to) -> void {
         if constexpr (std::string_view(to.name())
           == std::string_view(from.name())) {
           static_assert(requires { to.ref() = std::move(from).value(); });
           to.ref() = std::move(from).value();
         }
       },
       from,
       to),
      ...);
  };

  const auto visit = //
    [](omni::record_binding auto source, omni::record_meta auto target) -> To {
    // GCC 15 may miss dead-store elimination for this default initialization
    // after lowering the field traversal below.
    To result{};
    omni::record_binding auto destination = target.bind(result);
    std::apply(
      [&source](omni::field_binding auto... to) -> void {
        (std::apply( //
           [&to](omni::field_binding auto... from) -> void {
             assign_matching_field(to, from...);
           },
           source.public_fields()),
          ...);
      },
      destination.public_fields());

    return result;
  };

  return omni::reflected_call(visit, std::forward<From>(from), target_type);
}

template <typename From, typename To>
To aggregate_into_convert(From &&from, omni::type_t<To> target_type) {
  return omni::reflected_call(
    [](omni::record_binding auto source, omni::record_meta auto) -> To {
      return omni::refl::aggregate_into<To>(source.public_fields());
    },
    std::forward<From>(from),
    target_type);
}

namespace reordered {

struct move_source {
  std::string name;
  std::unique_ptr<int> payload;

  static move_source from_index(std::size_t index) {
    return {
      .name = "example",
      .payload = std::make_unique<int>(static_cast<int>(index % 32768)),
    };
  }
};

struct move_destination {
  std::unique_ptr<int> payload;
  std::string name;

  template <conversion_type Implementation, typename From>
  static move_destination from(From &&source)
    requires requires {
      source.payload;
      source.name;
    }
  {
    if constexpr (conversion_type::aggregate_return == Implementation) {
      return {
        .payload = std::move(source.payload),
        .name = std::move(source.name),
      };
    } else if constexpr (conversion_type::default_then_assign
      == Implementation) {
      move_destination result{};
      result.payload = std::move(source.payload);
      result.name = std::move(source.name);
      return result;
    } else if constexpr (conversion_type::reflected == Implementation) {
      return reflected_convert(std::forward<From>(source),
        omni::type<move_destination>);
    } else {
      static_assert(conversion_type::aggregate_into == Implementation);
      return aggregate_into_convert(std::forward<From>(source),
        omni::type<move_destination>);
    }
  }
};

struct trivial_source {
  std::uint64_t id;
  double score;
  std::uint32_t flags;

  static trivial_source from_index(std::size_t index) {
    return {
      .id = 0x100000000ULL + index,
      .score = static_cast<double>(index) + 0.5,
      .flags = static_cast<std::uint32_t>(index % 65536),
    };
  }
};

struct trivial_destination {
  std::uint32_t flags;
  std::uint64_t id;
  double score;

  template <conversion_type Implementation, typename From>
  static trivial_destination from(From &&source)
    requires requires {
      source.flags;
      source.id;
      source.score;
    }
  {
    if constexpr (conversion_type::aggregate_return == Implementation) {
      return {
        .flags = source.flags,
        .id = source.id,
        .score = source.score,
      };
    } else if constexpr (conversion_type::default_then_assign
      == Implementation) {
      trivial_destination result{};
      result.flags = source.flags;
      result.id = source.id;
      result.score = source.score;
      return result;
    } else if constexpr (conversion_type::reflected == Implementation) {
      return reflected_convert(std::forward<From>(source),
        omni::type<trivial_destination>);
    } else {
      static_assert(conversion_type::aggregate_into == Implementation);
      return aggregate_into_convert(std::forward<From>(source),
        omni::type<trivial_destination>);
    }
  }
};

} // namespace reordered

namespace same_order {

struct move_source {
  std::unique_ptr<int> payload;
  std::string name;

  static move_source from_index(std::size_t index) {
    return {
      .payload = std::make_unique<int>(static_cast<int>(index % 32768)),
      .name = "example",
    };
  }
};

struct move_destination {
  std::unique_ptr<int> payload;
  std::string name;

  template <conversion_type Implementation, typename From>
  static move_destination from(From &&source)
    requires requires {
      source.payload;
      source.name;
    }
  {
    if constexpr (conversion_type::aggregate_return == Implementation) {
      return {
        .payload = std::move(source.payload),
        .name = std::move(source.name),
      };
    } else if constexpr (conversion_type::default_then_assign
      == Implementation) {
      move_destination result{};
      result.payload = std::move(source.payload);
      result.name = std::move(source.name);
      return result;
    } else if constexpr (conversion_type::reflected == Implementation) {
      return reflected_convert(std::forward<From>(source),
        omni::type<move_destination>);
    } else {
      static_assert(conversion_type::aggregate_into == Implementation);
      return aggregate_into_convert(std::forward<From>(source),
        omni::type<move_destination>);
    }
  }
};

struct trivial_source {
  std::uint32_t flags;
  std::uint64_t id;
  double score;

  static trivial_source from_index(std::size_t index) {
    return {
      .flags = static_cast<std::uint32_t>(index % 65536),
      .id = 0x100000000ULL + index,
      .score = static_cast<double>(index) + 0.5,
    };
  }
};

struct trivial_destination {
  std::uint32_t flags;
  std::uint64_t id;
  double score;

  template <conversion_type Implementation, typename From>
  static trivial_destination from(From &&source)
    requires requires {
      source.flags;
      source.id;
      source.score;
    }
  {
    if constexpr (conversion_type::aggregate_return == Implementation) {
      return {
        .flags = source.flags,
        .id = source.id,
        .score = source.score,
      };
    } else if constexpr (conversion_type::default_then_assign
      == Implementation) {
      trivial_destination result{};
      result.flags = source.flags;
      result.id = source.id;
      result.score = source.score;
      return result;
    } else if constexpr (conversion_type::reflected == Implementation) {
      return reflected_convert(std::forward<From>(source),
        omni::type<trivial_destination>);
    } else {
      static_assert(conversion_type::aggregate_into == Implementation);
      return aggregate_into_convert(std::forward<From>(source),
        omni::type<trivial_destination>);
    }
  }
};

} // namespace same_order

template <typename To>
struct raw {
  template <std::ranges::sized_range Range, typename Convert>
  RECORD_CONVERSION_NOINLINE std::vector<To> operator()(Range &&input,
    Convert &&convert) const {
    std::vector<To> output;
    output.reserve(std::ranges::size(input));
    for (auto &&from : std::forward<Range>(input))
      output.emplace_back(convert(std::forward<decltype(from)>(from)));
    return output;
  }
};

template <typename To>
struct ranges_transform {
  template <std::ranges::sized_range Range, typename Convert>
  RECORD_CONVERSION_NOINLINE std::vector<To> operator()(Range &&input,
    Convert &&convert) const {
    std::vector<To> output;
    output.reserve(std::ranges::size(input));
    std::ranges::transform(std::forward<Range>(input),
      std::back_inserter(output),
      std::forward<Convert>(convert));
    return output;
  }
};

#if defined(__cpp_lib_ranges_to_container) \
  && 202202L <= __cpp_lib_ranges_to_container
template <typename To>
struct ranges_to {
  template <std::ranges::input_range Range, typename Convert>
  RECORD_CONVERSION_NOINLINE std::vector<To> operator()(Range &&input,
    Convert &&convert) const {
    return std::forward<Range>(input)
      | std::views::transform(std::forward<Convert>(convert))
      | std::ranges::to<std::vector>();
  }
};
#endif

template <typename From,
  typename To,
  conversion_type Implementation,
  template <typename> typename BatchConvert>
void benchmark_scenario() {
  benchmark::RegisterBenchmark(
    /*name=*/omni::reflected_call(reflected_visitors::benchmark_name,
      omni::type<From>,
      Implementation,
      omni::type<BatchConvert<To>>),

    /*function=*/ //
    [](benchmark::State &state) {
      const auto batch_size = static_cast<std::size_t>(state.range(0));

      while (state.KeepRunningBatch(
        static_cast<benchmark::IterationCount>(batch_size))) {
        state.PauseTiming();
        {
          std::vector<From> input;
          input.reserve(batch_size);
          std::ranges::transform(
            /*indexes*/ std::views::iota(std::size_t{0}, batch_size),
            std::back_inserter(input),
            From::from_index);
          state.ResumeTiming();

          auto output = BatchConvert<To>{}(input, [](auto &&from) -> To {
            if constexpr (std::is_copy_constructible_v<From>)
              return To::template from<Implementation>(from);
            else
              return To::template from<Implementation>(std::move(from));
          });
          benchmark::DoNotOptimize(output.data());
          benchmark::ClobberMemory();
          state.PauseTiming();
        } // Destroy the input and output batches outside the timed region.
        state.ResumeTiming();
      }

      state.SetBytesProcessed(state.iterations()
        * static_cast<benchmark::IterationCount>(sizeof(From) + sizeof(To)));
    })
    ->Arg(1024)
    ->Unit(benchmark::kNanosecond);
}

} // namespace record_conversion_bench

int main(int argc, char **argv) {
  benchmark::MaybeReenterWithoutASLR(argc, argv);
  char default_name[] = "record_conversion_bench";
  char *default_arguments[] = {default_name, nullptr};
  if (argv == nullptr) {
    argc = 1;
    argv = default_arguments;
  }

#if defined(RECORD_CONVERSION_HAS_PERF_COUNTERS)
  std::vector<char *> arguments{argv, argv + argc};
  if (std::ranges::none_of(arguments, [](const char *argument) {
        return std::string_view{argument}.starts_with(
          "--benchmark_perf_counters=");
      })) {
    static char counters[] = "--benchmark_perf_counters=instructions,cycles";
    arguments.push_back(counters);
  }
  argc = static_cast<int>(arguments.size());
  arguments.push_back(nullptr);
  argv = arguments.data();
#endif

  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;

  using namespace record_conversion_bench;
  using enum conversion_type;

  benchmark_scenario<reordered::move_source,
    reordered::move_destination,
    aggregate_return,
    raw>();
  benchmark_scenario<same_order::move_source,
    same_order::move_destination,
    aggregate_return,
    raw>();
  benchmark_scenario<reordered::move_source,
    reordered::move_destination,
    default_then_assign,
    raw>();
  benchmark_scenario<same_order::move_source,
    same_order::move_destination,
    default_then_assign,
    raw>();
  benchmark_scenario<reordered::move_source,
    reordered::move_destination,
    reflected,
    raw>();
  benchmark_scenario<same_order::move_source,
    same_order::move_destination,
    reflected,
    raw>();
  benchmark_scenario<reordered::move_source,
    reordered::move_destination,
    aggregate_into,
    raw>();
  benchmark_scenario<same_order::move_source,
    same_order::move_destination,
    aggregate_into,
    raw>();
  benchmark_scenario<reordered::trivial_source,
    reordered::trivial_destination,
    aggregate_return,
    raw>();
  benchmark_scenario<same_order::trivial_source,
    same_order::trivial_destination,
    aggregate_return,
    raw>();
  benchmark_scenario<reordered::trivial_source,
    reordered::trivial_destination,
    default_then_assign,
    raw>();
  benchmark_scenario<same_order::trivial_source,
    same_order::trivial_destination,
    default_then_assign,
    raw>();
  benchmark_scenario<reordered::trivial_source,
    reordered::trivial_destination,
    reflected,
    raw>();
  benchmark_scenario<same_order::trivial_source,
    same_order::trivial_destination,
    reflected,
    raw>();
  benchmark_scenario<reordered::trivial_source,
    reordered::trivial_destination,
    aggregate_into,
    raw>();
  benchmark_scenario<same_order::trivial_source,
    same_order::trivial_destination,
    aggregate_into,
    raw>();

  benchmark_scenario<reordered::move_source,
    reordered::move_destination,
    aggregate_return,
    ranges_transform>();
  benchmark_scenario<same_order::move_source,
    same_order::move_destination,
    aggregate_return,
    ranges_transform>();
  benchmark_scenario<reordered::move_source,
    reordered::move_destination,
    default_then_assign,
    ranges_transform>();
  benchmark_scenario<same_order::move_source,
    same_order::move_destination,
    default_then_assign,
    ranges_transform>();
  benchmark_scenario<reordered::move_source,
    reordered::move_destination,
    reflected,
    ranges_transform>();
  benchmark_scenario<same_order::move_source,
    same_order::move_destination,
    reflected,
    ranges_transform>();
  benchmark_scenario<reordered::move_source,
    reordered::move_destination,
    aggregate_into,
    ranges_transform>();
  benchmark_scenario<same_order::move_source,
    same_order::move_destination,
    aggregate_into,
    ranges_transform>();
  benchmark_scenario<reordered::trivial_source,
    reordered::trivial_destination,
    aggregate_return,
    ranges_transform>();
  benchmark_scenario<same_order::trivial_source,
    same_order::trivial_destination,
    aggregate_return,
    ranges_transform>();
  benchmark_scenario<reordered::trivial_source,
    reordered::trivial_destination,
    default_then_assign,
    ranges_transform>();
  benchmark_scenario<same_order::trivial_source,
    same_order::trivial_destination,
    default_then_assign,
    ranges_transform>();
  benchmark_scenario<reordered::trivial_source,
    reordered::trivial_destination,
    reflected,
    ranges_transform>();
  benchmark_scenario<same_order::trivial_source,
    same_order::trivial_destination,
    reflected,
    ranges_transform>();
  benchmark_scenario<reordered::trivial_source,
    reordered::trivial_destination,
    aggregate_into,
    ranges_transform>();
  benchmark_scenario<same_order::trivial_source,
    same_order::trivial_destination,
    aggregate_into,
    ranges_transform>();

#if defined(__cpp_lib_ranges_to_container) \
  && 202202L <= __cpp_lib_ranges_to_container
  benchmark_scenario<reordered::move_source,
    reordered::move_destination,
    aggregate_return,
    ranges_to>();
  benchmark_scenario<same_order::move_source,
    same_order::move_destination,
    aggregate_return,
    ranges_to>();
  benchmark_scenario<reordered::move_source,
    reordered::move_destination,
    default_then_assign,
    ranges_to>();
  benchmark_scenario<same_order::move_source,
    same_order::move_destination,
    default_then_assign,
    ranges_to>();
  benchmark_scenario<reordered::move_source,
    reordered::move_destination,
    reflected,
    ranges_to>();
  benchmark_scenario<same_order::move_source,
    same_order::move_destination,
    reflected,
    ranges_to>();
  benchmark_scenario<reordered::move_source,
    reordered::move_destination,
    aggregate_into,
    ranges_to>();
  benchmark_scenario<same_order::move_source,
    same_order::move_destination,
    aggregate_into,
    ranges_to>();
  benchmark_scenario<reordered::trivial_source,
    reordered::trivial_destination,
    aggregate_return,
    ranges_to>();
  benchmark_scenario<same_order::trivial_source,
    same_order::trivial_destination,
    aggregate_return,
    ranges_to>();
  benchmark_scenario<reordered::trivial_source,
    reordered::trivial_destination,
    default_then_assign,
    ranges_to>();
  benchmark_scenario<same_order::trivial_source,
    same_order::trivial_destination,
    default_then_assign,
    ranges_to>();
  benchmark_scenario<reordered::trivial_source,
    reordered::trivial_destination,
    reflected,
    ranges_to>();
  benchmark_scenario<same_order::trivial_source,
    same_order::trivial_destination,
    reflected,
    ranges_to>();
  benchmark_scenario<reordered::trivial_source,
    reordered::trivial_destination,
    aggregate_into,
    ranges_to>();
  benchmark_scenario<same_order::trivial_source,
    same_order::trivial_destination,
    aggregate_into,
    ranges_to>();
#endif

  record_conversion_bench::compact_report_t report;
  benchmark::RunSpecifiedBenchmarks(&report);
  benchmark::Shutdown();
  return 0;
}

#undef RECORD_CONVERSION_NOINLINE
