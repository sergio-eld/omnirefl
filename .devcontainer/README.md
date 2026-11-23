# Containers

On GitHub, click **Code** and select the **Codespaces** tab. Open its three-dot
menu and choose **New with options**. On the creation page, select the branch
and **Dev container configuration**, then click **Create codespace** to open
it in your browser.

In local VS Code, use **Dev Containers: Switch Container**. Separate Codespaces
have separate checkouts.

## Examples (Ubuntu 24.04)

- **Examples - v0.0.8** (default): use the matching GitHub release. If its
  package is not published yet, retry container creation after publication.
- **Examples - current source**: use this checkout, including
  local changes. After changing the library, rebuild the container from the
  command palette: **Codespaces: Rebuild Container** in the browser, or
  **Dev Containers: Rebuild Container** locally. This rebuilds and installs the
  package. Changes to example files only need a normal build.

Source-build configurations request at least 16 GB RAM for compiling Omnirefl.

Fresh `master` may contain API changes absent from the published release,
causing examples in the release-based container to fail to compile. The
**Examples - current source** and **Development** configurations build the
package from the same checkout, avoiding that version mismatch.

The Explorer opens `extras/`, and the terminal opens `extras/build/`. Startup
seeds the reflection and JSON sandboxes without overwriting edits; Codespaces
opens both. The JSON sandbox defaults to C++20; changing its
`CMAKE_CXX_STANDARD` to 11 selects the guarded compatibility examples. Select
the `examples` preset to build. To also enable tests and benchmarks, run in
that terminal:

```sh
cmake .. -DBUILD_TESTING=ON -DBUILD_BENCHMARKS=ON
cmake --build . --parallel
ctest --output-on-failure
```

Local containers require Bash, curl, and Docker Compose. On Windows, open the
repository through WSL.

## Recovery mode

Package preparation saves its output to `artifacts/devcontainer/prepare.log`
in the checkout, including when Codespaces falls back to recovery mode.

Source builds need a machine with at least 16 GB RAM. For an existing Codespace,
use **Codespaces: Change Machine Type**, then stop and restart it before
rebuilding. See [changing the machine type](https://docs.github.com/en/codespaces/customizing-your-codespace/changing-the-machine-type-for-your-codespace).

To build from source while a release is unpublished, open
`.devcontainer/devcontainer.json` and change the final `initializeCommand`
argument from `"v0.0.8"` to `"source"`. This builds Omnirefl in its Alpine
toolchain container. Then run **Codespaces: Rebuild Container** (or
**Dev Containers: Rebuild Container** locally).

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
