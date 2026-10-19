# Conan 2

For consumer requirements, see [Project integration](../README.md#project-integration).
To build a commit instead of downloading a release, add to `conanfile.txt`:

```ini
[options]
omnirefl/*:commit=<full-commit-hash>
```

The recipe checks out that commit and invokes Docker Compose. This requires
Git, Docker with Linux-container support, and an x86_64 build host. ARM64 hosts
can consume the prebuilt distribution.

CMake consumers use Ninja or Makefiles to produce the compilation database
required by Omnirefl. For Conan's CMake toolchain, Ninja can be selected in the
profile:

```ini
[conf]
tools.cmake.cmaketoolchain:generator=Ninja
```

## Recipe availability

Until the recipe is published, fetch only its directory and export it once:

```sh
git clone --depth 1 --filter=blob:none --no-checkout https://github.com/sergio-eld/omnirefl.git omnirefl-recipe
git -C omnirefl-recipe sparse-checkout set --no-cone /conan/
git -C omnirefl-recipe checkout
conan export omnirefl-recipe/conan
```

Use `--build=missing` with your normal `conan install` command so Conan runs the
recipe when its binary package is not cached. The recipe handles acquisition
and preserves Omnirefl's native CMake package.
