#pragma once

// definitions of data used between transformations
#include <filesystem>
#include <vector>

// todo: doesn't make much sense to separate the input/output data from transformations
// however, data types, specific to the actual program might make sense here
namespace data {

struct cli_opts {
  /// directory for clang's system headers (bundled)
  std::filesystem::path resource_dir;

  /// path to where generate the reflected implementation
  std::filesystem::path output_file;

  // todo: group options specific to invocation with compilation db.
  // one may want to invoke the tool on a single source file with compilation args

  /// path to compilation database (currenly only compile_commands.json)
  std::filesystem::path compilation_db_path;

  // todo: can they be optional?
  /// .cpp files to invoke the tool for.
  /// must be found within the compilation_db
  std::vector<std::filesystem::path> sources;

  std::vector<std::filesystem::path> excluded_folders;
};

// flags for definition properties
// these are used to determine violated limitations when using the tool
enum type_definition_flags {
  none = 0x0,
  // unnamed structure
  unnamed = 0x1,
  // definition within a scope
  local = 0x1 << 1,
  // defined within a .cpp file (source): when using a standalone tool
  // definition can (probably) be generated within the same .cpp file via
  // a force-include compiler command
  in_cpp = 0x1 << 2,
};

enum reference_type {
  ref_none,
  ref_lval,
  ref_rval,
};

struct type_definition_data {
  // todo: remove, use map key instead
  std::string name;
  std::filesystem::path source_file;
  type_definition_flags definition_flags = none;
};

struct struct_field_data {
  std::string name;

  // fully namespace-qualified type
  std::string nm_qual_type;

  // todo: I don't know how this can be useful, since there's a simple workaround for accessing
  // bit fields without a member pointer
  // bool is_bitfield;
};

struct function_signature_arg {
  // todo: better store qualifiers and the typename separatelly
  // fully cv and namespace-qualified type
  std::string cvr_qualified_type;
  std::string nm_qual_type;
  bool is_const : 1;
  reference_type ref_type;
};

struct func_signature {
  std::vector<function_signature_arg> args;
};
// todo: compiler invocation
// todo: ast matchers
// {src_path, compiler_commands}
//  | parse_ast
//  | fold(matchers)
//  |

} // namespace data
