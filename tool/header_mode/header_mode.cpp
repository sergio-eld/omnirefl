
#include "tool/header_mode.h"
#include "tool/header_mode/transforms.h"

#include "tool/ast.hpp"
#include "tool/ast/util.h"
#include "tool/cli.hpp"
#include "tool/reflection.hpp"
#include "tool/util.hpp"

#include <clang/Tooling/CompilationDatabase.h>
#include <fmt/core.h>

#include <fstream>
#include <utility>

namespace {
using namespace tool::header_mode;

// refactorme: use map transform
std::set<match::type_dependency, std::less<>> collect_dependency_types(
  const std::set<tool::refl::type_id> &resolved_types,
  const clang::SourceManager &sm,
  // todo: not only `CXXRecordDecl`
  const clang::CXXRecordDecl &reflected_type_decl) {
  using match::type_dependency;
  std::set<type_dependency, std::less<>> dependency_types;

  for (const clang::CXXRecordDecl *_rdecl :
    util::ast::recursively_collect_dependency_types(reflected_type_decl,
      resolved_types)) {
    const clang::CXXRecordDecl &rdecl = *_rdecl;
    const std::string nm_qual_type = rdecl.getQualifiedNameAsString();

    dependency_types.emplace(type_dependency{
      // todo: do not use nm_qual_type to uniquely identify the type
      .id = nm_qual_type,
      .reflected_data =
        {
          .definition = util::ast::resolve_definition(rdecl, sm),
          .fields = util::ast::resolve_struct_fields(rdecl.fields()),
        },
    });
  }

  return dependency_types;
}

template <template <typename, typename...> class List, typename... T>
void print_type_dependencies(const List<match::type_dependency, T...> &types) {
  fmt::println("{}",
    util::join(types,
      "\n",
      [](const match::type_dependency &d, fmt::context &ctx) {
        return fmt::format_to(ctx.out(),
          "",
          d.id,
          d.reflected_data.definition.nm_qual_type.value_or("(unnamed)"));
      }));
}

// refactorme: use map transform
std::vector<std::string> fields_as_strings(
  std::vector<tool::refl::struct_field_data> fields) {
  std::vector<std::string> r;
  for (auto &&f : fields)
    r.emplace_back(std::move(f.name));

  return r;
}
} // namespace

namespace tool {

tl::expected<tl::monostate, std::string> header_mode::run_pipeline(
  const cli::inplace_mode &mode,
  const cli::options &cli,
  const std::vector<std::filesystem::path> &sources,
  const clang::tooling::CompilationDatabase &compilation_db) {
  const tu_pipeline run_pipeline{
    .resource_dir = cli.resource_dir,
    .compilation_db = compilation_db,
    .verbosity = cli.verbosity,
    .mode = mode,

    .transforms =
      std::tuple{
        node_transform{
          .match_node = match::reflected_indexed_type{},
          .resolve_node = transforms::resolve_indexed_type_node,
          .fold_result = transforms::fold_indexed_type_result,
        },
      },
  };

  std::vector<std::string> errors;
  for (const auto &[src, n_processing] : util::indexed(sources)) {
    if (cli::verbosity_level::info & cli.verbosity) {
      fmt::println("[{}/{}] running header mode for file: {}\t\r",
        n_processing + 1,
        sources.size(),
        src.string());
    }

    tl::expected tu_reflected_data = run_pipeline(
      transforms::tu_data{
        ._verbosity = cli.verbosity,
      },
      src);
    if (!tu_reflected_data) {
      if (cli::print_debug(cli.verbosity)) {
        fmt::println("DEBUG: error running pipeline for file {}: {}",
          src.string(),
          tu_reflected_data.error());
      }
      errors.emplace_back(fmt::format("{}: AST transform failed with error: {}",
        src.string(),
        std::move(tu_reflected_data).error()));
      continue;
    }

    const std::filesystem::path output_file =
      mode.output_dir / src.stem().replace_extension(".hpp");
    if (cli::verbosity_level::info & cli.verbosity) {
      fmt::println("Generating reflection header: {} -> {}",
        src.string(),
        output_file.string());
    }

    const auto output =
      codegen::prepare_data(std::move(tu_reflected_data).value());
    if (!output) {
      errors.emplace_back(
        fmt::format("{}: failed to prepare codegen data with error: {}",
          src.string(),
          std::move(output).error()));
      continue;
    }

    std::ofstream f{output_file, std::ios::binary};
    if (const auto res = codegen::emit_reflection_header_file({}, f, *output);
      !res) {
      errors.emplace_back(
        fmt::format("{}: failed to generate header with error: {}",
          src.string(),
          std::move(output).error()));
      continue;
    };

    if (cli::verbosity_level::info & cli.verbosity) {
      fmt::println("Successfully generated {}", output_file.string());
    }
  }

  if (!errors.empty()) {
    return tl::unexpected(fmt::format("Errors occurred: {}",
      util::join(errors, "\n", [] { return "{}"; })));
  }

  return {};
}

namespace header_mode {
// testme:
//   reflected_call with non-reflectable type (i.e. std::) as a reflected type
//
// testme:
//   reflected_call with unnamed/local struct
//
// testme:
//   reflected_call with non-local unnamed type
//
// testme:
//   reflected_call with non-reflectable types (fundamental, unions, C-array)
//
// testme:
//   test indexed reflection when indexed type is a field in another (possibly,
//   also indexed) reflected type
auto match::reflected_indexed_type::resolve(const node_type &node,
  const clang::ASTUnit &ast,
  const std::set<refl::type_id> &resolved_types,
  [[maybe_unused]] bool print_debug) -> tl::expected<result, std::string> {
  using namespace refl;

  // omni::detail::_reflected_indexed_type<typename reflected_type, int
  // type_index>
  const clang::ClassTemplateSpecializationDecl &template_decl = node;
  tl::expected _reflected_type =
    util::ast::get_template_type_arg(template_decl, 0);
  if (!_reflected_type)
    return tl::unexpected(std::move(_reflected_type).error());

  tl::expected _type_index =
    util::ast::get_template_value_arg(template_decl, 1);
  if (!_type_index)
    return tl::unexpected(std::move(_type_index).error());

  const clang::Type &reflected_type = **_reflected_type;
  const size_t type_index = *_type_index;

  if (reflected_type.isFundamentalType()) {
    return non_reflectable{
      .id = reflected_type
        // fixme:
        //   I don't think it does what I need - (namespace-qualified typename)
        .getTypeClassName(),
      .index = type_index,
      .definition = std::nullopt,
      .type_dependencies = {},
    };
  }

  // fixme: handle unions, C-arrays, (maybe) pointers
  if (!reflected_type.isStructureOrClassType()) {
    return tl::unexpected(fmt::format("unsupported type {} in {}",
      // fixme:
      //   I don't think it does what I need - (namespace-qualified typename)
      reflected_type.getTypeClassName(),
      template_decl.getName()));
  }

  const clang::CXXRecordDecl &reflected_type_decl =
    *reflected_type.getAsCXXRecordDecl();
  const std::string nm_qual_type =
    reflected_type_decl.getQualifiedNameAsString();

  if (!reflected_type_decl.hasDefinition()) {
    return tl::unexpected(
      fmt::format("forward declarations are not allowed: {}", nm_qual_type));
  }

  // todo: do not use nm_qual_type to uniquely identify the type
  if (resolved_types.contains(nm_qual_type))
    return already_reflected{
      .id = nm_qual_type,
    };

  // fixme: non_reflectable

  return reflectable{
    .id = nm_qual_type,
    .index = type_index,
    .reflected_data =
      {
        .definition = util::ast::resolve_definition(reflected_type_decl,
          ast.getSourceManager()),
        .fields =
          util::ast::resolve_struct_fields(reflected_type_decl.fields()),
      },
    .type_dependencies = ::collect_dependency_types(resolved_types,
      ast.getSourceManager(),
      reflected_type_decl),
  };
}

auto transforms::fold_indexed_type_result(tu_data _accum,
  match::reflected_indexed_type::result _result)
  -> tl::expected<tu_data, std::string> {
  tl::expected<tu_data, std::string> accum{std::move(_accum)};

  // refactorme: use pattern matcher
  return std::visit(
    [verbosity = _accum._verbosity, &accum](
      auto &&_result) -> tl::expected<tu_data, std::string> {
      using namespace match;
      using result_t = std::decay_t<decltype(_result)>;

      if constexpr (std::is_same_v<already_reflected, result_t>) {
        if (cli::print_debug(verbosity)) {
          fmt::println("already resolved type {}",
            static_cast<const already_reflected>(_result).id);
        }
      }

      if constexpr (std::is_same_v<reflectable, result_t>) {
        auto &&result = static_cast<reflectable &&>(_result);

        if (cli::verbosity_level::parsed_types & verbosity) {
          fmt::println("resolved indexed type {}:{}({})",
            result.index,
            result.reflected_data.definition.nm_qual_type.value_or("(unnamed)"),
            result.id);
        }

        if (const auto found = accum->indexed_types.find(result.index);
          accum->indexed_types.cend() != found) {
          const auto &[_, existing] = *found;
          const auto &[type_id, existing_reflected] = existing;
          // fixme:
          // return tl::unexpected(fmt::format(
          //   "detected index {} conflict for types (existing) {}:{} and
          //   (resolved) {}", result.index, existing_id, result.id));
        }

        accum->indexed_types[result.index] = {result.id,
          std::move(result.reflected_data)};

        // todo: validate/assert
        accum->resolved_types.emplace(result.id);
      }

      if constexpr (std::is_same_v<reflectable, result_t>
        || std::is_same_v<non_reflectable, result_t>) {
        static_assert(!std::is_same_v<already_reflected, result_t>);

        if (cli::verbosity_level::parsed_types & verbosity)
          ::print_type_dependencies(_result.type_dependencies);

        while (!_result.type_dependencies.empty()) {
          auto _extracted = _result.type_dependencies.extract(
            _result.type_dependencies.begin());
          // todo: is this correct?
          type_dependency &&d = std::move(_extracted.value());
          accum->type_dependencies.emplace(std::move(d).reflected_data);
          // todo: error? or debug log if already resolved, but it shouldn't be
          // possible
          accum->resolved_types.emplace(d.id);
        }
      }

      return accum;
    },
    std::move(_result));
}

auto codegen::prepare_data(transforms::tu_data data)
  -> tl::expected<reflection_data, std::string> {
  const auto is_forward_declarable =
    [](const match::reflected_type_data &reflected) -> bool {
    using td_flags = refl::type_definition_flags;
    return !((td_flags::local | td_flags::non_public)
      & reflected.definition.definition_flags);
  };
  const auto verbosity = data._verbosity;

  std::set<forward_declarable, std::less<>> forward_declarable_types;

  // refactorme: fn (indexed) -> forward_declarable, indexed
  for (auto i = data.indexed_types.begin(); i != data.indexed_types.end();) {
    // _every_ indexed type can be reflected (can't it?),
    // but not if it can be forward-declared, it absolutely should be.
    auto &&[index, pair_type_id_reflected] = *i;
    auto &&[type_id, reflected] = pair_type_id_reflected;

    if (!is_forward_declarable(reflected)) {
      i = std::next(i);
      continue;
    }

    assert(reflected.definition.nm_qual_type
      && "unnamed type can't be forward declared");

    if (cli::verbosity_level::parsed_types & verbosity) {
      fmt::println(
        "indexed ({}) type {}:{} will be reflected via forward declaration.",
        index,
        type_id,
        *reflected.definition.nm_qual_type);
    }

    forward_declarable_types.emplace(forward_declarable{
      .nm_qual_type = *reflected.definition.nm_qual_type,
      .fields = ::fields_as_strings(std::move(reflected.fields)),
      .is_class = reflected.definition.is_class,
    });

    auto next = std::next(i);
    data.indexed_types.extract(i);
    i = next;
  }

  while (!data.type_dependencies.empty()) {
    auto extracted =
      data.type_dependencies.extract(data.type_dependencies.begin());
    // todo: is this correct?
    match::reflected_type_data &&t = std::move(extracted.value());
    if (is_forward_declarable(t)) {
      assert(
        t.definition.nm_qual_type && "unnamed type can't be forward declared");
      forward_declarable_types.emplace(forward_declarable{
        .nm_qual_type = *t.definition.nm_qual_type,
        .fields = ::fields_as_strings(std::move(t.fields)),
        .is_class = t.definition.is_class,
      });
      continue;
    }

    // todo:
    //   implement reflection for type dependencies or report an error. I think
    //   all of the type dependencies (at least member fields) can be reflected
    //   via `decltype(T::field)`
    if (cli::verbosity_level::parsed_types & verbosity) {
      // fixme: type_id
      fmt::println("Skipping non-forward declarable dependency type {}",
        t.definition.nm_qual_type.value_or("unnamed"));
    }
  }

  return reflection_data{
    .forward_declarable_types = std::move(forward_declarable_types),
    .reflected_indexed_types =
      [](auto indexed_types) {
        std::map<size_t /*type_index*/, std::vector<std::string> /*fields*/>
          reflected_indexed_types;
        for (auto &&[index, pair_type_id_reflected] : indexed_types) {
          auto &&[_, reflected] = pair_type_id_reflected;
          reflected_indexed_types[index] =
            fields_as_strings(std::move(reflected.fields));
        }
        return reflected_indexed_types;
      }(std::move(data.indexed_types)),
  };
}

} // namespace header_mode
} // namespace tool
