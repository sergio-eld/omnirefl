#pragma once

// definitions of data used between transformations
#include <filesystem>
#include <vector>

namespace data {

struct cli_opts {
    /// path to compilation database (currenly only compile_commands.json)
    std::filesystem::path compilation_db_path;

    /// path to where generate the reflected implementation
    std::filesystem::path output_file;

    // todo: can they be optional?
    /// .cpp files to invoke the tool for. 
    /// must be found within the compilation_db
    std::vector<std::filesystem::path> sources;

    std::vector<std::filesystem::path> excluded_folders;

    /// directory for clang's system headers (bundled)
    std::filesystem::path resource_dir;
};

} // namespace data
