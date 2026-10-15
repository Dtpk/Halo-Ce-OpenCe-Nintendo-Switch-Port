#!/usr/bin/env bash
# ==============================================================================
# Docker Build Script for Halo CE (Nintendo Switch Port)
# Uses official devkitPro devkitA64 container image
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "Building Halo: Combat Evolved using devkitpro/devkita64 Docker image..."
docker run --rm \
    -u "$(id -u):$(id -g)" \
    -v "${ROOT_DIR}":/work \
    -w /work \
    devkitpro/devkita64 \
    make -j"$(nproc 2>/dev/null || echo 2)" "$@"

echo "Build complete! Output: ${ROOT_DIR}/halo.nro"
