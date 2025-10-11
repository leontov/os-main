#!/usr/bin/env bash
set -euo pipefail

# scripts/release.sh - prepare release tarball and optional Docker image

root="$(cd "$(dirname "$0")/.." && pwd)"
tag="${1:-local}"
outdir="$root/dist/release"
mkdir -p "$outdir"

echo "Creating release package for: $tag"
tar -czf "$outdir/kolibri_release_${tag}.tar.gz" -C "$root/build" kolibri_node libkolibri_core.a wasm || true
tar -rzf "$outdir/kolibri_release_${tag}.tar.gz" -C "$root/frontend/dist" . || true

echo "Package created: $outdir/kolibri_release_${tag}.tar.gz"

if [[ "${2:-}" == "--docker" ]]; then
  echo "Building Docker image kolibri:$tag"
  docker build -f Dockerfile.release -t kolibri:$tag .
  echo "Docker image built: kolibri:$tag"
fi
