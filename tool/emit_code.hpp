#pragma once

#include "tool/reflection.hpp"

#include <tl/expected.hpp>

#include <set>
#include <string>
#include <vector>

namespace codegen {

// todo: support template info for forward declarations (inplace mode)
// used to generate `omni::reflected_t<user_type>` specializations
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

struct inplace_mode_reflection_data {
  // list of unique header paths for reflected types, which can't be forward
  // declared
  std::set<std::filesystem::path> refl_includes;

  // list of unique reflected types
  std::set<reflected_type, typename reflected_type::_cmp> reflected_types;
  std::unordered_map<size_t /*type_index*/, std::vector<std::string> /*fields*/>
    reflected_indexed_types;
};

tl::expected<target_mode_reflection_data, std::string>
  prepare_input(tool::refl::context ctx, tool::cli::target_mode) noexcept;

// todo: remove?
tl::expected<target_mode_reflection_data, std::string>
  prepare_input(tool::refl::context ctx, tool::cli::inplace_mode) noexcept;

struct options {
  // todo: options
  // - formatting
  // - annotating
};

tl::expected<void, std::string> emit_reflection_cpp_file(options,
  std::ostream &os,
  const target_mode_reflection_data &data);

// todo: remove
tl::expected<void, std::string> emit_inplace_reflection_header_file(options,
  std::ostream &os,
  const target_mode_reflection_data &data,
  const std::unordered_map<int, std::string> &index_type_map);

tl::expected<void, std::string> emit_inplace_reflection_header_file(options,
  std::ostream &os,
  const inplace_mode_reflection_data &data);

} // namespace codegen
