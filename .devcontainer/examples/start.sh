#!/bin/sh
set -eu

cd "$(dirname "$0")/../.."
tar -xzf artifacts/devcontainer/omnirefl.tar.gz -C /usr/local --strip-components=1

# Seed the ignored sandbox without replacing edits on subsequent starts.
mkdir -p share/examples/scratch

if [ ! -e share/examples/scratch/CMakeLists.txt ]; then
  cat > share/examples/scratch/CMakeLists.txt <<'EOF'
# Local scratchpad, ignored by Git. Edit freely to experiment with reflection.
cmake_minimum_required(VERSION 3.18.2 FATAL_ERROR)
project(scratch LANGUAGES CXX)

# The starter files are C++11-compatible; change this to try newer standards.
set(CMAKE_CXX_STANDARD 11)

if(NOT TARGET omni::tool)
    find_package(omnirefl CONFIG REQUIRED)
endif()

add_executable(scratch main.cpp)
omni_reflected_target(scratch)
EOF
fi

if [ ! -e share/examples/scratch/field_names.hpp ]; then
  cat > share/examples/scratch/field_names.hpp <<'EOF'
// Local scratchpad, ignored by Git. Edit freely to experiment with reflection.
#pragma once

#include <omnirefl/reflected_scope.hpp>

#include <string>
#include <tuple>
#include <vector>

// Use through omni::reflected_call to collect public field names.
struct field_names {
  template <typename _M>
  std::vector<std::string> operator()(omni::record_meta_t<_M> record) const {
    return names(record.public_fields());
  }

  private:
  // C++11 expands the metadata types without a generic lambda.
  template <typename... Fields>
  static std::vector<std::string> names(const std::tuple<Fields...> &) {
    return {Fields::name()...};
  }
};
EOF
fi

if [ ! -e share/examples/scratch/main.cpp ]; then
  cat > share/examples/scratch/main.cpp <<'EOF'
// Local scratchpad, ignored by Git. Edit freely to experiment with reflection.
#include "field_names.hpp"

#include <iostream>
#include <string>

struct person {
  std::string name;
  unsigned age;

  protected:
  unsigned internal_id;
};

int main() {
  const auto names = omni::reflected_call(field_names{}, omni::type_t<person>{});
  for (const auto &name : names)
    std::cout << name << '\n';
}
EOF
fi

# Configure after creating scratch so its compile command is available.
cd share
if [ "--fresh" = "${1:-}" ]; then
  cmake --fresh --preset examples
elif [ -f build/CMakeCache.txt ]; then
  cmake -S . -B build
else
  cmake --preset examples
fi

