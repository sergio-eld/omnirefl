
#include "tool/ast/util.h"

#include "tool/reflection.hpp"
#include "tool/util.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated"
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/Basic/SourceManager.h>
#pragma GCC diagnostic pop

#include <expected>
#include <numeric>
#include <set>
#include <stack>

namespace {

// refactorme: reflect in code instead of writing the comment below.
// types of interest for recursive reflection:
//   - value_type: std containers, std wrappers, std::optional
//   - key_type: std containers
//   - type: std::reference_wrapper
struct member_typedef_decl {
  std::string_view name;
  clang::QualType qual_type;
};

std::vector<member_typedef_decl> member_typedefs(
  const clang::RecordDecl &rd) noexcept {
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
} // namespace

// todo: use expected
std::string util::ast::get_declaration_source_file(const clang::Decl &d,
  const clang::SourceManager &sm) noexcept {
  const auto loc = d.getLocation();
  if (!loc.isValid())
    return "";

  const auto pm = sm.getPresumedLoc(loc);
  if (!pm.isValid())
    return "";

  std::string filename = pm.getFilename();
  return filename;
}

tool::refl::type_definition_data util::ast::resolve_definition(
  const clang::CXXRecordDecl &rd,
  const clang::SourceManager &sm) noexcept {
  using td_flags = tool::refl::type_definition_flags;
  std::string source_file = util::ast::get_declaration_source_file(rd, sm);
  const td_flags td_local =
    rd.isLocalClass() ? td_flags::local : td_flags::none;
  const td_flags td_non_public = td_flags::none; //< todo:

  return {
    .source_file = std::move(source_file),
    .nm_qual_type = rd.getQualifiedNameAsString(),
    // refactorme: enable bitwise operations
    .definition_flags = td_flags(td_local | td_non_public),
    .is_class = rd.isClass(),
  };
}

std::vector<tool::refl::struct_field_data> util::ast::resolve_struct_fields(
  clang::RecordDecl::field_range fields_range) noexcept {
  std::vector<tool::refl::struct_field_data> r;
  for (const clang::FieldDecl *fd : fields_range) {
    // todo:
    //   logging for skipped fields, since we are not reporting them as errors
    //
    // todo:
    //   checks that would prevent the field from being reflected (uniouns,
    //   bitfields, what else?)

    // todo:
    //   consider supporting non-public types with selecting based on a
    //   typename, like `fields(specific_typename)`
    if (clang::AccessSpecifier::AS_public != fd->getAccess())
      continue;
    r.push_back({
      .name = fd->getNameAsString(),
    });
  }
  return r;
}

// testme: struct with a tuple type field
std::vector<const clang::CXXRecordDecl *>
  util::ast::recursively_collect_dependency_types(
    const clang::CXXRecordDecl &root,
    const std::set<tool::refl::type_id> &resolved_types) noexcept {
  std::set<const clang::CXXRecordDecl *> visited;
  std::stack<const clang::CXXRecordDecl *> to_visit;
  to_visit.push(&root);

  while (!to_visit.empty()) {
    const clang::CXXRecordDecl *cur = to_visit.top();
    to_visit.pop();

    // todo: remove
    const std::string _debug_nm_qual_type = cur->getQualifiedNameAsString();
    if (visited.contains(cur)
      // todo: continious benchmark before optimization (lookup by string)
      || resolved_types.contains(cur->getQualifiedNameAsString())) {
      continue;
    }

    // not generating reflection info for std types, at least for now, at least
    // here...
    //
    // refactorme: ambiguous control flow/logic
    if (!cur->isInStdNamespace())
      visited.emplace(cur);

    // allow recursive reflection via certain member typedefs
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
        to_visit.push(md.qual_type->getAsCXXRecordDecl());
    }
    // fixme: what about unions, built-in arrays?

    // ad hoc solution:
    //   in c++ code template trait types can be used, but I don't know if it is
    //   possible to get from parsed ast.
    //   I could, however, capture such types in `omni::detail`...
    if (clang::Decl::ClassTemplateSpecialization == cur->getKind() &&
        [](std::string_view name) -> bool { //
          return "tuple" == name || "variant" == name;
        }(cur->getName())) {
      const auto arg_list =
        clang::cast<clang::ClassTemplateSpecializationDecl>(cur)
          ->getTemplateInstantiationArgs()
          .asArray();
      if (1 != arg_list.size()
        || clang::TemplateArgument::Pack != arg_list.front().getKind()) {
        // todo: log? this should not happen
      } else {
        for (const clang::TemplateArgument &t_arg :
          arg_list.front().getPackAsArray()) {
          // not a type pack
          // todo: is this possible though? should log?
          if (clang::TemplateArgument::Type != t_arg.getKind())
            break;

          const clang::QualType qt = t_arg.getAsType();
          if (qt->isStructureOrClassType())
            to_visit.push(qt->getAsCXXRecordDecl());
        }
      }
    }

    // fixme: just collect the types: determine reflectable later?
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
      to_visit.push(qt->getAsCXXRecordDecl());
    }
  }

  visited.erase(&root);
  return {visited.begin(), visited.end()};
}

std::expected<clang::Type const *, std::string> util::ast::get_template_type_arg(
  const clang::ClassTemplateSpecializationDecl &template_decl,
  size_t n) noexcept {
  const auto &template_args_list = template_decl.getTemplateArgs();

  if (template_args_list.size() < n) {
    const std::string_view detail_struct_name = template_decl.getName();
    return std::unexpected(
      fmt::format("invalid template signature for internal omnirefl type {}",
        detail_struct_name));
  }

  const clang::TemplateArgument &arg = template_args_list.get(n);
  if (clang::TemplateArgument::ArgKind::Type != arg.getKind()) {
    const std::string_view detail_struct_name = template_decl.getName();
    return std::unexpected(fmt::format("non-type template argument `{}` of {}",
      n,
      detail_struct_name));
  }

  return arg.getAsType().getTypePtr();
}

std::expected<int, std::string> util::ast::get_template_value_arg(
  const clang::ClassTemplateSpecializationDecl &template_decl,
  size_t n) noexcept {
  const auto &template_args_list = template_decl.getTemplateArgs();

  if (template_args_list.size() < n) {
    const std::string_view detail_struct_name = template_decl.getName();
    return std::unexpected(
      fmt::format("invalid template signature for internal omnirefl type {}",
        detail_struct_name));
  }

  const clang::TemplateArgument &arg = template_args_list.get(n);
  if (clang::TemplateArgument::ArgKind::Integral != arg.getKind()) {
    const std::string_view detail_struct_name = template_decl.getName();
    return std::unexpected(
      fmt::format("non-integral template argument `{}` of {}",
        n,
        detail_struct_name));
  }

  return arg.getAsIntegral().getExtValue();
}
