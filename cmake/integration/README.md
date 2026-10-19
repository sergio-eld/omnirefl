# FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(omnirefl
    GIT_REPOSITORY https://github.com/sergio-eld/omnirefl.git
    GIT_TAG <release-tag-or-full-commit>
    SOURCE_SUBDIR cmake/integration)
FetchContent_MakeAvailable(omnirefl)

find_package(omnirefl CONFIG REQUIRED)
omni_reflected_target(my_target)
```

Requires CMake 3.19 or later. Use the declaration name `omnirefl`.
Release tags (`vX.Y.Z`) download the corresponding verified universal archive.
Full commit hashes build a universal package through Docker Compose, requiring
an x86_64 host with Linux-container support. Both modes preserve the native
CMake package, including optional components.

The selected revision must contain this adapter; older releases cannot use
this entry point. Source builds keep their package in the consumer's build
tree and reuse it on subsequent configurations.

When running CMake inside a container, Docker must be available there and the
checkout and build directories must have the same absolute paths on the Docker
host, so Compose's bind mounts refer to the correct files.
