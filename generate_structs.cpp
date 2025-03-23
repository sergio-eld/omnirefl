
#include <fmt/core.h>

#include <charconv>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
struct cli_t {
  size_t n;
  std::filesystem::path output_dir;
  std::string header_name;
  std::string cpp_name;
};
} // namespace

int main(int argc, char **argv) {
  // defualt values
  cli_t cli{
    .n = 5,
    .output_dir = std::filesystem::current_path(),
    .header_name = "structs.h",
    .cpp_name = "structs_test.cpp",
  };
  std::vector<std::string_view> str_cli{
    argv,
    std::next(argv, argc),
  };
  for (size_t i = 1; i < str_cli.size(); ++i) {
    if ("-h" == str_cli[i] || "--help" == str_cli[i]) {
      std::cout << fmt::format(
        "Usage: {program} <N> <output_dir> <header_name> <cpp_name>\n\n"
        "N:            number of structs to generate. Default: {n}.\n"
        "output_dir:   output directory to write files to. Default: current working dir.\n"
        "header_name:  name of header file. Default: {header_name}\n"
        "cpp_name:     name of .cpp file. Default: {cpp_name}.\n",
        fmt::arg("n", cli.n),
        fmt::arg("program", std::filesystem::path(str_cli[0]).filename().string()),
        fmt::arg("header_name", cli.header_name),
        fmt::arg("cpp_name", cli.cpp_name));
      return 0;
    }

    switch (i) {
    // cli.n
    case 1: {
      const auto *data = str_cli[i].data();
      const size_t sz = str_cli[i].size();
      const auto &[ptr, ec] = std::from_chars(data, data + sz, cli.n);
      if (std::errc() != ec) {
        std::cerr << fmt::format("Invalid argument N: {}", str_cli[i]);
        return -1;
      }
      break;
    }
    // cli.output_dir
    case 2: {
      // todo: validate
      std::error_code errc{};
      cli.output_dir = std::filesystem::absolute(str_cli[i], errc).lexically_normal();
      if (std::error_code{} != errc) {
        std::cerr << fmt::format("Invalid output_path: {}", str_cli[i]);
      }
      break;
    }
    // cli.header_name
    case 3: {
      cli.header_name = str_cli[i];
      break;
    }
    // cli.cpp_name
    case 4: {
      cli.cpp_name = str_cli[i];
      break;
    }
    default:
      continue;
    } // switch
  } // for

  std::cout << fmt::format(
    "N: {}\n"
    "output_dir: {}\n"
    "header_name: {}\n"
    "cpp_name: {}\n\n",
    cli.n,
    cli.output_dir.string(),
    cli.header_name,
    cli.cpp_name);

  static constexpr const char struct_template[] =
    R"(struct s_{index} {{
{field_type} field;
}};

)";

  std::string structs;
  for (size_t i = 0; i < cli.n; ++i) {
    fmt::format_to( //
      std::back_inserter(structs),
      struct_template,
      fmt::arg("index", i),
      fmt::arg("field_type",
        i != 0 //
          ? fmt::format("s_{}", i - 1)
          : std::string("int")));
  }

  static constexpr const char header_template[] =
    // todo: I need some basic implementation, that will actually use omni api functions, so I could
    // benchmark and profile compilation time.
    R"(#pragma once

struct impl_t {{ 
template <typename T> void operator()(const T &) const noexcept {{}}
}} const static impl{{}};

{structs})";

  std::string header;
  fmt::format_to( //
    std::back_inserter(header),
    header_template,
    fmt::arg("structs", structs));

  static constexpr const char call_template[] = //
    "omni::reflected_call(impl, s_{index}{{}});\n";
  std::string calls;
  for (size_t i = 0; i < cli.n; ++i) {
    fmt::format_to( //
      std::back_inserter(calls),
      call_template,
      fmt::arg("index", i));
  }

  static constexpr const char cpp_template[] =
    R"(#include "{header}"

#include <omnirefl/refl.hpp>

int main() {{

{calls}
return 0;
}}
)";
  std::string cpp;
  fmt::format_to( //
    std::back_inserter(cpp),
    cpp_template,
    fmt::arg("header", cli.header_name),
    fmt::arg("calls", calls));

  if (!std::filesystem::exists(cli.output_dir)) {
    fmt::println("Creating directory {}", cli.output_dir.string());
    std::error_code errc;
    std::filesystem::create_directory(cli.output_dir, errc);
    if (std::error_code{} != errc) {
      std::cerr << fmt::format("Error creating directory: {}", errc.message());
      return -1;
    }
  }

  const auto write_file = [](const auto &out, std::string_view buffer) {
    fmt::println("Generating {}", out.string());
    std::ofstream f{out, std::ios::binary};
    if (!f.is_open() || f.fail()) {
      std::cerr << fmt::format("Failed to create file {}\n", out.string());
    }
    std::cout << buffer;
    f << buffer;
  };

  write_file(cli.output_dir / cli.header_name, header);
  write_file(cli.output_dir / cli.cpp_name, cpp);

  return 0;
}
