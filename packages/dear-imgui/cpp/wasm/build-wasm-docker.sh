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

docker build -t "${IMAGE_NAME}" -f "$SCRIPT_DIR/Dockerfile.wasm" "$SCRIPT_DIR"
docker run --rm \
  -v "$REPO_ROOT":/src \
  "${IMAGE_NAME}" \
  bash -c "cd /src/packages/dear-imgui/cpp/wasm && \
    cmake -S . -B build-wasm -GNinja && \
    cmake --build ./build-wasm --target xframes"
