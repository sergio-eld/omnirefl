#pragma once

#include <filesystem>
#include <optional>
#include <string>

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

namespace match {

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
} // namespace tool::refl
