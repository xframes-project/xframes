# XFrames WebAssembly build

This directory builds the Dear ImGui/ImPlot XFrames runtime for Emscripten and WebGPU. React/Fabric packaging and browser verification live under `packages/dear-imgui/npm`.

See [React Native Fabric embedding](../../npm/FABRIC_EMBEDDING.md) for the supported React Native version, JavaScript workspace, full verification matrix, and known gaps.

## Docker build (recommended)

Docker is the supported reproducible build path. From the repository root in Git Bash, macOS, or Linux:

```bash
./packages/dear-imgui/cpp/wasm/build-wasm-docker.sh
```

For a faster development build using `-O0`:

```bash
./packages/dear-imgui/cpp/wasm/build-wasm-docker.sh --fast
```

The script builds `Dockerfile.wasm`, currently pinned to Emscripten 5.0.2, mounts the repository at `/src`, and reuses the `xframes-ccache` Docker volume. The first vcpkg build is slow; later builds reuse cached dependencies.

Outputs are written directly to:

- `packages/dear-imgui/npm/wasm/src/lib/xframes.mjs`
- `packages/dear-imgui/npm/wasm/src/lib/xframes.data`

## Browser verification

Install the shared npm workspace once, then run the browser smoke:

```powershell
cd packages/dear-imgui/npm
npm ci
cd wasm
npm run smoke:browser
```

The test compiles the current common/Fabric package, serves the full `<App>`, launches Edge or Chrome with WebGPU, fails on browser/runtime exceptions or timeout, waits for the native `ready` callback, and writes `build/browser-smoke.png`.

The default SwiftShader flags are for trusted local/CI content. Set `XFRAMES_WEBGPU_ADAPTER=default` to use the machine's normal adapter. A restricted process sandbox may prevent Chromium's GPU subprocess from starting; the smoke needs normal driver and temporary-profile access.

## Manual and dev-container builds

Manual Emscripten and dev-container builds remain possible, but they are not the release verification path. Keep them aligned with `Dockerfile.wasm` and use the same CMake target:

```bash
cd packages/dear-imgui/cpp/wasm
cmake -S . -B build-wasm -GNinja
cmake --build ./build-wasm --target xframes
```

On Windows, only Visual Studio 2022 is supported for native tooling. Do not reinstall React inside this C++ directory; all JavaScript dependencies are owned by the npm workspace lockfile.

## Native unit tests

The existing Google Test suites remain under the C++ test directories. On Windows, use an x64 Visual Studio 2022 Developer Command Prompt and the repository's test scripts or configure/build the relevant test CMake target. On Linux, configure the test directory with CMake and run the produced test binary.
