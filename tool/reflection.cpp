#include "tool/reflection.hpp"

#include "tool/util.hpp"

#include <fmt/core.h>
#include <tl/expected.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated"
#include <clang/AST/ASTDumper.h>
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

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <stack>
#include <string>
#include <string_view>

namespace {
// todo: use expected
std::string get_declaration_source_file(const clang::Decl &d, const clang::SourceManager &sm) {
  const auto loc = d.getLocation();
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

  fmt::println("debug: indexed reflected types: {}", ctx.reflected_types_indexes.size());
  for (const auto &[index, nm_qual_typename] : ctx.reflected_types_indexes)
    fmt::println("  {}: {}", index, nm_qual_typename);
}

tool::refl::type_definition_data resolve_definition(const clang::CXXRecordDecl &rd,
  const clang::SourceManager &sm) noexcept {
  using td_flags = tool::refl::type_definition_flags;
  std::string source_file = get_declaration_source_file(rd, sm);
  const td_flags td_unnamed = rd.getIdentifier() ? td_flags::none : td_flags::unnamed;
  const td_flags td_local = rd.isLocalClass() ? td_flags::local : td_flags::none;
  const td_flags td_in_cpp = is_header_file(source_file) ? td_flags::none : td_flags::in_cpp;

  return {
    .source_file = std::move(source_file),
    .definition_flags = td_flags(td_unnamed | td_local | td_in_cpp),
    .is_class = rd.isClass(),
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
// todo: documentation. This function might be a candidate for unit testing
// Collect and resolve types:
// - from T::key_type, T::value_type, T::value
// - T... from variant<T...> and tuple<T...>
// - F from T::F field
std::vector<const clang::CXXRecordDecl *>
  resolve_types_depends_upon(const clang::CXXRecordDecl &root, const auto &reflected_types_map) {
  std::set<const clang::CXXRecordDecl *> visited;
  std::stack<const clang::CXXRecordDecl *> stack;
  stack.push(&root);

  while (!stack.empty()) {
    const clang::CXXRecordDecl *cur = stack.top();
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
        stack.push(md.qual_type->getAsCXXRecordDecl());
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
          if (qt->isStructureOrClassType())
            stack.push(qt->getAsCXXRecordDecl());
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
      stack.push(qt->getAsCXXRecordDecl());
    }
  }

  return {visited.cbegin(), visited.cend()};
}

tl::expected<clang::Type const *, std::string>
  get_template_arg_type(const clang::ClassTemplateSpecializationDecl &template_decl, size_t n) {
  const auto &template_args_list = template_decl.getTemplateArgs();

  if (template_args_list.size() < n) {
    const std::string_view detail_struct_name = template_decl.getName();
    return tl::unexpected(
      fmt::format("invalid template signature for internal omnirefl type {}", detail_struct_name));
  }

  const clang::TemplateArgument &arg = template_args_list.get(n);
  if (clang::TemplateArgument::ArgKind::Type != arg.getKind()) {
    const std::string_view detail_struct_name = template_decl.getName();
    return tl::unexpected(
      fmt::format("non-type template argument `{}` of {}", n, detail_struct_name));
  }

  return arg.getAsType().getTypePtr();
}

tl::expected<int, std::string>
  get_template_arg_value(const clang::ClassTemplateSpecializationDecl &template_decl, size_t n) {
  const auto &template_args_list = template_decl.getTemplateArgs();

  if (template_args_list.size() < n) {
    const std::string_view detail_struct_name = template_decl.getName();
    return tl::unexpected(
      fmt::format("invalid template signature for internal omnirefl type {}", detail_struct_name));
  }

  const clang::TemplateArgument &arg = template_args_list.get(n);
  if (clang::TemplateArgument::ArgKind::Integral != arg.getKind()) {
    const std::string_view detail_struct_name = template_decl.getName();
    return tl::unexpected(
      fmt::format("non-integral template argument `{}` of {}", n, detail_struct_name));
  }

  return arg.getAsIntegral().getExtValue();
}
} // namespace

namespace tool::refl::matches {

tl::expected<context, std::string> reflected_type_index::resolve(const node_type &node,
  [[maybe_unused]] const clang::ASTUnit &ast,
  [[maybe_unused]] const context &ctx,
  [[maybe_unused]] bool print_debug) {
  using namespace tool::refl;

  // omni::detail::_type_index<typename reflected_type, int reflected_type_index>
  const clang::ClassTemplateSpecializationDecl &template_decl = node;
  tl::expected _reflected_type = get_template_arg_type(template_decl, 0);
  if (!_reflected_type)
    return tl::unexpected(std::move(_reflected_type).error());
  const clang::Type &reflected_type = **_reflected_type;

  // todo: `hasDefinition` - at this point (as of this writing) forward declarations are not
  // allowed
  // fixme: what about fundamental types??? I think I just need to skip them. Also need to write a
  // test that involves fundamental or non-user defined types. However, I think it is a rather
  // complex question whether I should allow it or not.
  if (!reflected_type.isStructureOrClassType()) {
    return tl::unexpected(fmt::format("unsupported type {} in {}",
      // fixme: `getTypeClassName` doesn't do what I thought it does
      reflected_type.getTypeClassName(),
      template_decl.getName()));
  }

  tl::expected _reflected_type_index = get_template_arg_value(template_decl, 1);
  if (!_reflected_type_index)
    return tl::unexpected(std::move(_reflected_type_index).error());
  const int reflected_type_index = *_reflected_type_index;
  const clang::CXXRecordDecl &reflected_type_decl = *reflected_type.getAsCXXRecordDecl();
  std::string nm_qual_type = reflected_type_decl.getQualifiedNameAsString();

  tl::expected<context, std::string> ctx_delta{tl::in_place};
  ctx_delta->reflected_types_indexes[reflected_type_index] = std::move(nm_qual_type);

  // todo: should I check for conflicting indexes here or on update? let's do on update for now...
  return ctx_delta;
}

tl::expected<context, std::string> reflected_type::resolve(const node_type &node,
  const clang::ASTUnit &ast,
  const context &ctx,
  [[maybe_unused]] bool print_debug) {
  using namespace tool::refl;

  const clang::ClassTemplateSpecializationDecl &template_decl = node;
  tl::expected _first_arg_type = get_template_arg_type(template_decl, 0);
  if (!_first_arg_type)
    return tl::unexpected(std::move(_first_arg_type).error());
  const clang::Type &first_arg_type = **_first_arg_type;

  // todo: `hasDefinition` - at this point (as of this writing) forward declarations are not
  // allowed
  // fixme: what about fundamental types??? I think I just need to skip them. Also need to write a
  // test that involves fundamental or non-user defined types. However, I think it is a rather
  // complex question whether I should allow it or not.
  if (!first_arg_type.isStructureOrClassType()) {
    return tl::unexpected(fmt::format("unsupported type {} in {}",
      // fixme: `getTypeClassName` doesn't do what I thought it does
      first_arg_type.getTypeClassName(),
      template_decl.getName()));
  }

  // omni::detail::_reflected_type<T>
  // type declaration of <T>
  const clang::CXXRecordDecl &first_arg_decl = *first_arg_type.getAsCXXRecordDecl();
  const std::string first_arg_nm_qual_type = first_arg_decl.getQualifiedNameAsString();
  if (ctx.reflected_types.contains(first_arg_nm_qual_type))
    return {};

  // recursivelly collected types (including self)
  const std::vector<const clang::CXXRecordDecl *> types =
    resolve_types_depends_upon(first_arg_decl, ctx.reflected_types);

  tl::expected<context, std::string> ctx_delta{tl::in_place};

  for (const clang::CXXRecordDecl *_rdecl : types) {
    const auto &rdecl = *_rdecl;
    const std::string nm_qual_type = rdecl.getQualifiedNameAsString();

    if (rdecl.isInStdNamespace()) {
      // todo: std include path should not be absolute
      ctx_delta->std_includes.emplace(get_declaration_source_file(rdecl, ast.getSourceManager()));
    } else {
      // `resolve_types_depends_upon` discards alrady reflected types, so `types` should not
      // contain the ones that are being inserted
      assert(!ctx.definitions.contains(nm_qual_type));
      ctx_delta->definitions[nm_qual_type] = resolve_definition(rdecl, ast.getSourceManager());
    }

    // refactorme: this code is duplicated when collecting types, but whatever
    ctx_delta->reflected_types.emplace(nm_qual_type,
      [](const auto &_fields) -> std::vector<struct_field_data> {
        std::vector<struct_field_data> r;
        for (const clang::FieldDecl *fd : _fields) {
          // todo: logging for skipped fields, since we are not reporting them as errors
          // todo: checks that would prevent the field from being reflected (uniouns,
          //    bitfields, what else?)
          // todo: consider supporting non-public types with selecting based on a typename,
          //    like `fields(specific_typename)`
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

  return ctx_delta;
}

tl::expected<context, std::string> reflected_impl::resolve(const node_type &node,
  const clang::ASTUnit &ast,
  const context &ctx,
  [[maybe_unused]] bool print_debug) {
  using namespace tool::refl;

  const clang::ClassTemplateSpecializationDecl &template_decl = node;
  tl::expected _first_arg_type = get_template_arg_type(template_decl, 0);
  if (!_first_arg_type)
    return tl::unexpected(std::move(_first_arg_type).error());
  const clang::Type &first_arg_type = **_first_arg_type;

  // todo: `hasDefinition` - at this point (as of this writing) forward declarations are not
  // allowed
  if (!first_arg_type.isStructureOrClassType()) {
    const std::string_view detail_struct_name = template_decl.getName();
    return tl::unexpected(fmt::format("unsupported type {} in {}",
      // fixme: `getTypeClassName` doesn't do what I thought it does
      first_arg_type.getTypeClassName(),
      detail_struct_name));
  }

  // type declaration of <T>
  const clang::CXXRecordDecl &rdecl = *first_arg_type.getAsCXXRecordDecl();
  // as of this writing there's a "fixme" to remove `std::string`, but it is not clear
  // whether a new type will be used or another function should be called
  const std::string nm_qual_type = rdecl.getQualifiedNameAsString();
  // fixme: do not use definitions, because reflected types also use them, but field info
  // should be collected only for reflected types
  if (ctx.definitions.contains(nm_qual_type))
    return {};

  tl::expected<context, std::string> ctx_delta{tl::in_place};
  ctx_delta->definitions[nm_qual_type] = resolve_definition(rdecl, ast.getSourceManager());
  ctx_delta->reflected_implementations.emplace(nm_qual_type);
  // todo: should we capture wrapped types (using value_type trait)?
  return ctx_delta;
}

tl::expected<context, std::string> reflected_call::resolve(const node_type &node,
  const clang::ASTUnit &ast,
  const context &ctx,
  [[maybe_unused]] bool print_debug) {
  using namespace tool::refl;
  const clang::CXXMethodDecl &refl_call_decl = node;

  tl::expected<context, std::string> ctx_delta{tl::in_place};
  func_signature &refl_call = ctx_delta->reflected_calls.emplace_back();
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
    //   since they are not supported at this point

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
    const auto &nm_qual_type = refl_call.args.back().nm_qual_type;

    // fixme: what about unions, built-in arrays?
    if (!type.isStructureOrClassType())
      continue;

    const clang::CXXRecordDecl &rd = *type.getAsCXXRecordDecl();

    if (rd.isInStdNamespace()) {
      // todo: add std include (but actually why? user struct's header will be included)
    }

    if (!ctx.definitions.contains(nm_qual_type) //
      && ctx_delta->definitions.contains(nm_qual_type)) {
      ctx_delta->definitions[nm_qual_type] = resolve_definition(rd, ast.getSourceManager());
    }
  }

  return ctx_delta;
}

tl::expected<context, std::string> _debug_templ_spec_decl::resolve(const node_type &node,
  const clang::ASTUnit &ast,
  [[maybe_unused]] const context &ctx,
  bool print_debug) {
  using namespace tool::refl;
  if (!print_debug)
    return {};

  const clang::ClassTemplateSpecializationDecl &template_decl = node;
  const std::string_view detail_struct_name = template_decl.getName();
  fmt::println("debug: ClassTemplateSpecializationDecl: {}", detail_struct_name);
  std::string buf;
  llvm::raw_string_ostream os{buf};
  clang::ASTDumper dmp{os, ast.getASTContext(), false};
  dmp.SetTraversalKind(clang::TK_AsIs);
  dmp.Visit(&template_decl);
  fmt::print("debug: ASTDump: {}\n", buf);
  return {};
}

} // namespace tool::refl::matches

tl::expected<tool::refl::context, std::string> //
  tool::refl::update(tool::refl::context _current,
    tool::refl::context delta,
    bool a_print_debug) noexcept {
  tl::expected<tool::refl::context, std::string> r{std::move(_current)};
  if (a_print_debug) {
    fmt::println("debug: current:");
    print_debug(*r);
    fmt::println("debug: delta:");
    print_debug(delta);
  }

  // in-place utility
  const auto merge_with_conflicts_check =
    []<typename K, typename T>(std::unordered_map<K, T> first,
      std::unordered_map<K, T> second,
      auto cmp) -> tl::expected<std::unordered_map<K, T>, std::string> {
    for (auto node = second.begin(); node != second.end();) {
      auto &&[k, v] = *node;
      const auto found = first.find(k);
      if (found == first.cend()) {
        first.insert(second.extract(node++));
        continue;
      }
      if (tl::expected res = cmp(k, v, found->second); !res)
        return tl::unexpected(std::move(res).error());
      ++node;
    }
    return {std::move(first)};
  };

  if (tl::expected merged = merge_with_conflicts_check(std::move(r->definitions),
        std::move(delta.definitions),
        [](std::string_view qual_typename,
          const type_definition_data &lhs,
          const type_definition_data &rhs) -> tl::expected<void, std::string> {
          if (lhs.source_file != rhs.source_file) {
            return tl::unexpected(
              fmt::format("found different locations for type {} definition: {}, {}",
                qual_typename,
                lhs.source_file.string(),
                rhs.source_file.string()));
          }
          if (lhs.definition_flags != rhs.definition_flags) {
            return tl::unexpected(
              fmt::format("found different definition flags for type {} definition: {}, {}",
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

  if (tl::expected merged = merge_with_conflicts_check(std::move(r->reflected_types),
        std::move(delta.reflected_types),
        [](std::string_view qual_typename,
          const std::vector<struct_field_data> &lhs,
          const std::vector<struct_field_data> &rhs) -> tl::expected<void, std::string> {
          if (!std::equal(lhs.cbegin(),
                lhs.cend(),
                rhs.cbegin(),
                [](const struct_field_data &lhs, const struct_field_data &rhs) -> bool {
                  return lhs.nm_qual_type == rhs.nm_qual_type;
                })) {
            // todo: print detailed info on fields that differ
            return tl::unexpected(
              fmt::format("found different data member types for type {}", qual_typename));
          }
          return {};
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
  if (tl::expected merged = merge_with_conflicts_check(std::move(r->reflected_types_indexes),
        std::move(delta.reflected_types_indexes),
        [](const int type_index, const std::string &lhs, const std::string &rhs)
          -> tl::expected<void, std::string> {
          return tl::unexpected(
            fmt::format("confict for type index {}, types: {}, {}", type_index, lhs, rhs));
        });
    !merged) {
    return tl::unexpected(std::move(merged).error());
  }

  if (a_print_debug) {
    fmt::println("debug: current updated:");
    print_debug(*r);
  }
  return r;
};
