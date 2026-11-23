#!/usr/bin/env bash
set -euo pipefail

# FIXME: [high] Fresh master can change the examples' API before a matching
# package is released, breaking Codespaces despite this pin. Keep the package
# and checkout compatible when selecting the default container recipe.
readonly package_source=${1:-v0.0.8}

if [[ source != "$package_source" \
  && ! "$package_source" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  printf 'Unsupported package source: %s\n' "$package_source" >&2
  exit 1
fi

cd "$(dirname "$0")/../.."
readonly package_dir=artifacts/devcontainer
mkdir -p "$package_dir"
exec > >(tee "$package_dir/prepare.log") 2>&1
trap 'rm -f "$package_dir/omnirefl.tar.gz.tmp"' EXIT

case "$package_source" in
  v*)
    # Detached symbol archives are separate release assets.
    if ! package_url=$(curl -fsSL \
        "https://api.github.com/repos/sergio-eld/omnirefl/releases/tags/$package_source" \
        | sed -n \
          -e '/-symbols\.tar\.gz/d' \
          -e 's/.*"browser_download_url": "\(.*cosmo-universal.*\.tar\.gz\)".*/\1/p') \
      || [[ -z "$package_url" ]]; then
      cat >&2 <<EOF

Cannot fetch the public Omnirefl $package_source package.
The release may not be published yet. Retry after publication, or build locally:

1. Open .devcontainer/devcontainer.json.
2. Change the final initializeCommand argument to "source".
3. Run "Codespaces: Rebuild Container".
   Locally, run "Dev Containers: Rebuild Container".

The source option builds Omnirefl in its Alpine toolchain container.
Preparation log (also available in recovery mode): $package_dir/prepare.log
EOF
      exit 1
    fi
    readonly package_url
    curl -fL -o "$package_dir/omnirefl.tar.gz.tmp" "$package_url"
    ;;
  source)
    export COMPOSE_FILE=.devcontainer/compose.yml
    docker compose build development
    docker compose run --rm --no-deps --user "$(id -u):$(id -g)" \
      --workdir /workspaces/omnirefl development \
      cp /opt/omnirefl-packages/omnirefl.tar.gz "$package_dir/omnirefl.tar.gz.tmp"
    ;;
esac

mv "$package_dir/omnirefl.tar.gz.tmp" "$package_dir/omnirefl.tar.gz"
