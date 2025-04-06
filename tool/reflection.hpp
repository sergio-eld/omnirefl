#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tool::refl {

// todo: benchmark, then use hash unique type identifier
using type_id = std::string;

enum type_definition_flags {
  none = 0x0,

  // in global scope is not visible as public
  non_public = 0x1 << 0,

  // definition within a scope
  local = 0x1 << 1,
};

enum reference_type {
  ref_none,
  ref_lval,
  ref_rval,
};

struct type_definition_data {
  std::filesystem::path source_file;
  std::optional<std::string> nm_qual_type;

  type_definition_flags definition_flags;

  // refactorme: should be enum
  // - fundamental
  // - array
  // - struct
  // - class
  // - union
  bool is_class;
};

struct struct_field_data {
  std::string name;
};

// todo: only needed for tu-mode (former 'target' mode)
struct function_signature_arg {
  // fully namespace-qualified type
  std::string nm_qual_type;
  bool is_const : 1;
  reference_type ref_type;
};

// todo: only needed for tu-mode (former 'target' mode)
struct func_signature {
  std::vector<function_signature_arg> args;
};

namespace matches {

// struct _debug_templ_spec_decl {
//   static constexpr const char binding_tag[] = "asshole_tag";
//
//   using node_type = clang::ClassTemplateSpecializationDecl;
//
//   auto operator()() const noexcept {
//     using namespace clang::ast_matchers;
//     return classTemplateSpecializationDecl( //
//       hasAncestor(namespaceDecl(hasName("omni"))),
//       hasAncestor(namespaceDecl(hasName("detail"))),
//       isTemplateInstantiation(),
//       isDefinition()
//       //
//     );
//   }
//
//   struct result {};
//   // todo: can I avoid having the same signature?
//   static tl::expected<result, std::string> resolve(const node_type &node,
//     const clang::ASTUnit &ast,
//     const std::unordered_set<std::string> &resolved_types,
//     bool print_debug = false);
// };

} // namespace matches

// refactorme:
//   remove the 'blob' context class.
//   For inplace mode we only need to generate the types within a single
//   translation unit, so they must be isolated.
//   For target mode, the generated reflection symbols will be shared across
//   multiple translation units, so they must be shared... More over, in target
//   mode only non-local types that are defined in headers are allowed.
//
// todo: can this type really be independent from the matches?
// structure that collects and saves intermediate state from several ASTs
//
// struct context {
//   // todo: profile and optimize
//   // definitions for reflected types and implementations
//   std::unordered_map<std::string /*namespace_qualified_type*/,
//     type_definition_data>
//     definitions = {};
//
//   // todo: is it possible to have unresolved reflected types between ASTs? as
//   of
//   // this writing - no, because we assume that forward declarations are not
//   // allowed. however, if intermediate forward declarations are allowed
//   (defined
//   // in different AST), we should add a container to track them
//   std::unordered_map<std::string /*namespace_qualified_type*/,
//     std::vector<struct_field_data>>
//     reflected_types = {};
//
//   std::unordered_map<size_t, std::string /*namespace-qualified typename*/>
//     detected_indexed_types = {};
//
//   // as of this writing, for indexed types only struct fields matter
//   std::unordered_map<size_t /*type_index*/, std::vector<struct_field_data>>
//     reflected_indexed_types = {};
//
//   std::set<std::string /*namespace_qualified_type*/>
//   reflected_implementations =
//     {};
//   std::vector<func_signature> reflected_calls = {};
//
//   // todo: profile and optimize with flat_set
//   // unique list of std headers. used for reflecting std containers and
//   // aggregated types like `std::tuple`, `std::variant`, `std::optional` and
//   any
//   // other standard type that wraps a reflected type and defines `type` or
//   // `value_type` trait
//   std::set<std::string> std_includes = {};
// };
//
// tl::expected<tool::refl::context, std::string>
//   update(context current, context delta, bool print_debug) noexcept;
} // namespace tool::refl
