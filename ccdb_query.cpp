#include <CLI/CLI.hpp>

#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/JSONCompilationDatabase.h>

#include <algorithm>
#include <cctype>
#include <expected>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "std_compat.h"

namespace {

using namespace std::string_view_literals;

namespace fs = std::filesystem;

struct cli_path {
  fs::path value;
};

bool lexical_cast(const std::string &arg, cli_path &out) {
  out = {.value = fs::path{arg}};
  return true;
}

std::string shell_quote(std::string_view arg) {
  if (arg.empty())
    return "''";

  const bool needs_quote = std::ranges::any_of(arg, [](const char c) {
    return std::isspace(static_cast<unsigned char>(c))
      || "'\"\\$`|&;<>()[{}]*?!#"sv.contains(c);
  });

  const auto escaped = arg //
    | std::views::transform([](const char &c) -> std::string_view {
        return '\\' == c //
          ? "\\\\"sv
          : '\'' == c //
            ? "'\\''"sv
            : std::string_view{&c, 1};
      }) //
    | std::views::join //
    | std::ranges::to<std::string>();

  return needs_quote ? std::format("'{}'", escaped) : escaped;
}

bool is_drive_mount_path(std::string_view path) {
  return 3 <= path.size() && '/' == path[0]
    && std::isalpha(static_cast<unsigned char>(path[1])) && '/' == path[2];
}

std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
  load_compile_db(const fs::path &db_path) {
  std::string err;
  const std::string path = db_path.generic_string();

  // A Cosmopolitan process exposes C:/... as /C/.... LLVM was configured on
  // Linux, so automatic command-line detection would otherwise use GNU
  // escaping and discard backslashes from CMake's Windows command strings.
  std::unique_ptr<clang::tooling::CompilationDatabase> loaded =
    clang::tooling::JSONCompilationDatabase::loadFromFile(path,
      err,
      is_drive_mount_path(path) //
        ? clang::tooling::JSONCommandLineSyntax::Windows
        : clang::tooling::JSONCommandLineSyntax::AutoDetect);

  if (!loaded)
    return std::unexpected(std::format("failed to load compilation db {}: {}",
      db_path.generic_string(),
      err));

  return loaded;
}

struct options {
  fs::path compile_commands;
  fs::path source;
  std::string output_contains;
};

std::expected<clang::tooling::CompileCommand, std::string> resolve_command(
  const options &o) {
  std::expected db = load_compile_db(o.compile_commands);
  if (!db)
    return std::unexpected(std::move(db).error());

  // CMake may use either path separator in Windows compile databases.
  // Cosmopolitan additionally exposes C:/... as /C/....
  const auto normalize_path = [](std::string_view path) {
    std::string normalized = path //
      | std::views::transform(
        [](const char c) { return '\\' == c ? '/' : c; }) //
      | std::ranges::to<std::string>();

    if (3 <= normalized.size() && '/' == normalized[0]
      && std::isalpha(static_cast<unsigned char>(normalized[1]))
      && '/' == normalized[2]) {
      normalized[0] = normalized[1];
      normalized[1] = ':';
    }

    if (2 <= normalized.size() && ':' == normalized[1])
      normalized[0] = static_cast<char>(
        std::tolower(static_cast<unsigned char>(normalized[0])));

    return normalized;
  };

  std::vector commands =
    (*db)->getCompileCommands(o.source.generic_string());
  if (commands.empty()) {
    const std::string source = normalize_path(o.source.generic_string());
    commands = (*db)->getAllCompileCommands() //
      | std::views::filter([&source, normalize_path](const auto &command) {
          return source == normalize_path(command.Filename);
        }) //
      | std::ranges::to<std::vector>();
  }

  const std::string output_contains = normalize_path(o.output_contains);

  std::vector resolved = commands //
    | std::views::filter([&output_contains, normalize_path](
                           const clang::tooling::CompileCommand &c) {
        return normalize_path(c.Output).contains(output_contains);
      })
    | std::ranges::to<std::vector>();

  if (1 == resolved.size()) {
    clang::tooling::CompileCommand command = std::move(resolved.front());
    if (is_drive_mount_path(o.compile_commands.generic_string())) {
      for (std::string &arg : command.CommandLine) {
        for (std::size_t index = 0; index + 2 < arg.size(); ++index) {
          if (!std::isalpha(static_cast<unsigned char>(arg[index]))
            || ':' != arg[index + 1]
            || !"/\\"sv.contains(arg[index + 2]))
            continue;

          const char drive = arg[index];
          arg[index] = '/';
          arg[index + 1] = drive;
          arg[index + 2] = '/';
          std::ranges::replace(
            std::span{arg}.subspan(index + 3), '\\', '/');
          break;
        }
      }
    }

    return command;
  }

  if (resolved.empty()) {
    return std::unexpected(
      std::format("{}: no compile command from {} candidate(s) matches output "
                  "substring `{}`",
        o.source.generic_string(),
        commands.size(),
        o.output_contains));
  }

  return std::unexpected(
    std::format("{}: ambiguous compile command: {} command(s) from {} "
                "candidate(s) match output substring `{}`",
      o.source.generic_string(),
      resolved.size(),
      commands.size(),
      o.output_contains));
}

std::expected<options, std::string> parse(int argc, const char *const *argv) {
  CLI::App app{
    "\nQuery one command from compile_commands.json."
    "\n"
    "\nUsage: ccdb_query <compile_commands.json> <source.cpp> [output-contains]",
  };
  app.allow_windows_style_options(false);

  CLI::Option *compile_commands =
    app.add_option("compile_commands", "Path to compile_commands.json.")
      ->type_name("FILE")
      ->check(CLI::ExistingFile)
      ->required();

  CLI::Option *source = //
    app.add_option("source", "Source file to query.")
      ->type_name("FILE")
      ->required();

  CLI::Option *output_contains = //
    app
      .add_option("output_contains",
        "Substring used to disambiguate compile command output.")
      ->type_name("TEXT")
      ->default_val(std::string{});

  try {
    app.parse(argc, argv);
  } catch (const CLI::CallForHelp &e) {
    app.exit(e);
    return std::unexpected(std::string{});
  } catch (const CLI::ParseError &e) {
    return std::unexpected(std::string{e.what()});
  }

  fs::path parsed_source =
    fs::absolute(source->as<cli_path>().value).lexically_normal();

  if (!fs::exists(parsed_source)) {
    return std::unexpected(
      std::format("source does not exist: {}", parsed_source.generic_string()));
  }

  return options{
    .compile_commands =
      fs::absolute(compile_commands->as<cli_path>().value).lexically_normal(),
    .source = std::move(parsed_source),
    .output_contains = output_contains->as<std::string>(),
  };
}

} // namespace

int main(int argc, const char *const *argv) {
  return parse(argc, argv)
    .and_then(resolve_command)
    .transform([](const clang::tooling::CompileCommand &c) {
      std::cout << std::format("{} -working-directory {}\n",
        c.CommandLine //
          | std::views::transform(shell_quote) //
          | std_c::views::join_with(" "sv) //
          | std::ranges::to<std::string>(),
        shell_quote(c.Directory));
      return 0;
    })
    .or_else(
      [](const std::string &error) -> std::expected<int, std::monostate> {
        if (!error.empty())
          std::cerr << error << '\n';
        return error.empty() ? 0 : -1;
      })
    .value();
}
