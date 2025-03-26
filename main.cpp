
// todo:
// - ast caching (?)

// todo:
// #include "tool/..." // - tool related code
// #include "refl/..." // - reflection related code
// #include "plugin/..." // - plugin related code

#include "fmt/base.h"
#include "fmt/format.h"
#include "tool/ast.hpp"
#include "tool/cli.hpp"
#include "tool/emit_code.hpp"
#include "tool/reflection.hpp"
#include "tool/util.hpp"

#include <clang/Tooling/CompilationDatabase.h>

#include <fmt/core.h>
#include <tl/expected.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
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
  // todo: move this to normalizing?
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
      _mode.output_dir = std::filesystem::absolute(std::move(_mode.output_dir))
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
  if (cli::verbosity_level::input & cli->verbosity)
    fmt::println("{}", cli::to_string(*cli));

  // todo: map sources to compilation commands, do not use compilation DB
  // further down the line
  // todo: when implementing extracting compile commands, for in-place mode
  // remove generated includes
  const tl::expected sources = std::visit(
    [&sources = cli->sources](const auto &_v)
      -> tl::expected<
        // fixme: ad hoc until the pipeline depends on CompilationDatabase
        std::pair<std::vector<std::filesystem::path>,
          std::unique_ptr<clang::tooling::CompilationDatabase>>,
        std::string> {
      using type = std::decay_t<decltype(_v)>;
      if constexpr (!std::is_same_v<cli::compilation_db_entry, type>) {
        // todo: support direct compilation options (without
        // compile_commands.json)
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

  // TODO: remove this atrocity (see `sources` evaluation above)
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

  /*
   * // refactorme:
   * either<context, error> ctx_delta =
   *   std::tie(src, n_processing) >>=
   *     | unpack_to(parse_ast)
   *     | match_ast
   *     | foldl(std::move(ctx), resolve_matched_node);
   */
  const auto parse_ast = [&](const std::filesystem::path &src, size_t n) {
    if (cli::verbosity_level::info & cli->verbosity) {
      fmt::println( //
        "[{}/{}] building AST of file: {}\t\r",
        n,
        src_files.size(),
        src.string());
    }

    return tool::parse_ast_from_source( //
      cli->resource_dir,
      src,
      *compilation_db,
      cli::verbosity_level::debug & cli->verbosity);
  };

  tool::refl::context ctx{};
  std::vector<std::string> errors;

  // refactorme: this should only be used/available for in-place mode
  std::map<std::filesystem::path, std::unordered_map<int, std::string>>
    type_indexes_for_cpp;

  // TODO: support in-place mode. For in-place mode, unlike the target mode, N
  // headers will be generated, where N is equal to input reflected .cpp files.
  // But the reflected data in the context should be reused (would it be
  // correct?).
  for (const auto &[src, n_processing] : util::indexed(src_files)) {
    tl::expected ast = parse_ast(src, n_processing + 1);
    if (!ast) {
      llvm::errs() << ast.error();
      return -1;
    }

    // refactorme: should it be a 'table' {match{}, resolve{}}?
    // this would allow to untie the match type from context type and provide
    // different variants of implementations
    using ast_match = tool::matched_node_variant<
      // refactorme: should this run only in the `inplace-reflection` mode?
      tool::refl::matches::reflected_type_index,
      tool::refl::matches::reflected_type,
      tool::refl::matches::reflected_impl,
      tool::refl::matches::reflected_call,
      tool::refl::matches::_debug_templ_spec_decl>;
    tl::expected matches = tool::match_ast_nodes( //
      std::vector<ast_match>{},
      **ast,
      cli::verbosity_level::debug & cli->verbosity);
    if (!matches) {
      llvm::errs() << matches.error();
      return -1;
    }

    for (const ast_match &m : *matches) {
      tl::expected ctx_delta = //
        tool::refl::resolve_matched_node(m,
          **ast,
          ctx,
          cli::verbosity_level::debug & cli->verbosity);

      if (!ctx_delta) {
        errors.emplace_back(std::move(ctx_delta).error());
        continue;
      }

      tl::expected updated = tool::refl::update(std::move(ctx), //
        std::move(ctx_delta).value(),
        cli::verbosity_level::debug & cli->verbosity);

      if (!updated) {
        std::cerr //
          << fmt::format("Error updating context: {}",
               std::move(updated).error());
        return -1;
      }

      ctx = std::move(updated).value();
    }

    // todo: errors, add flag to stop on errors
    if (!errors.empty()) {
      std::cerr //
        << fmt::format("Errors while matching nodes: {}",
             fmt::join(errors, ", "));
      return -1;
    }

    // refactorme: this should not be part of the context in non-inline
    // reflection mode
    type_indexes_for_cpp[src] = std::move(ctx.reflected_types_indexes);
  }

  auto validated_reflection_data = codegen::prepare_input(std::move(ctx));
  if (!validated_reflection_data) {
    llvm::errs() << validated_reflection_data.error();
    return -1;
  }

  const tl::expected tool_result = std::visit(
    [&](const auto &_mode)
      -> tl::expected<std::string, std::pair<std::string, int>> {
      using mode_type = std::decay_t<decltype(_mode)>;
      if constexpr (std::is_same_v<cli::target_mode, mode_type>) {
        const cli::target_mode &mode = _mode;

        // todo: validation?
        const std::filesystem::path output_file =
          mode.output_dir / mode.output_file;

        if (cli::verbosity_level::info & cli->verbosity)
          fmt::println("Generating file: {}\n", output_file.string());

        std::ofstream f{output_file, std::ios::binary};
        if (const auto res = codegen::emit_reflection_cpp_file(
              {
                // todo: options
              },
              f,
              *validated_reflection_data);
          !res) {
          return tl::unexpected(std::pair{res.error(), -1});
        };

        return fmt::format("Successfully generated {}.", output_file.string());
      } else {
        static_assert(std::is_same_v<cli::inplace_mode, mode_type>);

        // todo: implement
        return tl::unexpected(
          std::pair{std::string("In-place mode not implemented"), -1});
      }
    },
    cli->mode);

  if (!tool_result) {
    const auto &[msg, code] = tool_result.error();
    std::cerr << fmt::format("Error writing reflected files: {}\n", msg);
    return code;
  }

  if (cli::verbosity_level::info & cli->verbosity)
    fmt::println("{}", *tool_result);

  return 0;
}
