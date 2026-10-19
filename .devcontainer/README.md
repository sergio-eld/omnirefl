# Containers

On GitHub, click **Code** and select the **Codespaces** tab. Open its three-dot
menu and choose **New with options**. On the creation page, select the branch
and **Dev container configuration**, then click **Create codespace** to open
it in your browser.

In local VS Code, use **Dev Containers: Switch Container**. Separate Codespaces
have separate checkouts.

## Examples (Ubuntu 24.04)

- **Examples - latest release** (default): use the latest
  GitHub release.
- **Examples - current source**: use this checkout, including
  local changes. After changing the library, rebuild the container from the
  command palette: **Codespaces: Rebuild Container** in the browser, or
  **Dev Containers: Rebuild Container** locally. This rebuilds and installs the
  package. Changes to example files only need a normal build.

The Explorer opens `extras/`, and the terminal opens `extras/build/`. Startup
configures the examples; select the `examples` preset to build. To also enable
tests and benchmarks, run in that terminal:

```sh
cmake .. -DBUILD_TESTING=ON -DBUILD_BENCHMARKS=ON
cmake --build . --parallel
ctest --output-on-failure
```

Local containers require Bash, curl, and Docker Compose. On Windows, open the
repository through WSL.

## Development (Cosmopolitan)

**Development** opens the repository and includes a prebuilt x86_64
package. Examples, tests, and benchmarks build directly from `extras/` in the
checkout; no extras archive is needed. Extensions are enabled.

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
