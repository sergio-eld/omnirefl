# macOS Cross Build

Source note: <https://stackoverflow.com/a/76183280/9363996>

Goal: cross-build the `omnirefl` tool for macOS from the Alpine build image.

## Findings

- Existing osxcross images are available on Docker Hub.
  - `liushuyu/osxcross:latest` was inspected.
  - It contains `/opt/osxcross/SDK/MacOSX10.14.sdk`.
  - It contains `o64-clang`, `x86_64-apple-darwin18-*`, `ld64`, `lipo`,
    `install_name_tool`, and related cctools.
  - It is old: created in 2020, Clang 10, Darwin18/x86_64 oriented.
  - It bundles an Apple SDK, so it has the same SDK redistribution concern.
  - Treat as a reference/probe image, not a release base.

- Darling has an SDK-like open-source tree.
  - Repo: <https://github.com/darlinghq/darling>
  - Path:
    `Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk`
  - C/POSIX headers are present.
  - `libSystem.dylib` is a symlink into Darling build output.
  - No `.tbd` stubs were found in the repo tree.
  - Not a drop-in SDK for release binaries, but useful for research.

- Apple open-source Darwin components are not a complete macOS SDK.
  - Repo: <https://github.com/apple-oss-distributions/distribution-macOS>
  - Useful source drops, not a packaged sysroot.

## Required Inputs

- Apple SDK/sysroot
  - `MacOSX.sdk` extracted from Xcode.
  - Not an Alpine package; must be imported from a Mac/Xcode install.

## Tools

- `MacOSX.sdk`
  - install: cannot be installed from Alpine packages
  - build: cannot be built from open source; copy/extract from Xcode or Apple
    Command Line Tools

- `clang`
  - install: Alpine `clang21` / existing prebuilt LLVM image layer
  - build: <https://github.com/llvm/llvm-project>

- `llvm-config`
  - install: existing prebuilt LLVM image layer
  - build: <https://github.com/llvm/llvm-project>

- `lld`
  - install: Alpine `lld21`
  - build: <https://github.com/llvm/llvm-project>

- `libc++` headers
  - install: existing generated LLVM runtimes headers in the image
  - build: <https://github.com/llvm/llvm-project/tree/main/runtimes>

- `apple-libtapi`
  - install: not available from Alpine packages
  - build: <https://github.com/tpoechtrager/apple-libtapi>

- `xar`
  - install: Alpine `xar`
  - build: <https://github.com/tpoechtrager/xar>

- `cctools-port`
  - install: not available from Alpine packages
  - build: <https://github.com/tpoechtrager/cctools-port>

- `ld`
  - install: built/installed by `cctools-port`
  - build: <https://github.com/tpoechtrager/cctools-port>

- `ar`
  - install: built/installed by `cctools-port`
  - build: <https://github.com/tpoechtrager/cctools-port>

- `ranlib`
  - install: built/installed by `cctools-port`
  - build: <https://github.com/tpoechtrager/cctools-port>

- `strip`
  - install: built/installed by `cctools-port`
  - build: <https://github.com/tpoechtrager/cctools-port>

- `libtool`
  - install: built/installed by `cctools-port`
  - build: <https://github.com/tpoechtrager/cctools-port>

- `lipo`
  - install: built/installed by `cctools-port`
  - build: <https://github.com/tpoechtrager/cctools-port>

- `install_name_tool`
  - install: built/installed by `cctools-port`
  - build: <https://github.com/tpoechtrager/cctools-port>

## LLVM Changes

- Add `AArch64` to `LLVM_TARGETS_TO_BUILD` for Apple Silicon.
- Consider adding `lld` to `LLVM_ENABLE_PROJECTS`.
  - May not be required if using cctools `ld64`.

Current Alpine image builds LLVM with `LLVM_TARGETS_TO_BUILD=X86` only.

## Alpine Build Dependencies

Likely needed to build the missing tools:

- `bash`
- `autoconf`
- `automake`
- `libtool`
- `pkgconf`
- `zlib-dev`
- `openssl-dev`
- `libxml2-dev`
- `musl-fts-dev` or equivalent

`musl-fts` matters because cctools-port documents musl-libc systems as needing
it.
