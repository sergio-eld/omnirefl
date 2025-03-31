#include "tool/reflection.hpp"

#include "tool/util.hpp"
#include "clang/ASTMatchers/ASTMatchers.h"

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
#include <cassert>
#include <cstddef>
#include <numeric>
#include <stack>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
// todo: use expected
std::string get_declaration_source_file(const clang::Decl &d,
  const clang::SourceManager &sm) {
  const auto loc = d.getLocation();
  if (!loc.isValid())
    return "";

  const auto pm = sm.getPresumedLoc(loc);
  if (!pm.isValid())
    return "";

  std::string filename = pm.getFilename();
  return filename;
}

// because stupid dap-ui doesn't show unordered_maps
void print_debug(const tool::refl::context &ctx) {
  fmt::println("debug: reflected implementations: {}",
    ctx.reflected_implementations.size());
  for (const auto &i : ctx.reflected_implementations)
    fmt::println("  {}", i);
  fmt::println("debug: reflected types: {}", ctx.reflected_types.size());
  for (const auto &i : ctx.reflected_types)
    fmt::println("  {}", i.first);

  fmt::println("debug: reflected indexed types: {}",
    ctx.detected_indexed_types.size());
  for (const auto &[index, nm_qual_typename] : ctx.detected_indexed_types)
    fmt::println("  {}: {}", index, nm_qual_typename);
}

tool::refl::type_definition_data resolve_definition(
  const clang::CXXRecordDecl &rd,
  const clang::SourceManager &sm) noexcept {
  using td_flags = tool::refl::type_definition_flags;
  std::string source_file = ::get_declaration_source_file(rd, sm);
  const td_flags td_unnamed =
    rd.getIdentifier() ? td_flags::none : td_flags::unnamed;
  const td_flags td_local =
    rd.isLocalClass() ? td_flags::local : td_flags::none;

  return {
    .source_file = std::move(source_file),
    .definition_flags = td_flags(td_unnamed | td_local),
    .is_class = rd.isClass(),
  };
}

std::vector<tool::refl::struct_field_data> resolve_struct_fields(
  const auto &fields_range) {
  std::vector<tool::refl::struct_field_data> r;
  for (const clang::FieldDecl *fd : fields_range) {
    // todo:
    // logging for skipped fields, since we are not reporting them
    // as errors todo: checks that would prevent the field from being
    // reflected (uniouns, bitfields, what else?)

    // todo:
    // consider supporting non-public types with selecting based on
    // a typename, like `fields(specific_typename)`
    if (clang::AccessSpecifier::AS_public != fd->getAccess())
      continue;
    r.push_back({
      .name = fd->getNameAsString(),
      // .nm_qual_type = fd->getQualifiedNameAsString(),
    });
  }
  return r;
}

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

/**
 * todo: documentation. This function might be a candidate for unit testing
 * todo: just 'collect' reflectable types, reflect everything later
 *
 * Fold AST from root to a vector of unique declarations that are not already
 * present in `reflected_types_map`
 * Collect and resolve types:
 * - from T::key_type, T::value_type, T::value
 * - T... from variant<T...> and tuple<T...>
 * - F from T::F field
 */
std::vector<const clang::CXXRecordDecl *> recursively_collect_reflectable_types(
  const clang::CXXRecordDecl &root,
  const auto &resolved_types) {
  // refactorme:
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
    // refactorme: ambiguous control flow/logic
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

    // fixme: just collect the types.
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

      // testme: struct with a tuple type field
      // todo: support only non-static fields
      to_visit.push(qt->getAsCXXRecordDecl());
    }
  }

  return {visited.begin(), visited.end()};
}

// todo: remove
// struct resolve_reflected_type_t {
//   // todo: this should be part of `matches`
//   struct result {
//     std::unordered_map<std::string /*nm_qual_typename*/,
//       tool::refl::type_definition_data>
//       definitions;
//     std::unordered_map<std::string /*namespace_qualified_type*/,
//       std::vector<tool::refl::struct_field_data>>
//       reflected_types;
//   };
//
//   // todo: tl::expected?
//   result operator()(const tool::refl::context &ctx,
//     const clang::CXXRecordDecl &reflected_type_decl,
//     const clang::ASTUnit &ast,
//     bool print_debug) const noexcept {
//     const std::vector<const clang::CXXRecordDecl *> types =
//       ::recursively_collect_reflectable_types(reflected_type_decl,
//         ctx.reflected_types);
//
//     result r;
//
//     // fixme: once I have a list of types to reflect, I need to perform the
//     // actual logic here.
//     // fixme: can these be repeated?
//     for (const clang::CXXRecordDecl *_rdecl : types) {
//       const auto &rdecl = *_rdecl;
//       const std::string nm_qual_type = rdecl.getQualifiedNameAsString();
//
//       if (rdecl.isInStdNamespace()) {
//         // todo: do I even care about the std includes? For target mode if a
//         // reflected struct has an std-typed field, including the struct's
//         // header is enough. For inplace mode no includes are needed, because
//         // forward declarations are used. todo: std include path should not
//         be
//         // absolute
//         r.std_includes.emplace(
//           ::get_declaration_source_file(rdecl, ast.getSourceManager()));
//       } else {
//         // `resolve_types_depends_upon` discards alrady reflected types, so
//         // `types` should not contain the ones that are being inserted
//         assert(!ctx.definitions.contains(nm_qual_type));
//
//         if (print_debug && nm_qual_type.empty())
//           fmt::println("DEBUG: empty namespace-qualified type resolved.");
//         if (print_debug && r.definitions.contains(nm_qual_type))
//           fmt::println("DEBUG: WARNING! {} already resolved", nm_qual_type);
//
//         // fixme: what about unnamed/local types?
//         r.definitions[nm_qual_type] =
//           resolve_definition(rdecl, ast.getSourceManager());
//       }
//
//       // fixme: what about unnamed/local types?
//       r.reflected_types.emplace(nm_qual_type,
//         ::resolve_struct_fields(rdecl.fields()));
//     }
//
//     return r;
//   }
// } constexpr resolve_reflected_type{};

tl::expected<clang::Type const *, std::string> get_template_type_arg(
  const clang::ClassTemplateSpecializationDecl &template_decl,
  size_t n) {
  const auto &template_args_list = template_decl.getTemplateArgs();

  if (template_args_list.size() < n) {
    const std::string_view detail_struct_name = template_decl.getName();
    return tl::unexpected(
      fmt::format("invalid template signature for internal omnirefl type {}",
        detail_struct_name));
  }

  const clang::TemplateArgument &arg = template_args_list.get(n);
  if (clang::TemplateArgument::ArgKind::Type != arg.getKind()) {
    const std::string_view detail_struct_name = template_decl.getName();
    return tl::unexpected(fmt::format("non-type template argument `{}` of {}",
      n,
      detail_struct_name));
  }

  return arg.getAsType().getTypePtr();
}

tl::expected<int, std::string> get_template_value_arg(
  const clang::ClassTemplateSpecializationDecl &template_decl,
  size_t n) {
  const auto &template_args_list = template_decl.getTemplateArgs();

  if (template_args_list.size() < n) {
    const std::string_view detail_struct_name = template_decl.getName();
    return tl::unexpected(
      fmt::format("invalid template signature for internal omnirefl type {}",
        detail_struct_name));
  }

  const clang::TemplateArgument &arg = template_args_list.get(n);
  if (clang::TemplateArgument::ArgKind::Integral != arg.getKind()) {
    const std::string_view detail_struct_name = template_decl.getName();
    return tl::unexpected(
      fmt::format("non-integral template argument `{}` of {}",
        n,
        detail_struct_name));
  }

  return arg.getAsIntegral().getExtValue();
}

} // namespace

namespace tool::refl::matches {

auto reflected_type::resolve(const node_type &node,
  const clang::ASTUnit &ast,
  const std::unordered_set<std::string> &resolved_types,
  bool print_debug) -> tl::expected<result, std::string> {
  // Reflected type is detected from `omni::detail::_reflected_type<T>`
  // specialization.
  const clang::ClassTemplateSpecializationDecl &template_decl = node;
  tl::expected _template_arg_0_type = ::get_template_type_arg(template_decl, 0);
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
    ::recursively_collect_reflectable_types(reflected_type_decl,
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
      .definition_data = resolve_definition(rdecl, ast.getSourceManager()),
      .fields = ::resolve_struct_fields(rdecl.fields()),
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

auto reflected_indexed_type::resolve(const node_type &node,
  const clang::ASTUnit &ast,
  const std::unordered_set<std::string> &resolved_types,
  bool print_debug) -> tl::expected<result, std::string> {
  using namespace tool::refl;

  // omni::detail::_type_index<typename reflected_type, int
  // reflected_type_index>
  const clang::ClassTemplateSpecializationDecl &template_decl = node;
  tl::expected _reflected_type = ::get_template_type_arg(template_decl, 0);
  if (!_reflected_type)
    return tl::unexpected(std::move(_reflected_type).error());

  const clang::Type &reflected_type = **_reflected_type;

  // todo: `hasDefinition` - at this point (as of this writing) forward
  // declarations are not allowed fixme: what about fundamental types??? I think
  // I just need to skip them. Also need to write a test that involves
  // fundamental or non-user defined types. However, I think it is a rather
  // complex question whether I should allow it or not.
  if (!reflected_type.isStructureOrClassType()) {
    return tl::unexpected(fmt::format("unsupported type {} in {}",
      // fixme: `getTypeClassName` doesn't do what I thought it does
      reflected_type.getTypeClassName(),
      template_decl.getName()));
  }

  tl::expected _reflected_type_index =
    ::get_template_value_arg(template_decl, 1);

  if (!_reflected_type_index)
    return tl::unexpected(std::move(_reflected_type_index).error());

  const int reflected_type_index = *_reflected_type_index;
  const clang::CXXRecordDecl &reflected_type_decl =
    *reflected_type.getAsCXXRecordDecl();

  const std::string nm_qual_type =
    reflected_type_decl.getQualifiedNameAsString();

  // todo: is empty possible?
  if (!nm_qual_type.empty() && resolved_types.contains(nm_qual_type))
    return {};

  // refactorme: this variable is mutable in the middle of the scope
  auto reflectable_data =
    ::recursively_collect_reflectable_types(reflected_type_decl,
      resolved_types);

  // testme: test with unnamed/local struct
  const auto matched_type_decl = std::find(reflectable_data.cbegin(),
    reflectable_data.cend(),
    &reflected_type_decl);
  if (reflectable_data.cend() == matched_type_decl) {
    // todo:
    //   what if parent node was not resolved? (i.e. an std type or fundamental)
  }

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

    // this should not be possible, since (I think) CXXRecordDecl* are all
    // unique types
    if (print_debug && reflected_types.contains(nm_qual_type))
      fmt::println("DEBUG: WARNING! {} already resolved", nm_qual_type);

    // fixme: what about unnamed/local types?
    reflected_types[nm_qual_type] = {
      .definition_data = resolve_definition(rdecl, ast.getSourceManager()),
      .fields = ::resolve_struct_fields(rdecl.fields()),
    };
  }

  // refactorme: UGLEEE BEATCH
  // fixme: what about unnamed, huh?
  auto matched_type = reflected_types.find(nm_qual_type);
  assert(reflected_types.cend() != matched_type);
  auto extracted = reflected_types.extract(matched_type);
  return result{
    .matched_type = {reflected_type_index, std::move(extracted).mapped()},
    // fixme: unnamed type
    .nm_qual_type = nm_qual_type,
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
  tl::expected _template_arg_0 = ::get_template_type_arg(template_decl, 0);
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
      ::resolve_definition(reflected_type_decl, ast.getSourceManager()),
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
    auto definition = ::resolve_definition(rd, ast.getSourceManager());
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
  if (a_print_debug) {
    fmt::println("debug: current:");
    ::print_debug(*r);
    fmt::println("debug: delta:");
    ::print_debug(delta);
  }

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

  if (a_print_debug) {
    fmt::println("debug: current updated:");
    ::print_debug(*r);
  }
  return r;
};
