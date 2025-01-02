#include "tool/reflection.hpp"

#include "tool/util.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <clang/AST/ASTImporter.h>
#include <clang/AST/DeclBase.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/PrecompiledPreamble.h>
#include <clang/Serialization/PCHContainerOperations.h>
#pragma GCC diagnostic pop

#include <fmt/base.h>

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <optional>
#include <stack>
#include <string_view>
#include <variant>

namespace {
std::string get_declaration_source_file(const clang::Decl &d, const clang::SourceManager &sm) {
  const auto loc = d.getLocation();
  // todo: use expected?
  if (!loc.isValid())
    return "";

  const auto pm = sm.getPresumedLoc(loc);
  if (!pm.isValid())
    return "";

  std::string filename = pm.getFilename();
  return filename;
}

bool is_header_file(std::string_view filename) {
  llvm::StringRef ext = llvm::sys::path::extension(filename);
  return ext.equals_insensitive(".h") || ext.equals_insensitive(".hpp")
    || ext.equals_insensitive(".hxx");
}

// because stupid dap-ui doesn't show unordered_maps
void print_debug(const tool::refl::context &ctx) {
  fmt::println("debug: reflected_implementations: {}", ctx.reflected_implementations.size());
  for (const auto &i : ctx.reflected_implementations)
    fmt::println("  {}", i);
  fmt::println("debug: reflected_types: {}", ctx.reflected_types.size());
  for (const auto &i : ctx.reflected_types)
    fmt::println("  {}", i.first);
}

tool::refl::type_definition_data resolve_definition(
  // namespace-qualified typename is needed before the call to this function to check whether
  // the definition has been already resolved
  std::string nm_qual_type,
  const clang::RecordDecl &rd,
  const clang::SourceManager &sm) noexcept {
  using td_flags = tool::refl::type_definition_flags;
  std::string source_file = get_declaration_source_file(rd, sm);
  const td_flags td_unnamed = td_flags::none; // todo:
  const td_flags td_local = td_flags::none; // todo:
  const td_flags td_in_cpp = is_header_file(source_file) ? td_flags::none : td_flags::in_cpp;

  return {
    .name = std::move(nm_qual_type),
    .source_file = std::move(source_file),
    .definition_flags = td_flags(td_unnamed | td_local | td_in_cpp),
  };
}

// types of interest for recursive reflection:
//   - value_type: std containers, std wrappers, std::optional
//   - key_type: std containers
//   - type: std::reference_wrapper
struct member_typedef_decl {
  std::string_view name;
  clang::QualType qual_type;
};

std::vector<member_typedef_decl> member_typedefs(const clang::RecordDecl &rd) noexcept {
  // todo: use `filter`
  std::vector<member_typedef_decl> r;
  r.reserve(std::accumulate(rd.decls_begin(),
    rd.decls_end(),
    size_t(0),
    [](size_t s, const clang::Decl *d) -> size_t {
      const clang::Decl::Kind k = d->getKind();
      return s + (clang::Decl::TypeAlias == k || clang::Decl::Typedef == k);
    }));

  for (const clang::Decl *_d : rd.decls()) {
    const clang::Decl::Kind k = _d->getKind();
    if (clang::Decl::TypeAlias != k && clang::Decl::Typedef != k)
      continue;

    const auto &d = *llvm::cast<clang::TypedefNameDecl>(_d);
    r.push_back({
      .name = d.getName(),
      .qual_type = d.getUnderlyingType(),
    });
  }
  return r;
};

/// fold AST from root to a vector of unique declarations that are not already present in
/// `reflected_types_map`
std::vector<const clang::RecordDecl *> dfs_fold_reflected_types(const clang::CXXRecordDecl &root,
  const auto &reflected_types_map) {
  std::set<const clang::RecordDecl *> visited;
  std::stack<const clang::RecordDecl *> stack;
  stack.push(&root);

  while (!stack.empty()) {
    const clang::RecordDecl *cur = stack.top();
    stack.pop();
    const std::string _debug_nm_qual_type = cur->getQualifiedNameAsString();
    if (visited.contains(cur) || reflected_types_map.contains(cur->getQualifiedNameAsString()))
      continue;
    // not generating reflection info for std types, at least for now, at least here...
    if (!cur->isInStdNamespace())
      visited.emplace(cur);

    // we allow recursive reflection via certain member typedefs
    for (const member_typedef_decl &md : util::filtered(
           [](const member_typedef_decl &m) -> bool {
             const static std::set<std::string_view> aliases{{
               "key_type",
               "value_type",
               "value",
             }};
             return !aliases.contains(m.name);
           },
           member_typedefs(*cur))) {
      if (md.qual_type->isStructureOrClassType())
        stack.push(md.qual_type->getAsRecordDecl());
    }
    // fixme: what about unions, built-in arrays?

    // ad hoc solution. in c++ code template trait types can be used, but I don't know if
    // it is possible to get from parsed ast
    // I could, however, capture such types in `omni::detail`...
    if (clang::Decl::ClassTemplateSpecialization == cur->getKind() &&
        [](std::string_view name) -> bool { //
          return "tuple" == name || "variant" == name;
        }(cur->getName())) {
      const auto arg_list = clang::cast<clang::ClassTemplateSpecializationDecl>(cur)
                              ->getTemplateInstantiationArgs()
                              .asArray();
      if (1 != arg_list.size() || clang::TemplateArgument::Pack != arg_list.front().getKind()) {
        // todo: log? this should not happen
      } else {
        for (const clang::TemplateArgument &t_arg : arg_list.front().getPackAsArray()) {
          // not a type pack
          if (clang::TemplateArgument::Type != t_arg.getKind())
            break;

          const clang::QualType qt = t_arg.getAsType();
          if (qt->isRecordType())
            stack.push(qt->getAsRecordDecl());
        }
      }
    }

    for (const clang::FieldDecl *fd : cur->fields()) {
      if (clang::AccessSpecifier::AS_public != fd->getAccess()
        // todo: other checks that would prevent the field from being
        // reflected (uniouns, bitfields, what else?)
        || fd->isUnnamedBitField())
        continue;

      const clang::QualType qt = fd->getType();
      // fixme: what about unions, built-in arrays?
      if (!qt->isStructureOrClassType())
        continue;

      // todo: support only non-static fields
      stack.push(clang::cast<clang::RecordDecl>(qt->getAsRecordDecl()));
    }
  }

  return {visited.cbegin(), visited.cend()};
}

struct resolve_t {
  tl::expected<tool::refl::context, std::string> operator()(tool::refl::context ctx,
    const clang::ASTUnit &ast,
    const tool::matched_node<tool::refl::matches::reflected_type> &m) const noexcept {
    using namespace tool::refl;

    const clang::ClassTemplateSpecializationDecl &template_decl = *m.node;
    const std::string_view detail_struct_name = template_decl.getName();
    const auto &template_args_list = template_decl.getTemplateArgs();

    // todo: error if template_args_list is empty
    const clang::TemplateArgument &first_arg = template_args_list.get(0);
    if (clang::TemplateArgument::ArgKind::Type != first_arg.getKind()) {
      return tl::unexpected(
        fmt::format("non-type template argument `0` of {}", detail_struct_name));
    }
    const clang::Type &first_arg_type = *first_arg.getAsType().getTypePtr();
    // todo: handle checks for `clang::Type::is...`
    // todo: `hasDefinition` - at this point (as of this writing) forward declarations are not
    //       allowed
    if (!first_arg_type.isStructureOrClassType()) {
      return tl::unexpected(fmt::format("unsupported type {} in {}",
        // fixme: `getTypeClassName` doesn't do what I thought it does
        first_arg_type.getTypeClassName(),
        detail_struct_name));
    }

    // omni::detail::_reflected_impl<T>
    if ("_reflected_impl" == detail_struct_name) {
      // type declaration of <T>
      const clang::CXXRecordDecl &rdecl = *first_arg_type.getAsCXXRecordDecl();
      // as of this writing there's a "fixme" to remove `std::string`, but it is not clear
      // whether a new type will be used or another function should be called
      const std::string nm_qual_type = rdecl.getQualifiedNameAsString();
      // fixme: do not use definitions, because reflected types also use them, but field info
      // should be collected only for reflected types
      if (ctx.definitions.contains(nm_qual_type))
        return {std::move(ctx)};

      ctx.definitions[nm_qual_type] =
        resolve_definition(nm_qual_type, rdecl, ast.getSourceManager());
      ctx.reflected_implementations.emplace(nm_qual_type);
      // todo: should we allow wrapped types (using value_type trait)?
      return {std::move(ctx)};
    }

    // omni::detail::_reflected_type<T>
    if ("_reflected_type" == detail_struct_name) {
      // type declaration of <T>
      const clang::CXXRecordDecl &first_arg_decl = *first_arg_type.getAsCXXRecordDecl();
      const std::string first_arg_nm_qual_type = first_arg_decl.getQualifiedNameAsString();
      if (ctx.reflected_types.contains(first_arg_nm_qual_type))
        return {std::move(ctx)};

      // recursivelly collected types (including self)
      const std::vector<const clang::RecordDecl *> types =
        dfs_fold_reflected_types(first_arg_decl, ctx.reflected_types);

      for (const clang::RecordDecl *_rdecl : types) {
        const auto &rdecl = *_rdecl;
        const std::string nm_qual_type = rdecl.getQualifiedNameAsString();

        if (rdecl.isInStdNamespace()) {
          // todo: std include path should not be absolute
          ctx.std_includes.emplace(get_declaration_source_file(rdecl, ast.getSourceManager()));
        } else {
          assert(!ctx.definitions.contains(nm_qual_type));
          ctx.definitions[nm_qual_type] =
            resolve_definition(nm_qual_type, rdecl, ast.getSourceManager());
        }

        // refactorme: this code is duplicated when collecting types, but whatever
        ctx.reflected_types.emplace(nm_qual_type,
          [](const auto &_fields) -> std::vector<struct_field_data> {
            std::vector<struct_field_data> r;
            for (const clang::FieldDecl *fd : _fields) {
              // todo: logging for skipped fields, since we are not reporting them as errors
              // todo: checks that would prevent the field from being reflected (uniouns,
              // bitfields, what else?)
              if (clang::AccessSpecifier::AS_public != fd->getAccess())
                continue;
              r.push_back({
                .name = fd->getNameAsString(),
                .nm_qual_type = fd->getQualifiedNameAsString(),
              });
            }
            return r;
          }(rdecl.fields()));
      }

      return {std::move(ctx)};
    }

    return tl::unexpected("unexpected reflection tag");
  }

  tl::expected<tool::refl::context, std::string> operator()(tool::refl::context ctx,
    const clang::ASTUnit &ast,
    const tool::matched_node<tool::refl::matches::reflected_call> &m) const noexcept {
    using namespace tool::refl;

    const clang::CXXMethodDecl &refl_call_decl = *m.node;
    func_signature &refl_call = ctx.reflected_calls.emplace_back();
    refl_call.args.reserve(refl_call_decl.parameters().size());

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
      // since they are not supported at this point

      const static auto printing_policy = [] {
        clang::PrintingPolicy p{{}};
        // todo: do I need to supress the tag here?
        // It is actually needed in template specialization
        p.SuppressTagKeyword = true;
        p.SuppressScope = false;
        p.PrintCanonicalTypes = true;

        return p;
      }();

      refl_call.args.push_back({
        // todo: remove
        .cvr_qualified_type = parm_decl->getType().getAsString(),
        // fixme: it still prints `struct`
        // refactorme: use `type`
        .nm_qual_type = clang::QualType::getAsString(
          [](clang::SplitQualType q) {
            q.Quals.removeCVRQualifiers();
            return q;
          }(parm_decl->getType().getNonReferenceType().split()),
          printing_policy),
        .is_const = is_const,
        .ref_type = ref_type,
      });
      const auto &nm_qual_type = refl_call.args.back().nm_qual_type;

      // fixme: what about unions, built-in arrays?
      if (!type.isStructureOrClassType())
        continue;
      const clang::RecordDecl &rd = *type.getAsRecordDecl();

      if (rd.isInStdNamespace()) {
        // todo: add std include
      }
      if (!ctx.definitions.contains(nm_qual_type))
        ctx.definitions[nm_qual_type] =
          resolve_definition(nm_qual_type, rd, ast.getSourceManager());
    }

    return {std::move(ctx)};
  }
} const inline resolve{};
} // namespace

tl::expected<tool::refl::context, std::string> //
  tool::refl::resolve_matched_node(context ctx,
    const clang::ASTUnit &ast,
    tool::match_variant<matches::reflected_type, matches::reflected_call> match) noexcept {
  return std::visit( //
    [&](const auto &m) noexcept { return resolve(std::move(ctx), ast, m); }, //
    match);
}

tl::expected<tool::refl::context, std::string> tool::refl::update(tool::refl::context _current,
  tool::refl::context delta) noexcept {
  tl::expected<tool::refl::context, std::string> r{std::move(_current)};
  fmt::println("current:");
  print_debug(*r);
  fmt::println("delta:");
  print_debug(delta);

  // in-place utility
  const auto merge_with_conflicts_check =
    []<typename K, typename T>(std::unordered_map<K, T> first,
      std::unordered_map<K, T> second,
      // (const K&, const T&, const T&) -> std::optional<std::string> error
      auto cmp) -> tl::expected<std::unordered_map<K, T>, std::string> {
    for (auto node = second.begin(); node != second.end();) {
      auto &&[k, v] = *node;
      const auto found = first.find(k);
      if (found == first.cend()) {
        first.insert(second.extract(node++));
        continue;
      }
      if (auto err = cmp(k, v, found->second))
        return tl::unexpected(std::move(err).value());
      ++node;
    }
    return {std::move(first)};
  };

  if (auto merged = merge_with_conflicts_check(std::move(r->definitions),
        std::move(delta.definitions),
        [](std::string_view qual_typename,
          const type_definition_data &lhs,
          const type_definition_data &rhs) -> std::optional<std::string> {
          if (lhs.source_file != rhs.source_file) {
            return fmt::format("found different locations for type {} definition: {}, {}",
              qual_typename,
              lhs.source_file.string(),
              rhs.source_file.string());
          }
          if (lhs.definition_flags != rhs.definition_flags) {
            return fmt::format("found different definition flags for type {} definition: {}, {}",
              qual_typename,
              // todo: stringify
              int(lhs.definition_flags),
              int(rhs.definition_flags));
          }
          return std::nullopt;
        })) {
    r->definitions = std::move(merged).value();
  } else
    return tl::unexpected(std::move(merged).error());

  if (auto merged = merge_with_conflicts_check(std::move(r->reflected_types),
        std::move(delta.reflected_types),
        [](std::string_view qual_typename,
          const std::vector<struct_field_data> &lhs,
          const std::vector<struct_field_data> &rhs) -> std::optional<std::string> {
          if (!std::equal(lhs.cbegin(),
                lhs.cend(),
                rhs.cbegin(),
                [](const struct_field_data &lhs, const struct_field_data &rhs) -> bool {
                  return lhs.nm_qual_type == rhs.nm_qual_type;
                })) {
            // todo: print detailed info on fields that differ
            return fmt::format("found different data member types for type {}", qual_typename);
          }
          return std::nullopt;
        })) {
    r->reflected_types = std::move(merged).value();
  } else
    return tl::unexpected(std::move(merged).error());

  r->reflected_implementations.merge(std::move(delta.reflected_implementations));

  r->reflected_calls.reserve(r->reflected_calls.size() + delta.reflected_calls.size());
  r->reflected_calls.insert(r->reflected_calls.end(),
    std::make_move_iterator(delta.reflected_calls.begin()),
    std::make_move_iterator(delta.reflected_calls.end()));

  r->std_includes.merge(std::move(delta.std_includes));

  fmt::println("current updated:");
  print_debug(*r);
  return r;
};
