
// todo:
// - ast caching (?)

// todo:
// #include "tool/..." // - tool related code
// #include "refl/..." // - reflection related code
// #include "plugin/..." // - plugin related code

#include "tool/ast.hpp"
#include "tool/cli.hpp"
#include "tool/header_mode.h"
#include "tool/source_mode.h"
#include "tool/util.hpp"

#include <clang/Tooling/CompilationDatabase.h>

#include <fmt/core.h>
#include <tl/expected.hpp>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
namespace cli = tool::cli;

tl::expected<std::vector<std::filesystem::path>, std::string>
  reflected_sources_from_compilation_db(
    const clang::tooling::CompilationDatabase &compilation_db,
    bool exclude,
    std::vector<std::filesystem::path> sources) noexcept {
  // refactorme: move this to normalizing?
  sources = util::sorted(std::less{},
    util::filtered(
      [](const std::filesystem::path &p) -> bool { return p.empty(); },
      std::move(sources)));

  tl::expected db_sources = util::to_std_paths(compilation_db.getAllFiles());
  if (!db_sources)
    return tl::unexpected(std::move(db_sources).error());

  if (exclude) {
    fmt::println("DEBUG: using sources to filter compilation DB files.");
    // todo: benchmark and compare with set_difference
    return {util::filtered(
      [&excluded = std::as_const(sources)](
        const std::filesystem::path &db_path) -> bool {
        for (const std::filesystem::path &e : excluded) {
          if (util::is_subpath(db_path.lexically_normal(),
                std::filesystem::absolute(e).lexically_normal())) {
            return true;
          }
        }
        return false;
      },
      std::move(db_sources).value())};
  }

  if (sources.empty())
    return {std::move(db_sources).value()};

  // todo: benchmark && optimize
  std::vector<std::filesystem::path> not_found_in_db;
  for (const auto &p : sources) {
    if (db_sources->cend()
      != std::find_if(db_sources->cbegin(),
        db_sources->cend(),
        [&p](const std::filesystem::path &db_path) -> bool {
          return util::is_subpath(db_path.lexically_normal(),
            std::filesystem::absolute(p).lexically_normal());
        }))
      continue;
    not_found_in_db.emplace_back(p);
  }

  if (not_found_in_db.empty())
    return {std::move(sources)};

  return tl::unexpected<std::string>(fmt::format("[{}]",
    util::join(not_found_in_db,
      ",\n",
      [](const std::filesystem::path &p, fmt::context &ctx) {
        return fmt::format_to(ctx.out(), "{}", p.string());
      })));
}

// convert to absolute paths
cli::options normalize_paths(cli::options o) noexcept {
  o.resource_dir =
    std::filesystem::absolute(std::move(o.resource_dir)).lexically_normal();
  for (auto &s : o.sources)
    s = std::filesystem::absolute(std::move(s)).lexically_normal();

  std::visit(
    [](auto &_mode) {
      _mode.output_dir = //
        std::filesystem::absolute(std::move(_mode.output_dir))
          .lexically_normal();
    },
    o.mode);

  std::visit(
    [](auto &_v) {
      using type = std::decay_t<decltype(_v)>;
      if constexpr (std::is_same_v<type, cli::compilation_db_entry>) {
        cli::compilation_db_entry &db = _v;
        db.path =
          std::filesystem::absolute(std::move(db.path)).lexically_normal();
      }
    },
    o.cl_flags);

  return o;
}

} // namespace

// allowing to configure specific things:
// - matchers
// - output to file
// - what else
int main(int argc, char **argv) {
  tl::expected cli = cli::parse(argc, argv);
  if (!cli) {
    const auto &[msg, code] = cli.error();
    (0 == code ? std::cout : std::cerr) << msg << '\n';
    return code;
  }
  if (auto evaluated = cli::evaluate_defaults(*cli)) {
    *cli = ::normalize_paths(std::move(evaluated).value());
  } else {
    std::cerr << evaluated.error() << '\n';
    return -1;
  }
  // todo: validations
  if (cli::verbosity_level::input & cli->verbosity)
    fmt::println("{}", cli::to_string(*cli));

  // todo:
  //   map sources to compilation commands, do not use compilation DB further
  //   down the line
  //
  // todo:
  //   when implementing extracting compile commands, for in-place mode remove
  //   generated includes
  const tl::expected sources = std::visit(
    [&sources = cli->sources](const auto &_v)
      -> tl::expected<
        // fixme: ad hoc until the pipeline depends on CompilationDatabase
        std::pair<std::vector<std::filesystem::path>,
          std::unique_ptr<clang::tooling::CompilationDatabase>>,
        std::string> {
      using type = std::decay_t<decltype(_v)>;
      if constexpr (!std::is_same_v<cli::compilation_db_entry, type>) {
        // todo:
        //   support direct compilation options (without compile_commands.json)
        return tl::unexpected(
          fmt::format("Only compile_commands.json is supported.\n"));
      } else {
        const cli::compilation_db_entry &cli_compilation_db = _v;
        // todo: make const after removing the usage in the actual pipeline
        /*const*/ tl::expected compilation_db =
          tool::load_compilation_db(cli_compilation_db.path);
        if (!compilation_db) {
          return tl::unexpected(
            fmt::format("Error loading compilation DB \"{}\": \"{}\".\n",
              compilation_db.error(),
              cli_compilation_db.path.string()));
        }

        tl::expected result =
          ::reflected_sources_from_compilation_db(**compilation_db,
            cli_compilation_db.filter_paths,
            sources);
        if (!result)
          return tl::unexpected(std::move(result).error());

        // fixme: exclude generated reflected files!!
        return {tl::in_place,
          std::move(result).value(),
          std::move(compilation_db).value()};
      }
    },
    cli->cl_flags);

  if (!sources) {
    std::cerr << fmt::format("Error validating reflected sources list: {}\n",
      sources.error());
    return -1;
  }

  // TODO: remove (compilation_db) this atrocity (see `sources` evaluation
  // above)
  const auto &[src_files, compilation_db] = *sources;

  if (cli::verbosity_level::info & cli->verbosity)
    // fixme: sorting output is weird
    fmt::println("Filtered sources: [{}]",
      util::join(src_files,
        ",\n",
        // refactorme: THIS IS BULLSHIT, I want just
        // `[](const auto& p) { return p.string(); }`
        [](const std::filesystem::path &p, fmt::context &ctx) {
          return fmt::format_to(ctx.out(), "{}", p.string());
        }));

  // fixme:
  const auto success = std::visit(
    [&](const auto &mode) {
      using tool::header_mode::run_pipeline;
      using tool::source_mode::run_pipeline;

      return run_pipeline(mode, *cli, src_files, *compilation_db);
    },
    cli->mode);
  if (!success) {
    std::cerr << fmt::format("{}\n", success.error());
    return -1;
  }
  // todo: logging?
  return 0;
}
