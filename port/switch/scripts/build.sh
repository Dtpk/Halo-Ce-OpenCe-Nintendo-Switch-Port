#!/usr/bin/env bash
# ==============================================================================
# Native Build Script for Halo CE (Nintendo Switch Port)
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Source devkitPro environment if available
if [ -f /etc/profile.d/devkit-env.sh ]; then
    source /etc/profile.d/devkit-env.sh
fi

if [ -z "${DEVKITPRO}" ]; then
    if [ -d /opt/devkitpro ]; then
        export DEVKITPRO=/opt/devkitpro
    else
        echo "Error: DEVKITPRO environment variable is not set and /opt/devkitpro was not found." >&2
        echo "Please install devkitPro and export DEVKITPRO=<path to>/devkitpro" >&2
        exit 1
    fi
fi

cd "${ROOT_DIR}"
echo "Building Halo: Combat Evolved for Nintendo Switch..."
make -j"$(nproc 2>/dev/null || echo 2)" "$@"
echo "Build complete! Output: ${ROOT_DIR}/halo.nro"
