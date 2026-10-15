# Containers

Choose a configuration when creating a Codespace. In local VS Code, use
**Dev Containers: Switch Container**. Separate Codespaces have separate
checkouts.

## Examples (Ubuntu 24.04)

- **Omnirefl bundled examples - latest release** (default): use the latest
  GitHub release.
- **Omnirefl bundled examples - current source**: use this checkout, including
  local changes.

The Explorer opens `share/`, and the terminal opens `share/build/`. Select the
`examples` CMake preset to configure and build. Tests and benchmarks can be
turned on through CMake's `BUILD_TESTING` and `BUILD_BENCHMARKS` options.

Requires Bash, curl, and Docker Compose on the host. On Windows, open the
repository through WSL.

## Development (Cosmopolitan)

**Omnirefl development** opens the repository and includes a prebuilt x86_64
package. Tests, benchmarks, and extensions are enabled.

Build from the repository root:

```sh
cmake --build --preset cosmo --parallel
```

To create a package:

```sh
cpack --preset cosmo
```

The package is written to `artifacts/devcontainer/omnirefl.tar.gz`. The image's
prebuilt x86_64 package is at `/opt/omnirefl-packages/omnirefl.tar.gz`.
To build a universal package from inside the development container:

```sh
COSMO_BUILD_UNIVERSAL=true cpack --preset cosmo
```
