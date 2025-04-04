#include "tool/source_mode/transforms.h"

// fixme: remove, implement
#ifdef _IMPLEMENTED

#  include "tool/reflection.hpp"

#  include "tool/ast/util.h"
#  include "tool/util.hpp"

#  include <fmt/core.h>
#  include <tl/expected.hpp>

#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-variable"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wdeprecated"
#  include <clang/AST/ASTDumper.h>
#  include <clang/AST/ASTImporter.h>
#  include <clang/AST/DeclBase.h>
#  include <clang/Basic/Diagnostic.h>
#  include <clang/Basic/FileManager.h>
#  include <clang/Basic/LangOptions.h>
#  include <clang/Frontend/ASTUnit.h>
#  include <clang/Frontend/CompilerInstance.h>
#  include <clang/Frontend/PrecompiledPreamble.h>
#  include <clang/Serialization/PCHContainerOperations.h>
#  pragma GCC diagnostic pop

#  include <algorithm>
#  include <cassert>
#  include <cstddef>
#  include <optional>
#  include <string>
#  include <string_view>
#  include <utility>
#  include <vector>

namespace tool::refl::matches {

auto reflected_type::resolve(const node_type &node,
  const clang::ASTUnit &ast,
  const std::unordered_set<std::string> &resolved_types,
  bool print_debug) -> tl::expected<result, std::string> {
  // Reflected type is detected from `omni::detail::_reflected_type<T>`
  // specialization.
  const clang::ClassTemplateSpecializationDecl &template_decl = node;
  tl::expected _template_arg_0_type =
    util::ast::get_template_type_arg(template_decl, 0);
  if (!_template_arg_0_type)
    return tl::unexpected(std::move(_template_arg_0_type).error());
  const clang::Type &template_arg_0_type = **_template_arg_0_type;
  // fixme: what about fundamental types?
  //  Fundamental types can be caught by the 'front end' (by predefined
  //  specializations)
  //  I think I just need to skip them. Also need to write a test that involves
  //  fundamental or non-user defined types. However, I think it is a rather
  //  complex question whether I should allow it or not.
  if (!template_arg_0_type.isStructureOrClassType()) {
    return tl::unexpected(fmt::format("unsupported type {} in {}",
      // fixme: `getTypeClassName` doesn't do what I thought it does
      template_arg_0_type.getTypeClassName(),
      template_decl.getName()));
  }

  const clang::CXXRecordDecl &reflected_type_decl =
    *template_arg_0_type.getAsCXXRecordDecl();
  // todo: `hasDefinition`
  //   at this point (as of this writing) forward declarations are not allowed.
  //   however, with inplace mode I can check at the end of TU

  const std::string reflected_type_nm_qual_typename =
    reflected_type_decl.getQualifiedNameAsString();
  if (resolved_types.contains(reflected_type_nm_qual_typename))
    return {};

  // refactorme: this variable is mutable in the middle of the scope
  auto reflectable_data =
    util::ast::recursively_collect_dependency_types(reflected_type_decl,
      resolved_types);

  std::unordered_map<std::string, reflected_type_data> reflected_types;

  for (const clang::CXXRecordDecl *_rdecl : reflectable_data) {
    const clang::CXXRecordDecl &rdecl = *_rdecl;
    const std::string nm_qual_type = rdecl.getQualifiedNameAsString();

    assert(!rdecl.isInStdNamespace()
      && "`recursively_collect_reflectable_types` is "
           "not supposed to return std type");
    assert(!resolved_types.contains(nm_qual_type)
      && "`recursively_collect_reflectable_types` is "
           "not supposed to return resolved types");

    if (print_debug && nm_qual_type.empty())
      fmt::println("DEBUG: empty namespace-qualified type resolved.");

    // fixme: what about unnamed/local types?
    reflected_types[nm_qual_type] = {
      .definition_data =
        util::ast::resolve_definition(rdecl, ast.getSourceManager()),
      .fields = util::ast::resolve_struct_fields(rdecl.fields()),
    };
  }

  // refactorme: UGLEEE BEATCH
  auto matched_type = reflected_types.find(reflected_type_nm_qual_typename);
  return result{
    .matched_type = reflected_types.cend() != matched_type
    ? [](auto extracted) -> decltype(result::matched_type) {
      return std::pair{std::move(extracted).key(),
        std::move(extracted).mapped()};
    }(reflected_types.extract(matched_type))
    : std::nullopt,
    .matched_type_dependencies = std::move(reflected_types),
  };
}

auto reflected_impl::resolve(const node_type &node,
  const clang::ASTUnit &ast,
  const std::unordered_set<std::string> &resolved_types,
  [[maybe_unused]] bool print_debug) -> tl::expected<result, std::string> {
  using namespace tool::refl;

  // todo: comment about template signature
  const clang::ClassTemplateSpecializationDecl &template_decl = node;
  tl::expected _template_arg_0 =
    util::ast::get_template_type_arg(template_decl, 0);
  if (!_template_arg_0)
    return tl::unexpected(std::move(_template_arg_0).error());
  const clang::Type &template_arg_0 = **_template_arg_0;

  // todo:
  //   `hasDefinition` - at this point (as of this writing) forward declarations
  //   are not allowed.
  if (!template_arg_0.isStructureOrClassType()) {
    const std::string_view detail_struct_name = template_decl.getName();
    return tl::unexpected(fmt::format("unsupported type {} in {}",
      // fixme: `getTypeClassName` doesn't do what I thought it does
      template_arg_0.getTypeClassName(),
      detail_struct_name));
  }

  // type declaration of <T>
  const clang::CXXRecordDecl &reflected_type_decl =
    *template_arg_0.getAsCXXRecordDecl();
  // as of this writing there's a "fixme" to remove `std::string`, but it is not
  // clear whether a new type will be used or another function should be called
  std::string nm_qual_type = reflected_type_decl.getQualifiedNameAsString();
  if (resolved_types.contains(nm_qual_type))
    return {};

  return result{
    .callable_nm_qual_type = std::move(nm_qual_type),
    .callable_definition_data =
      util::ast::resolve_definition(reflected_type_decl,
        ast.getSourceManager()),
  };
}

auto reflected_call::resolve(const node_type &node,
  const clang::ASTUnit &ast,
  [[maybe_unused]] const std::unordered_set<std::string> &resolved_types,
  [[maybe_unused]] bool print_debug) -> tl::expected<result, std::string> {
  using namespace tool::refl;

  const static auto printing_policy = [] {
    clang::PrintingPolicy p{{}};
    // todo: do I need to supress the tag here?
    // It is actually needed in template specialization
    p.SuppressTagKeyword = true;
    p.SuppressScope = false;
    p.PrintCanonicalTypes = true;

    return p;
  }();

  const clang::CXXMethodDecl &refl_call_decl = node;

  func_signature call_signature;
  call_signature.args.reserve(refl_call_decl.parameters().size());
  std::set<std::filesystem::path> std_includes, includes;

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
    // todo: validate that the type is not a forward declaration,
    //   since they are not supported at this point

    call_signature.args.push_back({
      // refactorme: use `type` (why? I forgot...)
      .nm_qual_type = clang::QualType::getAsString(
        [](clang::SplitQualType q) {
          q.Quals.removeCVRQualifiers();
          return q;
        }(parm_decl->getType().getNonReferenceType().split()),
        printing_policy),
      .is_const = is_const,
      .ref_type = ref_type,
    });

    // fixme: what about unions, built-in arrays? their dependency types'
    // headers must be also included
    if (!type.isStructureOrClassType())
      continue;

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
