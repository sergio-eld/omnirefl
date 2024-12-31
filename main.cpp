
// todo:
// - ast caching (?)
// - run queries on the ast and output the code

#include "tool/data.h"
#include "tool/tool_template.hpp"
#include "tool/util.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <tl/expected.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <clang/AST/ASTImporter.h>
#include <clang/AST/DeclBase.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/PrecompiledPreamble.h>
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Serialization/PCHContainerOperations.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/StringRef.h>
#pragma GCC diagnostic pop

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <optional>
#include <stack>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace util {

template <typename>
constexpr const std::tuple<> list_types = {};

template <template <typename...> class Template, typename... Types>
constexpr const auto list_types<Template<Types...>> = std::tuple{std::type_identity<Types>{}...};

template <typename T, typename Variant>
constexpr const size_t type_index = std::apply(
  []<typename... Ts>(std::type_identity<Ts>...) {
    constexpr const std::array _table{std::is_same_v<T, Ts>...};
    for (size_t i = 0; i < _table.size(); ++i)
      if (_table[i])
        return i;

    return size_t(-1);
  },
  list_types<Variant>);

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
} // namespace util

namespace {

struct matched_reflection {
  constexpr static const char binding_tag[] = "matched_reflection";
  using node_type = clang::ClassTemplateSpecializationDecl;
  const node_type *node;

  auto operator()() const noexcept {
    using namespace clang::ast_matchers;
    return classTemplateSpecializationDecl(unless(isInStdNamespace()),
      unless(isExpansionInSystemHeader()),
      hasAncestor(namespaceDecl(hasName("omni::detail"))),
      isTemplateInstantiation(),
      isDefinition(),
      isStruct());
  }
};

struct matched_reflected_call {
  constexpr static const char binding_tag[] = "matched_reflected_call";
  using node_type = clang::CXXMethodDecl;
  const node_type *node;

  auto operator()() const noexcept {
    using namespace clang::ast_matchers;
    return cxxMethodDecl( //
      unless(isInStdNamespace()),
      unless(isExpansionInSystemHeader()),
      hasAncestor(namespaceDecl(hasName("omni"))),
      hasAncestor(cxxRecordDecl(hasName("reflected_call_t"))),
      isTemplateInstantiation(),
      hasName("_call_impl"));
  }
};

// todo: context should be bound to list of matches to avoid compilation errors
// structure that collects and saves intermediate state from several ASTs
struct context {
  // todo: profile and optimize
  // definitions for reflected types and implementations
  std::unordered_map<std::string /*namespace_qualified_type*/, data::type_definition_data>
    definitions;

  // todo: is it possible to have unresolved reflected types between ASTs? as of this writing - no,
  // because we assume that forward declarations are not allowed. however, if intermediate forward
  // declarations are allowed (defined in different AST), we should add a container to track them
  std::unordered_map<std::string /*namespace_qualified_type*/, std::vector<data::struct_field_data>>
    reflected_types;

  std::set<std::string /*namespace_qualified_type*/> reflected_implementations;
  std::vector<data::func_signature> reflected_calls;

  // todo: profile and optimize with flat_set
  // unique list of std headers. used for reflecting std containers and aggregated types like
  // `std::tuple`, `std::variant`, `std::optional` and any other standard type that wraps a
  // reflected type and defines `type` or `value_type` trait
  std::set<std::string> std_includes;
};

// because stupid dap-ui doesn't show unordered_maps
void print_debug(const context &ctx) {
  fmt::println("debug: reflected_implementations: {}", ctx.reflected_implementations.size());
  for (const auto &i : ctx.reflected_implementations)
    fmt::println("  {}", i);
  fmt::println("debug: reflected_types: {}", ctx.reflected_types.size());
  for (const auto &i : ctx.reflected_types)
    fmt::println("  {}", i.first);
}

struct fold_matches_to_context {
  // todo: get rid of it from the members
  const clang::ASTUnit &m_ast;

  template <typename T>
  static tl::expected<void, std::string>
    _impl(const tool::matched_node<T> &m, const clang::ASTUnit &ast, context &ctx) noexcept {
    // refactorme: better type matching api
    if constexpr (std::is_same_v<matched_reflection, T>) {
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
          return {};

        ctx.definitions[nm_qual_type] =
          resolve_definition(nm_qual_type, rdecl, ast.getSourceManager());
        ctx.reflected_implementations.emplace(nm_qual_type);
        // todo: should we allow wrapped types (using value_type trait)?
        return {};
      }

      // omni::detail::_reflected_type<T>
      if ("_reflected_type" == detail_struct_name) {
        // type declaration of <T>
        const clang::CXXRecordDecl &first_arg_decl = *first_arg_type.getAsCXXRecordDecl();
        const std::string first_arg_nm_qual_type = first_arg_decl.getQualifiedNameAsString();
        if (ctx.reflected_types.contains(first_arg_nm_qual_type))
          return {};

        // recursivelly collected types (including self)
        const std::vector<const clang::RecordDecl *> types =
          dfs_fold_reflected_types(first_arg_decl, ctx.reflected_types);

        for (const clang::RecordDecl *_rdecl : types) {
          const auto &rdecl = *_rdecl;
          const std::string nm_qual_type = rdecl.getQualifiedNameAsString();

          if (rdecl.isInStdNamespace()) {
            // todo: std include path should not be absolute
            ctx.std_includes.emplace(
              util::get_declaration_source_file(rdecl, ast.getSourceManager()));
          } else {
            assert(!ctx.definitions.contains(nm_qual_type));
            ctx.definitions[nm_qual_type] =
              resolve_definition(nm_qual_type, rdecl, ast.getSourceManager());
          }

          // refactorme: this code is duplicated when collecting types, but whatever
          ctx.reflected_types.emplace(nm_qual_type,
            [](const auto &_fields) -> std::vector<data::struct_field_data> {
              std::vector<data::struct_field_data> r;
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

        return {};
      }

      return tl::unexpected("unexpected reflection tag");
    } else if constexpr (std::is_same_v<matched_reflected_call, T>) {
      const clang::CXXMethodDecl &refl_call_decl = *m.node;
      data::func_signature &refl_call = ctx.reflected_calls.emplace_back();
      refl_call.args.reserve(refl_call_decl.parameters().size());

      for (const clang::ParmVarDecl *parm_decl : refl_call_decl.parameters()) {
        const clang::QualType _qtype = parm_decl->getType(); //< keep alive
        const auto [type, is_const, ref_type] =
          // refactorme: it can return `data::func_arg`
          [](const clang::QualType &q) {
            struct _r {
              const clang::Type &type;
              bool is_const;
              data::reference_type ref_type;
            };
            if (q->isLValueReferenceType()) {
              return _r{
                .type = *q->getPointeeType().getTypePtr(),
                .is_const = q->getPointeeType().isConstQualified(),
                .ref_type = data::reference_type::ref_lval,
              };
            }
            if (q->isRValueReferenceType()) {
              return _r{
                .type = *q->getPointeeType().getTypePtr(),
                .is_const = q->getPointeeType().isConstQualified(),
                .ref_type = data::reference_type::ref_rval,
              };
            }
            return _r{
              .type = *q,
              .is_const = q.isConstQualified(),
              .ref_type = data::reference_type::ref_none,
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
      return {};
    } else {
      static_assert(((T*)nullptr, false), "non-exsaustive match");
    }
  }

  // refactorme: refine call arguments
  tl::expected<context, std::string> operator()(
    std::vector<tool::match_variant<matched_reflection, matched_reflected_call>> matches)
    const noexcept {
    tl::expected<context, std::string> r{tl::in_place};
    for (const auto &m : matches) {
      auto ctx = std::visit([&](const auto &_m) { return _impl(_m, m_ast, *r); }, m);
      if (!ctx)
        return tl::unexpected(std::move(ctx).error());
    }
    return r;
  };

  private:
  static data::type_definition_data resolve_definition(
    // namespace-qualified typename is needed before the call to this function to check whether
    // the definition has been already resolved
    std::string nm_qual_type,
    const clang::RecordDecl &rd,
    const clang::SourceManager &sm) noexcept {
    using td_flags = data::type_definition_flags;
    std::string source_file = util::get_declaration_source_file(rd, sm);
    const td_flags td_unnamed = td_flags::none; // todo:
    const td_flags td_local = td_flags::none; // todo:
    const td_flags td_in_cpp =
      util::is_header_file(source_file) ? td_flags::none : td_flags::in_cpp;

    return {
      .name = std::move(nm_qual_type),
      .source_file = std::move(source_file),
      .definition_flags = td_flags(td_unnamed | td_local | td_in_cpp),
    };
  };

  // types of interest for recursive reflection:
  //   - value_type: std containers, std wrappers, std::optional
  //   - key_type: std containers
  //   - type: std::reference_wrapper
  struct _member_typedef_decl {
    std::string_view name;
    clang::QualType qual_type;
  };

  static std::vector<_member_typedef_decl> member_typedefs(const clang::RecordDecl &rd) noexcept {
    // todo: use `filter`
    std::vector<_member_typedef_decl> r;
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
  static std::vector<const clang::RecordDecl *>
    dfs_fold_reflected_types(const clang::CXXRecordDecl &root, const auto &reflected_types_map) {
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
      for (const _member_typedef_decl &md : ::util::filtered(
             [](const _member_typedef_decl &m) -> bool {
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
  };
};

tl::expected<context, std::string> update(context _current, context delta) noexcept {
  tl::expected<context, std::string> r{std::move(_current)};
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
          const data::type_definition_data &lhs,
          const data::type_definition_data &rhs) -> std::optional<std::string> {
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
          const std::vector<data::struct_field_data> &lhs,
          const std::vector<data::struct_field_data> &rhs) -> std::optional<std::string> {
          if (!std::equal(lhs.cbegin(),
                lhs.cend(),
                rhs.cbegin(),
                [](const data::struct_field_data &lhs, const data::struct_field_data &rhs) -> bool {
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

struct emit_code_t {
  struct options {
    // todo: options
  };

  struct reflection_data {
    // used to generate `omni::reflected_t<user_type>` specializations
    struct reflected_type {
      // fully namespace-qualified type
      std::string name;
      std::vector<std::string> field_names;
    };

    // todo: profile and optimize (std::set -> std::vector)
    // list of unique header paths (non-reflection)
    std::set<std::string> includes;

    // list of unique header paths or reflected types' headers
    std::set<std::filesystem::path> refl_includes;

    // list of unique header paths or reflected implementations' headers
    std::set<std::string> refl_impl_includes;

    // list of unique reflected types
    std::set<reflected_type,
      decltype([](const reflected_type &lhs, const reflected_type &rhs) -> bool {
        // types are unique
        return lhs.name < rhs.name;
      })>
      reflected_types;

    // list of unique reflected call function signatures
    std::vector<data::func_signature> reflected_calls;
  };

  static tl::expected<reflection_data, std::string> prepare_input(context ctx) noexcept {
    tl::expected<reflection_data, std::string> r{tl::in_place};
    // todo: validate input

    r->includes = {std::make_move_iterator(ctx.std_includes.begin()),
      std::make_move_iterator(ctx.std_includes.end())};

    for (const auto &[nm_qual_type, fields] : ctx.reflected_types) {
      const auto definition = ctx.definitions.find(nm_qual_type);
      if (definition == ctx.definitions.cend())
        return tl::unexpected(fmt::format("no definition for reflected type {}", nm_qual_type));

      r->refl_includes.emplace(definition->second.source_file);
      r->reflected_types.insert({
        .name = nm_qual_type,
        .field_names =
          [](const std::vector<data::struct_field_data> &fields) -> std::vector<std::string> {
          std::vector<std::string> r;
          r.reserve(fields.size());
          std::transform(fields.cbegin(),
            fields.cend(),
            std::back_inserter(r),
            [](const data::struct_field_data &fd) { return fd.name; });
          return r;
        }(fields),
      });
    }

    for (const auto &nm_qual_type : ctx.reflected_implementations) {
      const auto definition = ctx.definitions.find(nm_qual_type);
      if (definition == ctx.definitions.cend())
        return tl::unexpected(
          fmt::format("no definition for reflected implementation type {}", nm_qual_type));
      const auto &sf = definition->second.source_file;
      if (!r->refl_includes.contains(sf))
        r->refl_impl_includes.emplace(sf);
    }

    std::set unique_func_signatures{std::make_move_iterator(ctx.reflected_calls.begin()),
      std::make_move_iterator(ctx.reflected_calls.end()),
      [](const data::func_signature &lhs, const data::func_signature &rhs) -> bool {
        // because c++ std is UGLEEEEEEE
        return std::lexicographical_compare(lhs.args.cbegin(),
          lhs.args.cend(),
          rhs.args.cbegin(),
          rhs.args.cend(),
          [](const data::function_signature_arg &lhs, const data::function_signature_arg &rhs)
            -> bool { return lhs.cvr_qualified_type < rhs.cvr_qualified_type; });
      }};

    r->reflected_calls = {std::make_move_iterator(unique_func_signatures.begin()),
      std::make_move_iterator(unique_func_signatures.end())};

    return r;
  }

  tl::expected<void, std::string>
    operator()(options, std::ostream &os, const reflection_data &data) const {
    os << "// This file has been generated by omnirefl tool (todo: timestamp, data, etc)."
          "\n// Do not modify this file manually.\n";

    os << "\n#include <omnirefl/refl.hpp>"
          "\n"
          "\n#include <tuple>"
          "\n#include <utility>";

    // todo: source paths should not be absolute.
    //    use `options::target_include_paths` to resolve relative paths.
    os << "\n\n// Headers of reflected types";
    for (const std::filesystem::path &p : data.refl_includes)
      os << fmt::format("\n#include \"{}\"", p.string());

    os << "\n\n// Headers of reflected implementations";
    for (const auto &p : data.refl_impl_includes)
      os << fmt::format("\n#include \"{}\"", p);

    os << "\n\n// Other headers";
    for (const auto &p : data.includes)
      os << fmt::format("\n#include <{}>", p);

    os << "\n\n// Generated specializations for types' reflection";
    os << "\n#ifndef OMNI_DEFINE_NAME_FUNC"
          "\n#  define OMNI_DEFINE_NAME_FUNC(STR) \\"
          "\nconstexpr static auto name() noexcept -> const char(&)[sizeof(STR)] { return STR; }"
          "\n#endif\n";
    for (const auto &refl_type : data.reflected_types) {
      os << fmt::format(
        // todo: annotate the header file where this type was included from
        "\ntemplate <>"
        "\nstruct omni::reflected_t<{type_name}>{{"
        "\n  using type = {type_name};"
        "\n"
        "\n  // fields meta types declarations"
        "{field_decls}"
        "\n"
        "\n  using fields_t = std::tuple<"
        "\n{field_names}"
        "\n  >;"
        "\n"
        "\n  constexpr reflected_binding<reflected_t<type>, type> operator()(type &t) const noexcept {{return {{t}};}}"
        "\n  constexpr reflected_binding<reflected_t<type>, const type> operator()(const type &t) const noexcept {{return {{t}};}}"
        "\n}};\n",
        fmt::arg("type_name", refl_type.name),
        fmt::arg("field_decls",
          fmt::join(
            util::with_fmt_rng{
              .value = std::cref(refl_type.field_names),
              .format =
                [](const auto &field_name, fmt::format_context &ctx) {
                  return fmt::format_to(ctx.out(),
                    "\n  struct {field_name}_t {{"
                    "\n    OMNI_DEFINE_NAME_FUNC(\"{field_name}\")"
                    "\n    constexpr static auto get_value(const type &t) noexcept"
                    "\n    -> const decltype(t.{field_name})& {{"
                    "\n      return t.{field_name};"
                    "\n    }}"
                    "\n"
                    "\n    template <typename V>"
                    "\n    void set_value(type &t, V &&v) {{"
                    "\n      t.{field_name} = std::forward<V>(v);"
                    "\n    }}"
                    "\n  }} constexpr static {field_name}{{}};",
                    fmt::arg("field_name", field_name));
                },
            },
            "\n")),
        fmt::arg("field_names",
          fmt::join(
            util::with_fmt_rng{
              .value = std::cref(refl_type.field_names),
              .format =
                [](const auto &field_name, fmt::format_context &ctx) {
                  return fmt::format_to(ctx.out(),
                    "    {field_name}_t",
                    fmt::arg("field_name", field_name));
                },
            },
            ",\n")));
    }
    os << "\n#undef OMNI_DEFINE_NAME_FUNC\n";

    os << "\n// Generated implementations for reflected calls";
    for (const data::func_signature &func_sig : data.reflected_calls) {
      // refactorme: this formatted printing doesn't really look cleaner than handwritten loops
      // fixme: otherwise lambda does not compile
      size_t arg_index = 0;
      os << fmt::format(
        "\ntemplate<>"
        "\nvoid omni::reflected_call_t::_call_impl("
        "\n  {params}) {{"
        "\n  {invoke}({args});"
        "\n}}",
        // refactorme: consider using fold to a substring instead
        fmt::arg("params",
          fmt::join(
            util::with_fmt_rng{
              // fixme: .value = util::indexed(fund_sig.args),
              .value = std::cref(func_sig.args),
              .format =
                [&arg_index](const data::function_signature_arg &f_arg, fmt::format_context &ctx) {
                  std::string param_name = 0 == arg_index //
                    ? std::string("impl")
                    : std::to_string(arg_index);
                  ++arg_index;
                  return fmt::format_to(ctx.out(),
                    "{nm_qual_type} {const}{ref}_{param_name}",
                    fmt::arg("nm_qual_type", f_arg.nm_qual_type),
                    fmt::arg("const", f_arg.is_const ? "const" : ""),
                    fmt::arg("ref",
                      [](data::reference_type r) -> std::string_view {
                        switch (r) {
                        case data::reference_type::ref_lval:
                          return "&";
                        case data::reference_type::ref_rval:
                          return "&&";
                        case data::ref_none:
                          return "";
                        }
                        return "/*unresolved_reference_type*/";
                      }(f_arg.ref_type)),
                    fmt::arg("param_name", std::move(param_name)));
                },
            },
            ",\n  ")),
        // refactorme: `util::indexed(util::sliced({1}, func_sig.args))`
        fmt::arg("invoke",
          [](const data::function_signature_arg &arg_impl) -> std::string_view {
            return data::reference_type::ref_rval == arg_impl.ref_type //
              ? "std::move(_impl)"
              : "_impl";
          }(func_sig.args.front())),
        // refactorme: this is outrigth ugly
        fmt::arg("args", [](auto begin, auto end) -> std::string {
          if (begin == end)
            return "";

          std::stringstream ss;
          size_t i = 1;
          if (data::reference_type::ref_rval == begin->ref_type)
            ss << "std::move(_" << i << ")";
          else
            ss << "_" << i;

          while (++begin != end) {
            ++i;
            if (data::reference_type::ref_rval == begin->ref_type)
              ss << ", std::move(_" << i << ")";
            else
              ss << ", _" << i;
          }
          return ss.str();
        }(std::next(func_sig.args.cbegin()), func_sig.args.cend())));
    }

    os << '\n';
    return {};
  }
} const inline emit_code{};

} // namespace

// refactorme: main should only contain a tool invocation code,
// allowing to configure specific things:
// - matchers
// - output to file
// - what else
int main(int argc, char **argv) {
  // disregard this comment. I'm experimenting with interface here
  /*
   *  std::pair{argc, argv}
   *  >>= chained
   *      | applied(tool::parse_cli) // -> cli_opts
   *      // will bind the result to the left
   *      | with_result_l([](const auto &cli) {
   *          return tool::load_compilation_db(cli->compilation_db_path);
   *      }) // -> tuple<std::unique_ptr<CompilationDb>, cli_opts>
   *      | with_result_l([](const auto &db, const auto &...) { return db.getAllFiles(); }
   *          | filtered(empty_str)
   *          | converted(to_std_path))
   *      // -> tuple<vector<path>, unique_ptr<CompilationDb>, cli_opts>
   *      | applied([](auto sources, auto db, auto cli_opts) {
   *          return tuple{map([&db](auto src) {
   *              auto opts = db.compileOptions(src);
   *              return tuple{std::move(src), std::move(opts)},
   *          stdd::move(db),
   *          std::move(cli_opts),
   *          };
   *      }) // -> tuple<vector<tuple<path, compile_options>>, db, cli>
   *      | applied([](auto compiled_sources, auto ... rest) {
   *          return tuple{
   *              foldl(tu_context{},
   *              // this actually requies `applied` but a little bit different
   *              [](auto ctx, const auto &src_path, const auto &compile_options){
   *                  auto delta_ctx = match_ast(src_path, compile_options);
   *                  return update(ctx, std::move(delta_ctx));
   *              }, compiled_sources),
   *              std::move(rest)...};
   *      }) // -> tuple<tu_context, db, cli>
   *      | // todo: validate accumulated tu_context and generate the .cpp file
   */

  auto cli = tool::parse_cli(argc, argv);
  if (!cli) {
    llvm::errs() << cli.error();
    return -1;
  }

  auto compilation_db = tool::load_compilation_db(cli->compilation_db_path);
  if (!compilation_db) {
    llvm::errs() << std::move(compilation_db).error();
    llvm::errs() << cli->compilation_db_path << '\n';
    return -1;
  }

  auto db_sources = util::to_std_paths((*compilation_db)->getAllFiles());
  if (!db_sources) {
    llvm::errs() << std::move(db_sources).error();
    llvm::errs() << cli->compilation_db_path << '\n';
    return -1;
  }

  const auto filtered_sources = tool::filter_db_sources(
    {
      .specified_sources = cli->sources,
      .excluded_folders = cli->excluded_folders,
    },
    *db_sources);
  if (!filtered_sources) {
    llvm::errs() << std::move(filtered_sources).error();
    llvm::errs() << cli->compilation_db_path << '\n';
    return -1;
  }

  // llvm doesn't know how to work with std::filesystem::path (so is fmt)
  // What a rotten way to die...
  const auto str_sources = [](const std::vector<std::filesystem::path> &paths) -> std::vector<std::string> {
    std::vector<std::string> res;
    res.reserve(paths.size());
    for (const auto &p : paths)
      res.push_back(p.string());
    return res;
  }(*filtered_sources);
  // todo: use verbosity option
  fmt::println("Filtered sources: [{}]", fmt::join(str_sources, ", "));

  // todo: fold
  context ctx{};
  size_t processing = 0;
  // todo: util::indexed(*filtered_sources)
  for (const auto &src : *filtered_sources) {
    fmt::println("[{}/{}] building AST of file: {}\t\r",
      ++processing,
      filtered_sources->size(),
      src.string());

    auto ast = tool::parse_ast({
      .resource_dir = cli->resource_dir,
      .source = src,
      .db = **compilation_db,
    });
    if (!ast) {
      llvm::errs() << ast.error();
      return -1;
    }

    // refactorme: list of template arguments here will cause a lot of problems, because the
    // order must be specified exactly like in other places
    auto matches = tool::match_ast(
      std::tuple{
        matched_reflection{},
        matched_reflected_call{},
      },
      **ast);
    if (!matches) {
      llvm::errs() << matches.error();
      return -1;
    }

    auto ctx_delta = fold_matches_to_context{.m_ast = **ast}(std::move(matches).value());
    if (!ctx_delta) {
      llvm::errs() << ctx_delta.error();
      return -1;
    }

    if (auto updated = update(std::move(ctx), std::move(ctx_delta).value()); updated) {
      ctx = std::move(updated).value();
    } else {
      llvm::errs() << updated.error();
      return -1;
    }
  }

  auto validated_reflection_data = emit_code_t::prepare_input(std::move(ctx));
  if (!validated_reflection_data) {
    llvm::errs() << validated_reflection_data.error();
    return -1;
  }

  fmt::println("Generating file: {}\n", cli->output_file.string());
  std::ofstream f{cli->output_file, std::ios::binary};
  if (const auto res = emit_code(
        {
          // todo: options
        },
        f,
        *validated_reflection_data);
      !res) {
    llvm::errs() << res.error() << '\n';
    return -1;
  };
  return 0;
}
