#pragma once

#include "tool/cli.hpp"
#include "tool/reflection.hpp"

#include <tl/expected.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <clang/AST/DeclTemplate.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Frontend/ASTUnit.h>
#pragma GCC diagnostic pop

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace tool::source_mode {
namespace match {

struct function_signature_arg {
  // fully namespace-qualified type
  std::string nm_qual_type;
  bool is_const : 1;
  refl::reference_type ref_type;
};

struct func_signature {
  std::vector<function_signature_arg> args;
};

// todo: support reflection of non-reflectable dependency types
struct reflected_type_data {
  refl::type_definition_data definition;
  std::vector<refl::struct_field_data> fields;
};

struct reflectable {
  refl::type_id id;

  reflected_type_data reflected_type;
  std::map<refl::type_id, reflected_type_data> type_dependencies;
};

struct already_reflected {
  refl::type_id id;
};

struct non_reflectable {
  refl::type_id id;
  std::optional<reflected_type_data> definition;
  std::map<refl::type_id, reflected_type_data> type_dependencies;
};

// types reflected via instantiation of a designated template struct
struct reflected_type {
  constexpr static const char binding_tag[] = "reflected_type";
  using node_type = clang::ClassTemplateSpecializationDecl;

  auto operator()() const noexcept {
    using namespace clang::ast_matchers;

    return classTemplateSpecializationDecl( //
      unless(isInStdNamespace()),
      hasAncestor(namespaceDecl(hasName("omni"))),
      hasAncestor(namespaceDecl(hasName("detail"))),
      hasName("_reflected_type"),
      isTemplateInstantiation(),
      isDefinition()
      //
    );
  }

  using result = std::variant<reflectable, already_reflected, non_reflectable>;
  static auto resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const std::set<refl::type_id> &resolved_types,
    bool print_debug = false) noexcept -> tl::expected<result, std::string>;
};

// refactorme:
//   consider removing this separate tag and just use `reflected_call`. having
//   those as separate introduces possible inconsistency: reflected_call without
//   reflected_impl doesn't make any sense
struct reflected_impl {
  constexpr static const char binding_tag[] = "reflected_impl";
  using node_type = clang::ClassTemplateSpecializationDecl;

  auto operator()() const noexcept {
    using namespace clang::ast_matchers;

    return classTemplateSpecializationDecl( //
      unless(isInStdNamespace()),
      hasAncestor(namespaceDecl(hasName("omni"))),
      hasAncestor(namespaceDecl(hasName("detail"))),
      hasName("_reflected_impl"),
      isTemplateInstantiation(),
      isDefinition()
      //
    );
  }

  struct result {
    refl::type_id id;
    refl::type_definition_data definition_data;
  };

  static auto resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const std::set<refl::type_id> &resolved_types,
    bool print_debug = false) noexcept -> tl::expected<result, std::string>;
};

struct reflected_call {
  constexpr static const char binding_tag[] = "matched_reflected_call";
  using node_type = clang::CXXMethodDecl;

  auto operator()() const noexcept {
    using namespace clang::ast_matchers;
    return cxxMethodDecl( //
      unless(isInStdNamespace()),
      hasAncestor(namespaceDecl(hasName("omni"))),
      hasAncestor(cxxRecordDecl(hasName("reflected_call_t"))),
      isTemplateInstantiation(),
      hasName("_call_impl"));
  }

  struct result {
    func_signature call_signature;
    std::set<std::filesystem::path> std_includes;
    std::set<std::filesystem::path> includes;
  };

  static auto resolve(const node_type &node, const clang::ASTUnit &ast) noexcept
    -> tl::expected<result, std::string>;
};

} // namespace match

namespace transforms {

// accumulated translation unit data
struct tu_data {
  // caching
  std::set<refl::type_id> resolved_types;

  std::map<refl::type_id, match::reflected_type_data> reflected_types = {};
  std::map<refl::type_id, match::reflected_type_data>
    reflected_dependency_types = {};
  std::map<refl::type_id, refl::type_definition_data> reflected_impls = {};

  std::vector<match::func_signature> reflected_calls = {};

  // detected from 'other' args of `reflected_call`
  std::set<std::filesystem::path> std_includes = {};
  std::set<std::filesystem::path> includes = {};

  // todo: forward declarations to be resolved (in the next TUs)

  cli::verbosity_level _verbosity;
};

template <typename Match>
auto resolve_reflected_node(const typename Match::node_type &n,
  const clang::ASTUnit &ast,
  const tu_data &tu_data) noexcept
  -> tl::expected<typename Match::result, std::string> {
  if constexpr (std::is_same_v<match::reflected_call, Match>) {
    return Match::resolve(n, ast);
  } else {
    return Match::resolve(n,
      ast,
      tu_data.resolved_types,
      cli::print_debug(tu_data._verbosity));
  }
}

template <typename... Matches>
using match_result_variant = std::variant<typename Matches::result...>;

auto fold_resolved_types(tu_data accum,
  match_result_variant<match::reflected_type,
    match::reflected_impl,
    match::reflected_call> result) noexcept
  -> tl::expected<tu_data, std::string>;

} // namespace transforms

namespace codegen {

// used to generate `omni::reflected_t<reflected_type>` specializations
struct reflected_specialization_data {
  // fully namespace-qualified type
  std::string nm_qual_type;

  // list of public fields
  std::vector<tool::refl::struct_field_data> fields;

  // todo: enum
  bool is_class;

  friend bool operator<(const reflected_specialization_data &lhs,
    const reflected_specialization_data &rhs) noexcept {
    return lhs.nm_qual_type < rhs.nm_qual_type;
  }
};

struct reflection_data {
  // list of unique header paths (non-reflection)
  std::set<std::string> includes;

  // list of unique header paths or reflected types' headers
  std::set<std::filesystem::path> refl_includes;

  // list of unique header paths or reflected implementations' headers
  std::set<std::string> refl_impl_includes;

  // list of unique reflected types
  std::set<reflected_specialization_data> reflected_types;

  // list of unique reflected call function signatures
  std::vector<match::func_signature> reflected_calls;
};

tl::expected<reflection_data, std::string> prepare_data(
  std::map<std::filesystem::path, transforms::tu_data>
    reflection_data_by_source) noexcept;

struct options {
  // todo:
  // - formatting
  // - annotating
};

tl::expected<void, std::string> emit_reflection_cpp_file(options,
  std::ostream &os,
  const reflection_data &data);

} // namespace codegen
} // namespace tool::source_mode
