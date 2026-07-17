#include <CLI/CLI.hpp>

#include <clang/Tooling/CompilationDatabase.h>

#include <algorithm>
#include <cctype>
#include <expected>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

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
          ? "'\\\\'"sv
          : '\'' == c //
            ? "'\\''"sv
            : std::string_view{&c, 1};
      }) //
    | std::views::join //
    | std::ranges::to<std::string>();

  return needs_quote ? std::format("'{}'", escaped) : escaped;
}

std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
  load_compile_db(const fs::path &db_path) {
  std::string err;
  std::unique_ptr loaded =
    clang::tooling::CompilationDatabase::loadFromDirectory(
      db_path.parent_path().generic_string(),
      err);

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

  const std::vector commands =
    (*db)->getCompileCommands(o.source.generic_string());

  // CMake may use either path separator in Windows compile databases.
  const auto normalize_path_separators = [](std::string_view path) {
    return path //
      | std::views::transform(
        [](const char c) { return '\\' == c ? '/' : c; }) //
      | std::ranges::to<std::string>();
  };

  const std::string output_contains =
    normalize_path_separators(o.output_contains);

  std::vector resolved = commands //
    | std::views::filter([&output_contains, normalize_path_separators](
                           const clang::tooling::CompileCommand &c) {
        return normalize_path_separators(c.Output).contains(output_contains);
      })
    | std::ranges::to<std::vector>();

  if (1 == resolved.size())
    return std::move(resolved.front());

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
          | std::views::join_with(" "sv) //
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
