#pragma once
// CLang AST utility functions

#include <tool/reflection.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated"
#include <clang/AST/Decl.h>
#include <clang/AST/DeclBase.h>
#include <clang/AST/DeclTemplate.h>
#pragma GCC diagnostic pop

#include <expected>
#include <set>
#include <string>
#include <vector>

namespace util::ast {

// todo: use expected
std::string get_declaration_source_file(const clang::Decl &d,
  const clang::SourceManager &sm) noexcept;

tool::refl::type_definition_data resolve_definition(
  const clang::CXXRecordDecl &rd,
  const clang::SourceManager &sm) noexcept;

std::vector<tool::refl::struct_field_data> resolve_struct_fields(
  clang::RecordDecl::field_range fields_range) noexcept;

/**
 * refactorme:
 *   `recursively_collect_dependency_types`
 *
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
std::vector<const clang::CXXRecordDecl *> recursively_collect_dependency_types(
  const clang::CXXRecordDecl &root,
  const std::set<tool::refl::type_id> &resolved_types) noexcept;

std::expected<clang::Type const *, std::string> get_template_type_arg(
  const clang::ClassTemplateSpecializationDecl &template_decl,
  size_t n) noexcept;

std::expected<int, std::string> get_template_value_arg(
  const clang::ClassTemplateSpecializationDecl &template_decl,
  size_t n) noexcept;

} // namespace util::ast
