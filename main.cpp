
// todo:
// - ast caching (?)

// todo:
// #include "tool/..." // - tool related code
// #include "refl/..." // - reflection related code
// #include "plugin/..." // - plugin related code

#include "tool/ast.hpp"
#include "tool/cli.hpp"
#include "tool/emit_code.hpp"
#include "tool/reflection.hpp"
#include "tool/util.hpp"

#include <clang/Tooling/CompilationDatabase.h>

#include <fmt/core.h>
#include <memory>
#include <tl/expected.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <unordered_set>
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

tl::expected<tl::monostate, std::string> run_pipeline(
  const cli::target_mode &mode,
  const cli::options &cli,
  const std::vector<std::filesystem::path> &sources,
  const clang::tooling::CompilationDatabase &compilation_db) {
  // for (const auto &[src, n_processing] : util::indexed(src_files)) {
  //   tl::expected ast = parse_ast(src, n_processing + 1);
  //   if (!ast) {
  //     llvm::errs() << ast.error();
  //     return -1;
  //   }

  //   using ast_match = std::variant<
  //     // refactorme: should this run only in the `inplace-reflection` mode?
  //     tool::refl::matches::reflected_indexed_type,

  //     tool::refl::matches::reflected_type,
  //     tool::refl::matches::reflected_impl,
  //     tool::refl::matches::reflected_call,

  //     tool::refl::matches::_debug_templ_spec_decl>;
  //   tl::expected matches = tool::match_ast_nodes( //
  //     std::type_identity<ast_match>{},
  //     **ast,

  //     // refactorme: pass verbosity
  //     tool::cli::print_debug(cli->verbosity));
  //   if (!matches) {
  //     llvm::errs() << matches.error();
  //     return -1;
  //   }

  //   for (const ast_match &m : *matches) {
  //     tl::expected ctx_delta = //
  //       tool::refl::resolve_matched_node(m,
  //         **ast,
  //         ctx,
  //         // refactorme: pass verbosity
  //         tool::cli::print_debug(cli->verbosity));

  //     if (!ctx_delta) {
  //       errors.emplace_back(std::move(ctx_delta).error());
  //       continue;
  //     }

  //     tl::expected updated = tool::refl::update(std::move(ctx), //
  //       std::move(ctx_delta).value(),
  //       // refactorme: pass verbosity
  //       tool::cli::print_debug(cli->verbosity));

  //     if (!updated) {
  //       std::cerr //
  //         << fmt::format("Error updating context: {}",
  //              std::move(updated).error());
  //       return -1;
  //     }

  //     ctx = std::move(updated).value();
  //   }

  //   // todo: errors, add flag to stop on errors
  //   if (!errors.empty()) {
  //     std::cerr //
  //       << fmt::format("Errors while matching nodes: {}",
  //            fmt::join(errors, ", "));
  //     return -1;
  //   }

  // }
  // auto validated_reflection_data = codegen::prepare_input(std::move(ctx),
  // mode); if (!validated_reflection_data) {
  //   return tl::unexpected(
  //     std::pair{fmt::format("Error while validating data: {}",
  //                 std::move(validated_reflection_data).error()),
  //       -1});
  // }

  // // todo: validation?
  // const std::filesystem::path output_file = mode.output_dir /
  // mode.output_file;

  // if (cli::verbosity_level::info & cli->verbosity)
  //   fmt::println("Generating reflection file: {}", output_file.string());

  // std::ofstream f{output_file, std::ios::binary};
  // if (const auto res = codegen::emit_reflection_cpp_file(
  //       {
  //         // todo: options
  //       },
  //       f,
  //       *validated_reflection_data);
  //   !res) {
  //   return tl::unexpected(std::pair{res.error(), -1});
  // };

  // return fmt::format("Successfully generated {}.", output_file.string());

  // todo: implement
  return tl::unexpected(std::string("target mode not implemented"));
}

tl::expected<tl::monostate, std::string> run_pipeline(
  const cli::inplace_mode &mode,
  const cli::options &cli,
  const std::vector<std::filesystem::path> &sources,
  const clang::tooling::CompilationDatabase &compilation_db) {
  namespace matches = tool::refl::matches;

  using reflection_data = codegen::inplace_mode_reflection_data;

  std::unordered_set<std::string> resolved_types;

  const tool::tu_pipeline run_pipeline{
    .resource_dir = cli.resource_dir,
    .compilation_db = compilation_db,
    .verbosity = cli.verbosity,
    .mode = mode,

    .transforms =
      std::tuple{
        tool::node_transform{
          .match_node = matches::reflected_type{},
          .resolve_node = //
          [&resolved_types = std::as_const(resolved_types),
            verbosity =
              cli.verbosity](const matches::reflected_type::node_type &n,
            const clang::ASTUnit &ast)
            -> tl::expected<typename matches::reflected_type::result,
              std::string> {
            return matches::reflected_type::resolve(n,
              ast,
              resolved_types,
              cli::print_debug(verbosity));
          },
          .fold_result = //
          [verbosity = cli.verbosity](reflection_data _accum,
            matches::reflected_type::result r)
            -> tl::expected<reflection_data, std::string> {
            tl::expected<reflection_data, std::string> accum{std::move(_accum)};

            if (cli::verbosity_level::parsed_types & verbosity) {
              if (r.matched_type)
                fmt::println("matched reflected type {}",
                  r.matched_type->first);

              if (!r.matched_type_dependencies.empty()) {
                fmt::println("matched reflected dependency type {}",
                  util::join(r.matched_type_dependencies,
                    "\n",
                    [](const auto &k_val, fmt::context &ctx) {
                      const auto &[name, _] = k_val;
                      return fmt::format_to(ctx.out(), "{}", name);
                    }));
              }
            }

            // todo: populate when implemented/if needed
            (void)accum->refl_includes;

            // refactorme: reduce conversions, reuse data
            const auto to_reflected_type =
              [](std::string name,
                tool::refl::matches::reflected_type_data data)
              -> codegen::reflected_type {
              return {
                .name = std::move(name),
                .fields = std::move(data.fields),
                .is_class = data.definition_data.is_class,
              };
            };

            if (r.matched_type) {
              auto &&[name, data] = *r.matched_type;
              accum->reflected_types.emplace(
                to_reflected_type(std::move(name), std::move(data)));
            }

            for (auto &&[name, data] : r.matched_type_dependencies) {
              // todo: merge with conflicts check
              accum->reflected_types.emplace(
                to_reflected_type(std::move(name), std::move(data)));
            }
            return accum;
          },
        },

        // todo: reflected_indexed_type
      },
  };

  std::unordered_map<std::filesystem::path, reflection_data> reflected_sources;
  for (const auto &[src, n_processing] : util::indexed(sources)) {
    if (cli::verbosity_level::info & cli.verbosity) {
      fmt::println("[{}/{}] running in-place mode for file: {}\t\r",
        n_processing + 1,
        sources.size(),
        src.string());
    }

    tl::expected tu_reflected_data = run_pipeline(reflection_data{}, src);
    if (!tu_reflected_data) {
      if (cli::print_debug(cli.verbosity)) {
        fmt::println("DEBUG: error running pipeline for file {}: {}",
          src.string(),
          tu_reflected_data.error());
      }
      // handle errors
      continue;
    }
    reflected_sources[src] = std::move(tu_reflected_data).value();
  }

  for (const auto &[key_val, n_processing] : util::indexed(reflected_sources)) {
    const auto &[src, reflection_data] = key_val;
    const std::filesystem::path output_file =
      mode.output_dir / src.stem().replace_extension(".hpp");
    if (cli::verbosity_level::info & cli.verbosity) {
      fmt::println("Generating reflection header [{}/{}]: {} -> {}",
        n_processing + 1,
        reflected_sources.size(),
        src.string(),
        output_file.string());
    }

    std::ofstream f{output_file, std::ios::binary};
    if (const auto res = codegen::emit_inplace_reflection_header_file(
          {
            // todo: options
          },
          f,
          reflection_data);
      !res) {
      // refactorme: I didn't stop on errors for when running pipelines. But
      // stop here -> wasting everything else...
      return tl::unexpected(
        fmt::format("Error generating reflection header {}: {}",
          output_file.string(),
          res.error()));
    };

    if (cli::verbosity_level::info & cli.verbosity) {
      fmt::println("Successfully generated {}", output_file.string());
    }
  }

  return {};
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

  const auto success = std::visit(
    [&](const auto &mode) {
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
