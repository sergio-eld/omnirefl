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
#include <vector>

namespace {

using namespace std::string_view_literals;

namespace fs = std::filesystem;

std::string shell_quote(std::string_view arg) {
  if (arg.empty())
    return "''";

  const bool needs_quote = std::ranges::any_of(arg, [](const char c) {
    return std::isspace(static_cast<unsigned char>(c))
      || "'\"\\$`|&;<>()[{}]*?!#"sv.contains(c);
  });

  return needs_quote
    ? std::format("'{}'",
        arg //
          | std::views::transform([](const char &c) -> std::string_view {
              return '\'' == c //
                ? "'\\''"sv
                : std::string_view{&c, 1};
            }) //
          | std::views::join //
          | std::ranges::to<std::string>())
    : std::string{arg};
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

std::expected<clang::tooling::CompileCommand, std::string> resolve_command(
  const fs::path &db_path,
  const fs::path &source,
  std::string_view output_contains) {
  std::expected db = load_compile_db(db_path);
  if (!db)
    return std::unexpected(std::move(db).error());

  const std::vector commands =
    (*db)->getCompileCommands(source.generic_string());

  std::vector resolved = commands //
    | std::views::filter(
      [&output_contains](const clang::tooling::CompileCommand &c) {
        return c.Output.contains(output_contains);
      })
    | std::ranges::to<std::vector>();

  if (1 == resolved.size())
    return std::move(resolved.front());

  if (resolved.empty()) {
    return std::unexpected(
      std::format("{}: no compile command from {} candidate(s) matches output "
                  "substring `{}`",
        source.generic_string(),
        commands.size(),
        output_contains));
  }

  return std::unexpected(
    std::format("{}: ambiguous compile command: {} command(s) from {} "
                "candidate(s) match output substring `{}`",
      source.generic_string(),
      resolved.size(),
      commands.size(),
      output_contains));
}

struct options {
  fs::path compile_commands;
  fs::path source;
  std::string output_contains;
};

std::expected<options, std::string> parse(int argc, const char *const *argv) {
  CLI::App app{
    "\nQuery one command from compile_commands.json."
    "\n"
    "\nUsage: ccdb_query <compile_commands.json> <source.cpp> [output-contains]",
  };

  options o;

  app
    .add_option("compile_commands",
      o.compile_commands,
      "Path to compile_commands.json.")
    ->type_name("FILE")
    ->check(CLI::ExistingFile)
    ->required();

  app.add_option("source", o.source, "Source file to query.")
    ->type_name("FILE")
    ->required();

  app
    .add_option("output_contains",
      o.output_contains,
      "Substring used to disambiguate compile command output.")
    ->default_val(std::string{});

  try {
    app.parse(argc, argv);
  } catch (const CLI::CallForHelp &e) {
    app.exit(e);
    return std::unexpected(std::string{});
  } catch (const CLI::ParseError &e) {
    return std::unexpected(std::string{e.what()});
  }

  o.compile_commands =
    fs::absolute(std::move(o.compile_commands)).lexically_normal();
  o.source = fs::absolute(std::move(o.source)).lexically_normal();

  if (!fs::exists(o.source)) {
    return std::unexpected(
      std::format("source does not exist: {}", o.source.generic_string()));
  }

  return o;
}

} // namespace

int main(int argc, const char *const *argv) {
  return parse(argc, argv)
    .and_then([](const options &o) {
      return resolve_command(o.compile_commands, o.source, o.output_contains);
    })
    .transform([](const clang::tooling::CompileCommand &c) {
      std::cout << (c.CommandLine //
        | std::views::transform(shell_quote) //
        | std::views::join_with(" "sv) //
        | std::ranges::to<std::string>())
                << '\n';
      return 0;
    })
    .or_else([](const std::string &error) -> std::expected<int, std::string> {
      if (!error.empty())
        std::cerr << error << '\n';
      return error.empty() ? 0 : -1;
    })
    .value();
}
