#!/usr/bin/env bash
set -euo pipefail

readonly package_source=${1:-latest_release}

if [[ latest_release != "$package_source" && source != "$package_source" ]]; then
  printf 'Unsupported package source: %s\n' "$package_source" >&2
  exit 1
fi

cd "$(dirname "$0")/../.."
readonly package_dir=artifacts/devcontainer
mkdir -p "$package_dir"
trap 'rm -f "$package_dir/omnirefl.tar.gz.tmp"' EXIT

case "$package_source" in
  latest_release)
    # Detached symbol archives are separate release assets.
    curl -fL -o "$package_dir/omnirefl.tar.gz.tmp" "$(curl -fsSL \
        https://api.github.com/repos/sergio-eld/omnirefl/releases/latest \
        | sed -n \
          -e '/-symbols\.tar\.gz/d' \
          -e 's/.*"browser_download_url": "\(.*cosmo-universal.*\.tar\.gz\)".*/\1/p')"
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
