#pragma once

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

// todo: support reflection of non-reflectable dependency types
struct reflected_type_data {
  refl::type_definition_flags definition_data;
  std::vector<refl::struct_field_data> fields;
};

// todo:
//   do I need to add a specialization based on `mode`? For inplace mode headers
//   are not needed
//
// types reflected via instantiation of a designated template struct
struct reflected_type {
  // todo: remove. `binding_tag` is implementation-specific
  constexpr static const char binding_tag[] = "reflected_type";

  // todo: this should be deducible from the ASTMatchers' expression
  using node_type = clang::ClassTemplateSpecializationDecl;
  auto operator()() const noexcept {
    using namespace clang::ast_matchers;

    // matching `omni::detail::_reflected_type<T>`
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

  struct result {
    // nullopt, if data has been already reflected, or if it is a
    // non-reflectable type: for example, std::tuple<T...> itself can't be
    // reflected, but types `T...` can.
    std::optional<std::pair<std::string /*nm_qual_type*/, reflected_type_data>>
      matched_type;
    std::unordered_map<std::string, reflected_type_data>
      matched_type_dependencies;
  };

  static tl::expected<result, std::string> resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const std::set<refl::type_id> &resolved_types,
    bool print_debug = false);
};

// todo:
//   consider removing this separate tag and just use `reflected_call`. having
//   those as separate introduces possible inconsistency: reflected_call without
//   reflected_impl doesn't make any sense
//
// implementation types reflected via instantiation of a designated template
// struct
struct reflected_impl {
  // todo: remove. `binding_tag` is implementation-specific
  constexpr static const char binding_tag[] = "reflected_impl";

  // todo: this should be deducible from the ASTMatchers' expression
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
    // namespace-qualified type of the callable type
    std::string callable_nm_qual_type;
    refl::type_definition_flags callable_definition_data;
  };

  static tl::expected<result, std::string> resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const std::set<refl::type_id> &resolved_types,
    bool print_debug = false);
};

// invocations
// only needed for target mode
struct reflected_call {
  constexpr static const char binding_tag[] = "matched_reflected_call";

  // todo: this should be deducible from the ASTMatchers' expression
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
    refl::func_signature call_signature;
    std::set<std::filesystem::path> std_includes;
    std::set<std::filesystem::path> includes;
  };

  static tl::expected<result, std::string> resolve(const node_type &node,
    const clang::ASTUnit &ast,
    const std::set<refl::type_id> &resolved_types,
    bool print_debug = false);
};
} // namespace match

namespace codegen {

// used to generate `omni::reflected_t<reflected_type>` specializations
struct reflected_type {
  // fully namespace-qualified type
  std::string name;

  // list of public fields
  std::vector<tool::refl::struct_field_data> fields;

  // todo: use enum from `TagTypeKind::`, but what about `Enum`?
  // `reflected_type::field_names` shouldn't be 'reused'
  bool is_class;

  struct _cmp {
    bool operator()(const reflected_type &lhs,
      const reflected_type &rhs) const noexcept {
      // types are unique
      return lhs.name < rhs.name;
    }
  };
};
// todo: consider reusing data types from `reflection.hpp`
// this struct can and should be used for standalone unit testing (without
// actually building and ast or parsing a source file)
struct target_mode_reflection_data {
  // todo: profile and optimize (std::set -> std::vector)
  // list of unique header paths (non-reflection)
  std::set<std::string> includes;

  // list of unique header paths or reflected types' headers
  std::set<std::filesystem::path> refl_includes;

  // list of unique header paths or reflected implementations' headers
  std::set<std::string> refl_impl_includes;

  // list of unique reflected types
  std::set<reflected_type, typename reflected_type::_cmp> reflected_types;

  // list of unique reflected call function signatures
  std::vector<tool::refl::func_signature> reflected_calls;
};

// fixme:
// tl::expected<target_mode_reflection_data, std::string>
//   prepare_input(tool::refl::context ctx, tool::cli::target_mode) noexcept;

struct options {
  // todo: options
  // - formatting
  // - annotating
};

tl::expected<void, std::string> emit_reflection_cpp_file(options,
  std::ostream &os,
  const target_mode_reflection_data &data);

} // namespace codegen
} // namespace tool::source_mode
