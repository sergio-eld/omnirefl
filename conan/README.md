# Conan 2

From the repository root, install the versioned universal distribution into
your local Conan cache:

```sh
conan profile detect
conan create conan
```

To build the checked-out commit instead:

```sh
conan create conan -o "omnirefl/*:commit=$(git rev-parse HEAD)"
```

The `commit` option also accepts any other full Git commit hash.
Source builds require Git, Docker Compose, and an x86_64 build host with
Linux-container support. They pull the existing toolchain image and build a
universal package through `build-cosmo`. ARM64 hosts can consume the prebuilt
distribution.

Add `omnirefl/0.0.5` to your consumer's Conan requirements and generate
`CMakeToolchain` and `CMakeDeps`. To consume a source-built package, pass the same
`omnirefl/*:commit` option to `conan install`.
With the generated toolchain, use the installed CMake package:

```cmake
find_package(omnirefl CONFIG REQUIRED)
omni_reflected_target(my_target)
```

The recipe preserves the distribution's CMake configuration, bundled projects,
and Clang resource headers. Optional extensions retain their existing CMake
dependency resolution.
