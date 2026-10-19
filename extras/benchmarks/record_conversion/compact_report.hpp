#pragma once

#include <benchmark/benchmark.h>

#include <algorithm>
#include <iomanip>
#include <map>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace record_conversion_bench {

class compact_report_t : public benchmark::ConsoleReporter {
  using Run = benchmark::BenchmarkReporter::Run;

  struct implementation_run_t {
    std::string implementation;
    std::string layout;
    const Run *run;
  };

  std::vector<Run> runs_;
  std::string last_completed_;

  static int preference(const Run &run) {
    if (run.run_type == Run::RT_Aggregate && run.aggregate_name == "median")
      return 2;
    return run.run_type == Run::RT_Iteration ? 1 : 0;
  }

  static double nanoseconds(const Run &run) {
    const double value = run.GetAdjustedCPUTime();
    switch (run.time_unit) {
    case benchmark::kSecond:
      return value * 1'000'000'000.0;
    case benchmark::kMillisecond:
      return value * 1'000'000.0;
    case benchmark::kMicrosecond:
      return value * 1'000.0;
    case benchmark::kNanosecond:
      return value;
    }
    return value;
  }

  static std::optional<double> counter(const Run &run, std::string_view name) {
    const auto value = run.counters.find(std::string{name});
    if (value == run.counters.end())
      return std::nullopt;
    return value->second.value;
  }

  static std::string percentage(double value, double baseline) {
    if (baseline == 0.0)
      return "-";
    std::ostringstream result;
    result << std::fixed << std::setprecision(1) << value * 100.0 / baseline
           << '%';
    return result.str();
  }

  static int implementation_order(std::string_view implementation) {
    if (implementation == "aggregate_return")
      return 0;
    if (implementation == "default_then_assign")
      return 1;
    if (implementation == "reflected")
      return 2;
    return 3;
  }

  static int layout_order(std::string_view layout) {
    return layout == "same_order" ? 0 : 1;
  }

  static int materialization_order(std::string_view group) {
    if (group.starts_with("raw/"))
      return 0;
    if (group.starts_with("ranges_transform/"))
      return 1;
    if (group.starts_with("ranges_to/"))
      return 2;
    return 3;
  }

public:
  compact_report_t(): ConsoleReporter(OO_None) {}

  void ReportRuns(const std::vector<Run> &runs) override {
    if (!runs.empty()
      && last_completed_ != runs.front().run_name.function_name) {
      last_completed_ = runs.front().run_name.function_name;
      GetOutputStream() << "[completed] " << last_completed_ << '\n'
                        << std::flush;
    }
    runs_.insert(runs_.end(), runs.begin(), runs.end());
  }

  void Finalize() override {
    std::map<std::string, const Run *> selected;
    for (const Run &run : runs_) {
      const std::string name = run.run_name.function_name;
      const auto current = selected.find(name);
      if (current == selected.end()
        || preference(*current->second) < preference(run))
        selected[name] = &run;
    }

    std::map<std::pair<int, std::string>, std::vector<implementation_run_t>>
      groups;
    for (const auto &[name, run] : selected) {
      const auto implementation_separator = name.rfind('/');
      const auto layout_separator = implementation_separator == std::string::npos
        ? std::string::npos
        : name.rfind('/', implementation_separator - 1);
      if (layout_separator != std::string::npos) {
        const std::string group = name.substr(0, layout_separator);
        groups[{materialization_order(group), group}].push_back({
          name.substr(implementation_separator + 1),
          name.substr(layout_separator + 1,
            implementation_separator - layout_separator - 1),
          run,
        });
      }
    }

    bool has_instructions = false;
    bool has_cycles = false;
    bool has_bytes = false;
    for (const auto &[group, implementations] : groups) {
      (void)group;
      for (const auto &implementation : implementations) {
        has_instructions |=
          counter(*implementation.run, "instructions").has_value();
        has_cycles |= counter(*implementation.run, "cycles").has_value();
        has_bytes |=
          counter(*implementation.run, "bytes_per_second").has_value();
      }
    }

    auto &output = GetOutputStream();
    output << std::fixed << std::setprecision(2);
    for (auto &[group, implementations] : groups) {
      std::ranges::sort(implementations, [](const auto &left, const auto &right) {
        return std::pair{implementation_order(left.implementation),
                 layout_order(left.layout)}
          < std::pair{implementation_order(right.implementation),
              layout_order(right.layout)};
      });

      output << '\n' << group.second << '\n' << std::left << std::setw(24)
             << "implementation" << std::setw(12) << "layout" << std::right
             << std::setw(12) << "ns/record" << std::setw(12) << "time/base";
      if (has_bytes)
        output << std::setw(12) << "GiB/s";
      if (has_instructions)
        output << std::setw(21) << "instructions/record" << std::setw(12)
               << "ins/base";
      if (has_cycles)
        output << std::setw(16) << "cycles/record" << std::setw(14)
               << "cycles/base";
      if (has_instructions && has_cycles)
        output << std::setw(9) << "IPC";
      output << '\n';

      for (const auto &implementation : implementations) {
        const auto baseline = std::ranges::find_if(implementations,
          [&implementation](const auto &candidate) {
            return candidate.implementation == "aggregate_return"
              && candidate.layout == implementation.layout;
          });
        const Run *const baseline_run = baseline == implementations.end()
          ? nullptr
          : baseline->run;
        const auto baseline_instructions = baseline_run == nullptr
          ? std::nullopt
          : counter(*baseline_run, "instructions");
        const auto baseline_cycles = baseline_run == nullptr
          ? std::nullopt
          : counter(*baseline_run, "cycles");
        const double time = nanoseconds(*implementation.run);
        const auto bytes_per_second =
          counter(*implementation.run, "bytes_per_second");
        const auto instructions = counter(*implementation.run, "instructions");
        const auto cycles = counter(*implementation.run, "cycles");
        output << std::left << std::setw(24) << implementation.implementation
               << std::setw(12) << implementation.layout << std::right
               << std::setw(12) << time << std::setw(12)
               << (baseline_run == nullptr
                    ? "-"
                    : percentage(time, nanoseconds(*baseline_run)));
        if (has_bytes) {
          if (bytes_per_second)
            output << std::setw(12)
                   << *bytes_per_second / (1024.0 * 1024.0 * 1024.0);
          else
            output << std::setw(12) << '-';
        }
        if (has_instructions) {
          if (instructions)
            output << std::setw(21) << *instructions;
          else
            output << std::setw(21) << '-';
          output << std::setw(12)
                 << (instructions && baseline_instructions
                     ? percentage(*instructions, *baseline_instructions)
                     : "-");
        }
        if (has_cycles) {
          if (cycles)
            output << std::setw(16) << *cycles;
          else
            output << std::setw(16) << '-';
          output << std::setw(14)
                 << (cycles && baseline_cycles
                     ? percentage(*cycles, *baseline_cycles)
                     : "-");
        }
        if (has_instructions && has_cycles) {
          if (instructions && cycles && *cycles != 0.0)
            output << std::setw(9) << *instructions / *cycles;
          else
            output << std::setw(9) << '-';
        }
        output << '\n';
      }
    }

    if (!has_instructions) {
      output.flush();
      GetErrorStream()
        << "Hardware instructions were not collected. Configure with "
           "RECORD_CONVERSION_ENABLE_PERF_COUNTERS=ON.\n";
    }
  }
};

} // namespace record_conversion_bench
