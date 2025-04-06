
#include "tool/source_mode.h"
#include "fmt/base.h"
#include "tool/cli.hpp"
#include "tool/source_mode/transforms.h"

#include "tool/ast.hpp"
#include "tool/ast/util.h"
#include "tool/reflection.hpp"
#include "tool/util.hpp"

#include <fmt/core.h>
#include <tl/expected.hpp>
#include <type_traits>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated"
#include <clang/AST/DeclBase.h>
#pragma GCC diagnostic pop

#include <cassert>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace {
using namespace tool::source_mode;
namespace refl = tool::refl;

[[maybe_unused]] bool is_cpp_file(const std::filesystem::path &filename) {
  const auto &ext = filename.extension();
  return ".cpp" == ext || ".cxx" == ext || ".c" == ext;
}

// refactorme: use map transform
std::map<tool::refl::type_id, match::reflected_type_data>
  collect_dependency_types(const std::set<tool::refl::type_id> &resolved_types,
    const clang::SourceManager &sm,
    // todo: not only `CXXRecordDecl`
    const clang::CXXRecordDecl &reflected_type_decl) {
  std::map<tool::refl::type_id, match::reflected_type_data> dependency_types;

  for (const clang::CXXRecordDecl *_rdecl :
    util::ast::recursively_collect_dependency_types(reflected_type_decl,
      resolved_types)) {
    const clang::CXXRecordDecl &rdecl = *_rdecl;
    const std::string nm_qual_type = rdecl.getQualifiedNameAsString();

    // todo: do not use nm_qual_type to uniquely identify the type
    dependency_types[nm_qual_type] = {
      .definition = util::ast::resolve_definition(rdecl, sm),
      .fields = util::ast::resolve_struct_fields(rdecl.fields()),
    };
  }

  return dependency_types;
}

void print_type_dependencies(
  const std::map<refl::type_id, match::reflected_type_data> &types) {
  fmt::println("{}",
    util::join(types, "\n", [](const auto &id_data_pair, fmt::context &ctx) {
      const auto &[id, data] = id_data_pair;
      return fmt::format_to(ctx.out(),
        "resolved as dependency type {}:{}",
        id,
        data.definition.nm_qual_type.value_or("(unnamed)"));
    }));
}

auto fold_resolved(transforms::tu_data _accum,
  match::reflected_type::result _result) noexcept
  -> tl::expected<transforms::tu_data, std::string> {
  return std::visit(
    [&]<typename Result>(Result &&result) {
      const tool::cli::verbosity_level verbosity = _accum._verbosity;
      tl::expected<transforms::tu_data, std::string> accum{std::move(_accum)};
      // todo:
      //   check for confilcts and inconcistencies. Note: within a single TU,
      //   makes sense to check for tool-related expectations (asserts). AST
      //   build errors would be detected way before this code

      // refactorme: use pattern matcher
      using result_type = std::decay_t<Result>;
      if constexpr (!std::is_same_v<match::already_reflected, result_type>) {
        if (tool::cli::verbosity_level::parsed_types & verbosity) {
          ::print_type_dependencies(result.type_dependencies);

          if constexpr (std::is_same_v<match::reflectable, result_type>) {
            fmt::println("resolved type {}:{}",
              result.id,
              *result.reflected_type.definition.nm_qual_type);
          }
        }

        for (auto &&[id, data] : result.type_dependencies) {
          // todo: checks for conflicts
          accum->reflected_dependency_types[id] = std::move(data);
          accum->resolved_types.emplace(std::move(id));
        }

        if constexpr (std::is_same_v<match::reflectable, result_type>) {
          // todo: checks for conflicts
          accum->reflected_types[result.id] = std::move(result.reflected_type);
          accum->resolved_types.emplace(std::move(result.id));
        }
      }

      return accum;
    },
    std::move(_result));
}

auto fold_resolved(transforms::tu_data _accum,
  match::reflected_impl::result result) noexcept
  -> tl::expected<transforms::tu_data, std::string> {
  tl::expected<transforms::tu_data, std::string> accum{std::move(_accum)};
  // todo: check for conflicts

  accum->reflected_impls[result.id] = std::move(result.definition_data);
  accum->resolved_types.emplace(std::move(result.id));
  return accum;
}

auto fold_resolved(transforms::tu_data _accum,
  match::reflected_call::result result) noexcept
  -> tl::expected<transforms::tu_data, std::string> {
  tl::expected<transforms::tu_data, std::string> accum{std::move(_accum)};

  accum->reflected_calls.emplace_back(std::move(result.call_signature));
  accum->includes.merge(std::move(result.includes));
  accum->std_includes.merge(std::move(result.std_includes));

  return accum;
}

} // namespace

namespace tool {

tl::expected<tl::monostate, std::string> source_mode::run_pipeline(
  const cli::source_mode &mode,
  const cli::options &cli,
  const std::vector<std::filesystem::path> &sources,
  const clang::tooling::CompilationDatabase &compilation_db) noexcept {
  const tu_pipeline run_pipeline{
    .resource_dir = cli.resource_dir,
    .compilation_db = compilation_db,
    .verbosity = cli.verbosity,
    .mode = mode,

    .transforms =
      []<typename... Match>(Match... m) {
        return std::tuple{node_transform{
          .match_node = m,
          .resolve_node = transforms::resolve_reflected_node<Match>,
          .fold_result = transforms::fold_resolved_types,
        }...};
      }(match::reflected_type{},
        match::reflected_impl{},
        match::reflected_call{}),
  };

  std::map<std::filesystem::path, transforms::tu_data>
    accum_reflected_data_by_source;
  for (const auto &[src, n_processing] : util::indexed(sources)) {
    if (cli::verbosity_level::info & cli.verbosity) {
      fmt::println("[{}/{}] running source mode for file: {}\t\r",
        n_processing + 1,
        sources.size(),
        src.string());
    }
    tl::expected tu_reflected_data = run_pipeline(
      transforms::tu_data{
        // todo:
        //   reuse resolved types from other TUs. As of now just check for
        //   conflicts when merging later
        .resolved_types = {},
        ._verbosity = cli.verbosity,
      },
      src);
    if (!tu_reflected_data) {
      if (cli::print_debug(cli.verbosity)) {
        fmt::println("DEBUG: error running pipeline for file {}: {}",
          src.string(),
          tu_reflected_data.error());
      }

      return tl::unexpected(
        fmt::format("{}: AST transform failed with error: {}",
          src.string(),
          std::move(tu_reflected_data).error()));
    }
    accum_reflected_data_by_source[src] = std::move(tu_reflected_data).value();
  }

  // todo: merge everything
  const std::filesystem::path output_file = mode.output_dir / mode.output_file;
  const auto output =
    codegen::prepare_data(std::move(accum_reflected_data_by_source));
  if (!output) {
    return tl::unexpected(
      fmt::format("{}: failed to prepare codegen data with error: {}",
        output_file.string(),
        std::move(output).error()));
  }

  std::ofstream f{output_file, std::ios::binary};
  if (const auto res = codegen::emit_reflection_cpp_file({}, f, *output);
    !res) {
    return tl::unexpected(
      fmt::format("{}: failed to generate source file with error: {}",
        output_file.string(),
        std::move(output).error()));
  };

  if (cli::verbosity_level::info & cli.verbosity) {
    fmt::println("Successfully generated {}", output_file.string());
  }

  return {};
}

namespace source_mode {

auto match::reflected_type::resolve(const node_type &node,
  const clang::ASTUnit &ast,
  const std::set<refl::type_id> &resolved_types,
  [[maybe_unused]] bool print_debug) noexcept
  -> tl::expected<result, std::string> {
  using namespace refl;

  // omni::detail::_reflected_type<T>
  const clang::ClassTemplateSpecializationDecl &template_decl = node;
  tl::expected _reflected_type =
    util::ast::get_template_type_arg(template_decl, 0);
  if (!_reflected_type)
    return tl::unexpected(std::move(_reflected_type).error());
  const clang::Type &reflected_type = **_reflected_type;

  if (reflected_type.isFundamentalType()) {
    return non_reflectable{
      .id = reflected_type
        // fixme:
        //   I don't think it does what I need - (namespace-qualified typename)
        .getTypeClassName(),
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

  // todo: do not use nm_qual_type to uniquely identify the type
  if (resolved_types.contains(nm_qual_type)) {
    return already_reflected{
      .id = nm_qual_type,
    };
  }

  // todo: `hasDefinition`
  //   at this point (as of this writing) forward declarations are not allowed.
  //   however, with inplace mode I can check at the end of TU
  if (!reflected_type_decl.hasDefinition()) {
    return tl::unexpected(
      fmt::format("forward declarations are not allowed: {}", nm_qual_type));
  }

  // fixme: non_reflectable

  return reflectable{
    // todo: do not use nm_qual_type to uniquely identify the type
    .id = nm_qual_type,
    .reflected_type =
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

auto match::reflected_impl::resolve(const node_type &node,
  const clang::ASTUnit &ast,
  const std::set<refl::type_id> &resolved_types,
  [[maybe_unused]] bool print_debug) noexcept
  -> tl::expected<result, std::string> {
  using namespace tool::refl;

  // omni::detail::_reflected_impl<T>
  const clang::ClassTemplateSpecializationDecl &template_decl = node;
  tl::expected _reflected_type =
    util::ast::get_template_type_arg(template_decl, 0);
  if (!_reflected_type)
    return tl::unexpected(std::move(_reflected_type).error());
  const clang::Type &reflected_type = **_reflected_type;

  // todo:
  //   `hasDefinition` - at this point (as of this writing) forward declarations
  //   are not allowed.
  if (!reflected_type.isStructureOrClassType()) {
    const std::string_view detail_struct_name = template_decl.getName();
    return tl::unexpected(fmt::format("unsupported type {} in {}",
      // fixme: `getTypeClassName` doesn't do what I thought it does
      reflected_type.getTypeClassName(),
      detail_struct_name));
  }

  // type declaration of <T>
  const clang::CXXRecordDecl &reflected_type_decl =
    *reflected_type.getAsCXXRecordDecl();

  std::string nm_qual_type = reflected_type_decl.getQualifiedNameAsString();
  if (resolved_types.contains(nm_qual_type))
    return {};

  return result{
    // todo: do not use nm_qual_type to uniquely identify the type
    .id = nm_qual_type,
    .definition_data = util::ast::resolve_definition(reflected_type_decl,
      ast.getSourceManager()),
  };
}

auto match::reflected_call::resolve(const node_type &node,
  const clang::ASTUnit &ast) noexcept -> tl::expected<result, std::string> {
  using namespace tool::refl;

  const static auto printing_policy = [] {
    clang::PrintingPolicy p{{}};
    // todo:
    //   do I need to supress the tag here? It is actually needed in template
    //   specialization
    p.SuppressTagKeyword = true;
    p.SuppressScope = false;
    p.PrintCanonicalTypes = true;

    return p;
  }();

  const clang::CXXMethodDecl &refl_call_decl = node;

  func_signature call_signature;
  call_signature.args.reserve(refl_call_decl.parameters().size());
  std::set<std::filesystem::path> std_includes;
  std::set<std::filesystem::path> includes;

  for (const clang::ParmVarDecl *parm_decl : refl_call_decl.parameters()) {
    const clang::QualType _qtype = parm_decl->getType(); //< keep alive
    const auto [type, is_const, ref_type] =
      // refactorme: it can return `func_arg`
      [](const clang::QualType &q) {
        struct _r {
          const clang::Type &type;
          bool is_const;
          reference_type ref_type;
        };

        if (q->isLValueReferenceType()) {
          return _r{
            .type = *q->getPointeeType().getTypePtr(),
            .is_const = q->getPointeeType().isConstQualified(),
            .ref_type = reference_type::ref_lval,
          };
        }
        if (q->isRValueReferenceType()) {
          return _r{
            .type = *q->getPointeeType().getTypePtr(),
            .is_const = q->getPointeeType().isConstQualified(),
            .ref_type = reference_type::ref_rval,
          };
        }
        return _r{
          .type = *q,
          .is_const = q.isConstQualified(),
          .ref_type = reference_type::ref_none,
        };
      }(_qtype);

    call_signature.args.push_back({
      .nm_qual_type = clang::QualType::getAsString(
        [](clang::SplitQualType q) {
          q.Quals.removeCVRQualifiers();
          return q;
        }(parm_decl->getType().getNonReferenceType().split()),
        printing_policy),
      .is_const = is_const,
      .ref_type = ref_type,
    });

    // fixme:
    //   what about unions, built-in arrays? their dependency types' headers
    //   must be also included
    if (!type.isStructureOrClassType())
      continue;

    // refactorme:
    //   (see refactorme for `match::reflected_impl`). This duplication can and
    //   should be avoided
    const clang::CXXRecordDecl &rd = *type.getAsCXXRecordDecl();
    auto definition = util::ast::resolve_definition(rd, ast.getSourceManager());
    rd.isInStdNamespace()
      ? std_includes.emplace(std::move(definition.source_file))
      : includes.emplace(std::move(definition.source_file));
  }

  return result{
    .call_signature = std::move(call_signature),
    .std_includes = std::move(std_includes),
    .includes = std::move(includes),
  };
}

auto transforms::fold_resolved_types(tu_data accum,
  match_result_variant<match::reflected_type,
    match::reflected_impl,
    match::reflected_call> result) noexcept
  -> tl::expected<tu_data, std::string> {
  return std::visit(
    [&]<typename R>(R &&resolved) {
      return ::fold_resolved(std::move(accum), std::forward<R>(resolved));
    },
    std::move(result));
}

auto codegen::prepare_data(std::map<std::filesystem::path, transforms::tu_data>
    reflection_data_by_source) noexcept
  -> tl::expected<reflection_data, std::string> {
  // todo: implement
  return tl::unexpected("prepare_data not implemented");
}

} // namespace source_mode
} // namespace tool

// fixme: remove, implement
#ifdef _IMPLEMENTED

auto _debug_templ_spec_decl::resolve(const node_type &node,
  const clang::ASTUnit &ast,
  [[maybe_unused]] const std::unordered_set<std::string> &resolved_types,
  bool print_debug) -> tl::expected<result, std::string> {
  using namespace tool::refl;
  if (!print_debug)
    return {};

  const clang::ClassTemplateSpecializationDecl &template_decl = node;
  const std::string_view detail_struct_name = template_decl.getName();
  fmt::println("debug: ClassTemplateSpecializationDecl: {}",
    detail_struct_name);
  std::string buf;
  llvm::raw_string_ostream os{buf};
  clang::ASTDumper dmp{os, ast.getASTContext(), false};
  dmp.SetTraversalKind(clang::TK_AsIs);
  dmp.Visit(&template_decl);
  fmt::print("debug: ASTDump: {}\n", buf);
  return {};
}

} // namespace tool::refl::matches

// todo: remove
tl::expected<tool::refl::context, std::string> //
  tool::refl::update(tool::refl::context _current,
    tool::refl::context delta,
    bool a_print_debug) noexcept {
  tl::expected<tool::refl::context, std::string> r{std::move(_current)};
  // if (a_print_debug) {
  //   fmt::println("debug: current:");
  //   ::print_debug(*r);
  //   fmt::println("debug: delta:");
  //   ::print_debug(delta);
  // }

  if (tl::expected merged = util::merge_with_conflicts_check(
        std::move(r->definitions),
        std::move(delta.definitions),
        [](std::string_view qual_typename,
          const type_definition_data &lhs,
          const type_definition_data &rhs) -> tl::expected<void, std::string> {
          if (lhs.source_file != rhs.source_file) {
            return tl::unexpected(fmt::format(
              "found different locations for type {} definition: {}, {}",
              qual_typename,
              lhs.source_file.string(),
              rhs.source_file.string()));
          }
          if (lhs.definition_flags != rhs.definition_flags) {
            return tl::unexpected(fmt::format(
              "found different definition flags for type {} definition: {}, {}",
              qual_typename,
              // todo: stringify
              int(lhs.definition_flags),
              int(rhs.definition_flags)));
          }
          return {};
        })) {
    r->definitions = std::move(merged).value();
  } else
    return tl::unexpected(std::move(merged).error());

  // fixme:
  // if (tl::expected merged =
  //       merge_with_conflicts_check(std::move(r->reflected_types),
  //         std::move(delta.reflected_types),
  //         [](std::string_view qual_typename,
  //           const std::vector<struct_field_data> &lhs,
  //           const std::vector<struct_field_data> &rhs)
  //           -> tl::expected<void, std::string> {
  //           // fixme: wtf is happening here? how is it possible? what was I
  //           // thinking?
  //           if (!std::equal(lhs.cbegin(),
  //                 lhs.cend(),
  //                 rhs.cbegin(),
  //                 [](const struct_field_data &lhs, const struct_field_data
  //                 &rhs)
  //                   -> bool { return lhs.nm_qual_type == rhs.nm_qual_type;
  //                   })) {
  //             // todo: print detailed info on fields that differ
  //             return tl::unexpected(
  //               fmt::format("found different data member types for type {}",
  //                 qual_typename));
  //           }
  //           return {};
  //         })) {
  //   r->reflected_types = std::move(merged).value();
  // } else
  //   return tl::unexpected(std::move(merged).error());

  r->reflected_implementations.merge(
    std::move(delta.reflected_implementations));

  r->reflected_calls.reserve(
    r->reflected_calls.size() + delta.reflected_calls.size());
  r->reflected_calls.insert(r->reflected_calls.end(),
    std::make_move_iterator(delta.reflected_calls.begin()),
    std::make_move_iterator(delta.reflected_calls.end()));

  r->std_includes.merge(std::move(delta.std_includes));
  if (tl::expected merged =
        util::merge_with_conflicts_check(std::move(r->detected_indexed_types),
          std::move(delta.detected_indexed_types),
          [](const size_t type_index,
            std::string_view lhs,
            std::string_view rhs) -> tl::expected<void, std::string> {
            return tl::unexpected(
              fmt::format("confict for detected type index {}, types: {}, {}",
                type_index,
                lhs,
                rhs));
          })) {
    r->detected_indexed_types = std::move(merged).value();
  } else {
    return tl::unexpected(std::move(merged).error());
  }

  if (tl::expected merged =
        util::merge_with_conflicts_check(std::move(r->reflected_indexed_types),
          std::move(delta.reflected_indexed_types),
          [](const int type_index, const auto &, const auto &)
            -> tl::expected<void, std::string> {
            // todo:
            //   is it possible to identify those? `detected_indexed_types` will
            //   show the info. However, would be more useful to print detailed
            //   standard diagnostics
            return tl::unexpected(
              fmt::format("confict for type index {}", type_index));
          })) {
    r->reflected_indexed_types = std::move(merged).value();
  } else {
    return tl::unexpected(std::move(merged).error());
  }

  // if (a_print_debug) {
  //   fmt::println("debug: current updated:");
  //   ::print_debug(*r);
  // }
  return r;
};
#endif //
