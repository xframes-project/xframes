# React Native Fabric embedding

- Status: implemented and verified
- Last verified: 28 August 2026
- React Native: 0.87.0
- React: 19.2.3
- Scheduler: 0.27.0

## Purpose

XFrames uses React Native's battle-tested Fabric reconciler without shipping the React Native runtime to XFrames applications. A generated snapshot of the renderer and the small upstream helpers it needs are compiled into `@xframes/common`. The same host adapter drives the Node/OpenGL and browser/WebGPU native runtimes.

This document is the authoritative maintenance and verification guide for that embedding. The separate [Fabric runtime hardening design](../../../docs/architecture/fabric-runtime-hardening.md) covers future commit batching, cleanup, replay, invalidation, instrumentation, and automation work.

## Supported toolchain

- Node.js must satisfy React Native 0.87's engine range: `^22.13.0 || ^24.3.0 || >=26.0.0`.
- The workspace is verified with Node.js 24.14.0.
- React and React DOM are pinned to 19.2.3 at the workspace root so every package resolves one physical React instance.
- Visual Studio 17/2022 is the only supported Windows compiler for the Node native addon.
- The browser native module is built in Docker with Emscripten 5.0.2.
- macOS, Safari, and Firefox have not been verified by this upgrade.

The Fabric upgrade changes JavaScript/TypeScript integration only. It does not change native C++ sources. The existing Visual Studio 2022 Node addon was exercised, and the existing C++ source was rebuilt to WebAssembly in Docker.

## Architecture

```text
App JSX
   |
   v
embedded ReactFabric dev/prod renderer
   |
   v
ReactNativePrivateInterface + nativeFabricUIManager
   |
   +----------------------------+
   |                            |
   v                            v
Node N-API/OpenGL          Emscripten/WebGPU
   |                            |
   +------------+---------------+
                v
       retained XFrames tree
       Yoga + Dear ImGui
```

`process.env.NODE_ENV` selects the embedded renderer:

- `production` selects `ReactFabric-prod.js`.
- Any other value selects `ReactFabric-dev.js`.

`ReactFabricProdInitialiser` remains exported for compatibility. New code uses `ReactFabricInitialiser` so development checks are not silently bypassed.

React Native's `render` export now accepts a fifth `options` argument. Node and Wasm pass `undefined` explicitly.

## Generated source boundary

The generator is [`common/scripts/extract-rn-fabric-renderer.ts`](common/scripts/extract-rn-fabric-renderer.ts). It resolves React Native relative to `@xframes/common`, so it is independent of the caller's current directory.

It embeds five files:

| Upstream React Native source                                                     | Generated XFrames file           |
| -------------------------------------------------------------------------------- | -------------------------------- |
| `Libraries/Renderer/implementations/ReactFabric-dev.js`                          | `ReactFabric-dev.js`             |
| `Libraries/Renderer/implementations/ReactFabric-prod.js`                         | `ReactFabric-prod.js`            |
| `Libraries/ReactNative/ReactFabricPublicInstance/ReactNativeAttributePayload.js` | `ReactNativeAttributePayload.js` |
| `Libraries/StyleSheet/flattenStyle.js`                                           | `flattenStyle.js`                |
| `Libraries/Utilities/differ/deepDiffer.js`                                       | `deepDiffer.js`                  |

Generated files are committed because Node and browser consumers must not install or resolve React Native at runtime. Each file has a banner containing the React Native version, exact upstream path, source SHA-256, and generator path, followed by the validated upstream Meta/MIT notice. Never edit a generated file by hand.

The generator uses Babel's Hermes parser and AST visitors rather than textual replacement. It:

- strips Flow syntax from upstream helpers;
- replaces only the four expected renderer imports;
- rejects any new or unknown renderer dependency;
- validates that every expected import occurs exactly once;
- rewrites CommonJS renderer exports into the initializer result;
- requires the public `render` and `stopSurface` exports;
- rejects unresolved `require()` and `exports.*` expressions;
- formats deterministic output; and
- supports non-mutating `--check` verification.

Missing upstream files and changed contracts fail loudly. A React Native upgrade cannot silently retain an old renderer snapshot.

## Host contract added for React Native 0.87

React Native 0.87 moved prop payload work behind `createAttributePayload` and `diffAttributePayloads`. The XFrames private interface now uses the exact matching upstream implementations rather than object-shaped placeholders. It also exposes:

- the matching `flattenStyle` and `deepDiffer` helpers;
- `createPublicRootInstance`;
- callable text/public-instance fallbacks;
- a minimal callable `UIManager`;
- discrete, continuous, and idle event-priority constants;
- `unstable_getCurrentEventPriority`;
- no-op command, accessibility, and responder methods required by the current renderer.

The host-contract test covers payload creation and diffing, style flattening, the root public instance, and event priority. These tests validate the integration seam; they do not claim to test React Native's renderer internals.

## Upgrade findings

Official npm package contents were inspected at representative compatibility boundaries:

| React Native | Finding                                                                                                                                                                                                                     |
| ------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 0.74.7       | Same four renderer imports as the old 0.74 embedding. The former exact-string extractor happened to match this source shape.                                                                                                |
| 0.76.9       | The renderer's initial variable declaration changed, breaking the old string replacement even though the conceptual host contract remained close. This is why extraction is now syntax-aware.                               |
| 0.78.3       | The renderer moves to React 19, requiring React and JSX type upgrades together.                                                                                                                                             |
| 0.81.6       | The host contract adds `createPublicRootInstance`.                                                                                                                                                                          |
| 0.87.0       | Current target. Prop payloads delegate to upstream attribute-payload helpers, continuous/idle event priorities are required, and `render` has an options argument. Public `render` and `stopSurface` exports remain usable. |

The conclusion is favorable: the embedding still works on the current stable renderer, but the old textual extraction method was already broken by 0.76 and its placeholder host methods would not satisfy 0.87. The new generator converts source-shape drift into explicit verification failures.

React Native 0.87 was the latest stable version when this work was verified. Consult the official [React Native releases overview](https://reactnative.dev/versions) and the [`react-native` npm package](https://www.npmjs.com/package/react-native) before a future upgrade.

## Workspace and lockfile

Run npm commands from `packages/dear-imgui/npm` unless a section says otherwise:

```powershell
cd packages/dear-imgui/npm
npm ci
```

`packages/dear-imgui/npm/package-lock.json` is the only authoritative JavaScript lockfile for `common`, `node`, and `wasm`. Do not recreate per-package `package-lock.json` or `yarn.lock` files.

This workspace layout is correctness-critical, not cosmetic. A second React installation makes application hooks use a different dispatcher from the embedded renderer and causes invalid-hook-call failures. Verify resolution after dependency changes:

```powershell
npm ls react react-dom react-native scheduler --all
```

The expected result has one deduplicated React 19.2.3 instance.

### Coordinated package boundary

This upgrade is a coordinated package release because the Node and Wasm wrappers now import `ReactFabricInitialiser`, which older `@xframes/common` releases do not export:

| Package           | Version in this source | Required peer boundary |
| ----------------- | ---------------------- | ---------------------- |
| `@xframes/common` | `0.1.7`                | React `^19.2.3`        |
| `@xframes/node`   | `0.1.14`               | common `^0.1.7`        |
| `@xframes/wasm`   | `0.1.9`                | common `^0.1.7`        |

Publish common first, then Node and Wasm. The versions immediately preceding these (`0.1.6`, `0.1.13`, and `0.1.8`) are already present in the npm registry with the old React 18 contract. Do not widen the common peer range back to `^0.1.0`: that would permit npm to install an API-incompatible renderer host and defer the failure to runtime.

## Fabric maintenance commands

From `packages/dear-imgui/npm`:

```powershell
# Regenerate committed files after deliberately changing react-native.
npm run fabric:generate

# Snapshot, strict generator typecheck, and host-contract tests.
npm run fabric:verify

# Build all published package surfaces.
npm run build:common
npm run build:node
npm run build:wasm
```

`build:common` runs `fabric:verify` before bundling, so a stale or structurally incompatible renderer cannot be published accidentally.

Upgrade procedure:

1. Change `react-native`, `react`, `react-dom`, `scheduler`, and React type versions together where their peer ranges require it.
2. Run `npm install` at the npm workspace root and inspect the lockfile.
3. Run `npm run fabric:generate`.
4. Review generated-source hashes and the generator diff. Do not weaken a failed invariant merely to make generation pass.
5. Update the private host interface for any real new renderer dependency or callable contract.
6. Run `npm run fabric:verify` and all package builds.
7. Run both Node renderer smokes and the Docker/Wasm/browser path below.
8. Advance all three package versions together, preserve the common peer boundary, and publish common first.
9. Update the compatibility table and verified versions in this document.

## Node full-App verification

The smoke harness renders the hook-heavy dashboard and captures the native OpenGL framebuffer:

```powershell
cd packages/dear-imgui/npm/node

# Development renderer
npm run smoke:app

# Production renderer
$env:NODE_ENV = "production"
npm run smoke:app
Remove-Item Env:NODE_ENV
```

Output is `node/build/app-smoke.png`. Both modes must exit zero, and the image must show the populated dashboard rather than only an empty window. The smoke uses the existing native addon; rebuild that addon only when C++ changes, using Visual Studio 2022.

## Docker and Wasm verification

From the repository root in Git Bash, macOS, or Linux:

```bash
./packages/dear-imgui/cpp/wasm/build-wasm-docker.sh --fast
```

The first build downloads and compiles vcpkg dependencies. Later builds reuse Docker and ccache volumes. Output is written directly to:

- `packages/dear-imgui/npm/wasm/src/lib/xframes.mjs`
- `packages/dear-imgui/npm/wasm/src/lib/xframes.data`

Then run the browser proof from PowerShell:

```powershell
cd packages/dear-imgui/npm/wasm
npm run smoke:browser
```

The smoke starts webpack-dev-server when needed, launches Edge or Chrome headlessly, enables WebGPU with Chromium's SwiftShader test configuration, listens for console and protocol exceptions, waits for the XFrames `ready` callback, and writes `wasm/build/browser-smoke.png`. Set `XFRAMES_BROWSER` to an Edge or Chrome executable to override discovery.

SwiftShader flags lower Chromium's security guarantees and are suitable only for trusted local/CI content. To use the machine's normal WebGPU adapter instead:

```powershell
$env:XFRAMES_WEBGPU_ADAPTER = "default"
npm run smoke:browser
Remove-Item Env:XFRAMES_WEBGPU_ADAPTER
```

The current software-adapter flags come from Chromium's official [WebGPU test configuration](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/third_party/blink/web_tests/FlagSpecificConfig). A process or filesystem sandbox that denies the browser GPU subprocess access will make Chromium fail before XFrames loads; run the browser smoke in an environment with normal graphics-driver and temporary-profile access.

## Verified result on 28 August 2026

| Gate                                          | Result                                                 |
| --------------------------------------------- | ------------------------------------------------------ |
| Deterministic generated snapshot check        | Pass                                                   |
| Strict extractor/tool TypeScript check        | Pass                                                   |
| Fabric host-contract test                     | Pass                                                   |
| Single React instance check                   | Pass                                                   |
| `@xframes/common` CJS and declarations        | Pass                                                   |
| `@xframes/node` TypeScript/package build      | Pass                                                   |
| `@xframes/wasm` CJS and declarations          | Pass                                                   |
| npm dry-run tarballs for all three packages   | Pass; coordinated versions and peer boundary inspected |
| Full Node App, development renderer           | Pass; screenshot inspected                             |
| Full Node App, production renderer            | Pass; screenshot inspected                             |
| Docker/Emscripten 5.0.2 native rebuild        | Pass                                                   |
| Webpack development bundle                    | Pass                                                   |
| Docker-built Wasm in headless WebGPU Chromium | Pass; `ready`, no runtime errors, screenshot inspected |

## Known gaps and deliberate non-hacks

- Fabric implementation files are private React Native internals. Hashes and AST invariants detect source drift, but runtime smokes are still mandatory after every upgrade.
- Both development and production renderers are currently present in the `@xframes/common` CJS artifact (about 1.11 MB uncompressed). Runtime selection is correct, but CJS consumers may not eliminate the unused variant. Separate production/development entry points are a future size optimization, not a correctness blocker.
- The existing Windows `@xframes/node` package rule copies and publishes all `build/Release/*.*` files. A dry-run tarball therefore includes build-only Janet bootstrap, import-library, and export files in addition to `xframes.node` and its runtime DLLs. Narrow that allowlist only after auditing every required runtime DLL; it is package-size/release hygiene debt, not a Fabric runtime blocker.
- The browser smoke proves initialization, absence of reported runtime errors, and a real screenshot. It does not yet perform semantic assertions for every imperative widget animation; the first-viewport live-plot trace is therefore a visual coverage gap. Do not turn screenshot presence alone into a claim of full per-widget behavior.
- The repository-wide common ESLint command has a pre-existing backlog (356 findings at verification time). The new extraction scripts lint cleanly, but this upgrade does not hide or mass-rewrite unrelated legacy findings.
- An online npm install reported 70 dependency advisories in the legacy development dependency graph. No uncontrolled `npm audit fix --force` was applied. Production exposure and dependency-toolchain modernization need a separate audit.
- macOS hardware and Safari are untested. The technologies are portable, but portability is not a substitute for a real platform build and runtime test.
- The future atomic-commit, destruction, replay, invalidation, and automation work remains intentionally separate in the [runtime hardening design](../../../docs/architecture/fabric-runtime-hardening.md). No reconciler patch or speculative C++ rewrite was introduced during this version upgrade.
