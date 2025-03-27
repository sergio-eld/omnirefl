#pragma once

#include "tool/reflection.hpp"

#include <tl/expected.hpp>

#include <set>
#include <string>
#include <vector>

namespace codegen {

// used to generate `omni::reflected_t<user_type>` specializations
struct reflected_type {
  // fully namespace-qualified type
  std::string name;

  // list of public fields
  std::vector<std::string> field_names;
  // todo: use enum from `TagTypeKind::`, but what about `Enum`?
  // `reflected_type::field_names` shouldn't be 'reused'
  bool is_class;
};

// todo: consider reusing data types from `reflection.hpp`
// this struct can and should be used for standalone unit testing (without
// actually building and ast or parsing a source file)
struct reflection_data {
  // todo: profile and optimize (std::set -> std::vector)
  // list of unique header paths (non-reflection)
  std::set<std::string> includes;

  // list of unique header paths or reflected types' headers
  std::set<std::filesystem::path> refl_includes;

  // list of unique header paths or reflected implementations' headers
  std::set<std::string> refl_impl_includes;

  struct _cmp_reflected_types {
    bool operator()(const reflected_type &lhs,
      const reflected_type &rhs) const noexcept {
      // types are unique
      return lhs.name < rhs.name;
    }
  };
  // list of unique reflected types
  std::set<reflected_type, _cmp_reflected_types> reflected_types;

  // list of unique reflected call function signatures
  std::vector<tool::refl::func_signature> reflected_calls;
};

tl::expected<reflection_data, std::string> prepare_input(
  tool::refl::context ctx) noexcept;

struct options {
  // todo: options
  // - formatting
  // - annotating
};
tl::expected<void, std::string> emit_reflection_cpp_file(options,
  std::ostream &os,
  const reflection_data &data);

tl::expected<void, std::string> emit_inplace_reflection_header_file(options,
  std::ostream &os,
  const reflection_data &data,
  const std::unordered_map<int, std::string> &index_type_map);

} // namespace codegen
