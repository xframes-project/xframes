#!/usr/bin/env bash
# Build the WASM target using the emscripten/emsdk Docker image.
# Run from anywhere: ./packages/dear-imgui/cpp/wasm/build-wasm-docker.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

# Convert to Windows path if running under MSYS/Git Bash (needed for Docker volume mounts)
if command -v cygpath &>/dev/null; then
  REPO_ROOT="$(cygpath -w "$REPO_ROOT")"
fi

IMAGE_NAME="xframes-emsdk"
CMAKE_EXTRA=""

if [ "${1:-}" = "--fast" ]; then
  CMAKE_EXTRA="-DXFRAMES_FAST_BUILD=ON"
  echo "Fast build: using -O0 (dev mode)"
fi

docker build -t "${IMAGE_NAME}" -f "$SCRIPT_DIR/Dockerfile.wasm" "$SCRIPT_DIR"
docker run --rm \
  -v "$REPO_ROOT":/src \
  -v xframes-ccache:/ccache \
  "${IMAGE_NAME}" \
  bash -c "cd /src/packages/dear-imgui/cpp/wasm && \
    cmake -S . -B build-wasm -GNinja $CMAKE_EXTRA && \
    cmake --build ./build-wasm --target xframes"
