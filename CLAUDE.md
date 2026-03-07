# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

XFrames is a DOM-free GUI framework that renders native desktop and browser UIs using Dear ImGui. Developers write React/TypeScript components that drive an ImGui widget tree through a custom React Native Fabric renderer. The C++ core is exposed via Node-API (NAPI v9) for desktop and WebAssembly (Emscripten/WebGPU) for browsers.

## Build Commands

### Node.js Native Addon (desktop)
```bash
cd packages/dear-imgui/npm/node
npm run cpp:compile         # cmake-js compile + copy .node to src/lib
npm run build:library       # tsc + copy artifacts to dist
npm run start               # tsx ./src/index.tsx (runs demo)
```

### WASM Module (browser)
```bash
# Requires emsdk installed and sourced
cd packages/dear-imgui/cpp/wasm
cmake -S . -B build -GNinja
cmake --build ./build --target xframes
```

### Unit Tests (C++ / Google Test)
```bash
# Linux
cd packages/dear-imgui/cpp/tests
cmake -S . -B build && cd build && make && ./Google_Tests_run

# Windows
cd packages/dear-imgui/cpp/tests
cmake -S . -B build
cmake --build ./build --target Google_tests_run
build/Debug/Google_Tests_run.exe
```

### Quick Start (end user)
```bash
npx create-xframes-node-app
cd <project-name>
npm start
```

## Architecture

### C++ Core (`packages/dear-imgui/cpp/app/`)

The shared C++ library is not a standalone CMake target — it is compiled as source files included by each build target (Node addon, WASM, tests).

**Class hierarchy:**
```
Element → Widget → StyledWidget → (Button, Checkbox, InputText, Table, Window, ...)
```

**Key classes:**
- **`XFrames`** (`xframes.h/.cpp`) — Main orchestrator. Owns element registry (`unordered_map<int, unique_ptr<Element>>`), element hierarchy, and the RPP reactive subject for queuing UI operations (`CreateElement`, `PatchElement`, `SetChildren`, `AppendChild`). Drives the render loop.
- **`ImGuiRenderer`** (`imgui_renderer.h/.cpp`) — Initializes GLFW window + OpenGL3 (desktop) or WebGPU (WASM) backend. Manages font loading and the main render loop.
- **`LayoutNode`** (`element/layout_node.h/.cpp`) — Wraps a Yoga `YGNodeRef`. Translates JSON style definitions into Yoga flexbox API calls.

**Key patterns:**
- **Reactive event queue**: JS-thread operations are pushed onto an RPP `serialized_replay_subject<ElementOpDef>` and consumed on the render thread for thread safety.
- **JSON as API contract**: All element definitions, patches, styles, and font configs cross the C++/JS boundary as JSON strings (nlohmann/json).
- **Per-state styling**: Widgets support base/hover/active/disabled style variants for both Yoga layout and ImGui color/stylevar overrides.

### TypeScript/JavaScript Layer (`packages/dear-imgui/npm/`)

Uses a **vendored React Native Fabric renderer** (`ReactFabric-prod.js`) — not React DOM. The custom `nativeFabricUiManager` intercepts `createNode`, `appendChild`, etc. and calls the native module (NAPI or WASM).

**Packages:**
- **`@xframes/common`** (`npm/common/`) — Platform-agnostic React components, types, stylesheet system (`RWStyleSheet`, `XFramesStyle`, `YogaStyle`), `WidgetRegistrationService`, hooks
- **`@xframes/node`** (`npm/node/`) — Node.js native addon wrapper + render entry point
- **`@xframes/wasm`** (`npm/wasm/`) — WASM wrapper for browser (Webpack bundled)

### Native Bindings

- **Node.js** (`npm/node/src/xframes-node.cpp`): Uses `node-addon-api` (NAPI v9). `Napi::ThreadSafeFunction` for C++ → JS callbacks.
- **WASM** (`cpp/wasm/src/main.cpp`): Uses Emscripten `embind`. `EM_ASM_ARGS` for C++ → JS callbacks via `Module.eventHandlers`.

## Dependencies

- **vcpkg** (submodule at `cpp/deps/vcpkg`) manages most C++ deps. Per-target `vcpkg.json` files in `cpp/app/`, `cpp/wasm/`, `cpp/tests/`.
- **Git submodules** (15 total in `cpp/deps/`): imgui, implot, glfw, yoga, ReactivePlusPlus, googletest, nlohmann/json, stb, IconFontCppHeaders, css-color-parser-cpp, node-addon-api, node-api-headers, and others.
- **C++ standard**: C++23 (tests/addon), C++20 (Python binding).
- **cmake-js** compiles the Node.js native addon from npm scripts.

## Local Development

`npm run start` in `npm/node/` auto-rebuilds `@xframes/common` from source via `prestart` (tsup build + cpy copy into `node_modules`). No manual patching needed when editing common's source files.

**Important:** Do not use esbuild directly to bundle `@xframes/common` on Windows — it truncates output at 128KB. Use tsup (which works correctly with `splitting: false`).

## Table Widget

The Table widget supports sorting, per-column filtering, row selection, column reordering, and column visibility toggles.

**Table-level boolean props:** `filterable`, `reorderable`, `hideable`

**Per-column config:** Each column definition has `heading`, `fieldId`, and optional `type` (`"number"`, `"boolean"`). Boolean fields on column definitions (`defaultHide`, `defaultSort`, `widthFixed`, `noSort`, `noResize`, `noReorder`, `noHide`) map to `ImGuiTableColumnFlags_*` and are parsed in `Table::extractColumns()`.

**Type-aware filtering:** Boolean columns render a Combo dropdown (All/Yes/No) instead of a text filter. Number columns filter against displayed values (formatted integers/decimals). String columns use `ImGuiTextFilter`.

**Flag logic:** When `hideable` is set on the table, columns are hideable by default (the `NoHide` flag is dropped). Use `noHide: true` on individual columns to pin them. When `hideable` is not set, all columns get `NoHide` automatically (original behavior).

**Event callbacks:** `onSort`, `onFilter`, `onRowClick` — each follows the same pipeline: C++ Render() → XFrames callback → NAPI TSFN / WASM EM_ASM → JS `dispatchEvent`. The `init()` function takes 13 arguments (indices 0–12).

## PlotBar Widget

Wraps `ImPlot::PlotBars`. Same data model as PlotLine: parallel `double` vectors for X (positions) and Y (heights). Props: `axisAutoFit`, `dataPointsLimit`. Internal ops: `setData` (array of `{x, y}`), `appendData` (single `x, y`), `resetData`, `setAxesAutoFit`. The `setData` op accepts a JSON array of `{x, y}` objects. Existing `appendDataToPlotLine` and `resetPlotData` on `WidgetRegistrationService` can be reused since they send generic ops by widget ID; `setPlotBarData` is the dedicated batch setter.

## PlotScatter Widget

Wraps `ImPlot::PlotScatter`. Identical data model and API to PlotBar — parallel `double` vectors for X/Y, same props (`axisAutoFit`, `dataPointsLimit`), same internal ops (`setData`, `appendData`, `resetData`, `setAxesAutoFit`). `setPlotScatterData` is the dedicated batch setter on `WidgetRegistrationService`.

## PlotHeatmap Widget

Wraps `ImPlot::PlotHeatmap`. Different data model from other plots: flat 1D array of `rows × cols` doubles (row-major). Props: `axisAutoFit`, `scaleMin`, `scaleMax` (0/0 = auto-scale), `colormap` (ImPlotColormap enum, default `Viridis`). Internal ops: `setData` (with `rows`, `cols`, `values` array), `resetData`, `setAxesAutoFit`. No `appendData` — heatmaps are batch-only. `setPlotHeatmapData(id, rows, cols, values)` is the dedicated setter on `WidgetRegistrationService`. Bounds are set to `(0,0)-(cols,rows)` so cells map 1:1 to plot coordinates.

## ColorIndicator Widget

Renders a filled colored shape via `ImDrawList`. Props: `color` (CSS color string, parsed via `extractColor`), `shape` (optional `"rect"` | `"circle"`, default `"rect"`). Size is controlled entirely by Yoga layout style props (`width`, `height`). Uses `ImGui::Dummy` after drawing to advance the cursor. No events, no imperative handle.

## ProgressBar Widget

Wraps `ImGui::ProgressBar`. Simple props-only widget (no imperative handle). Props: `fraction` (0.0–1.0), `overlay` (optional text displayed on top of the bar, e.g. "75%"). Uses Yoga layout width; height is auto-measured from font size + frame padding.

## InputText Widget

Uses `imgui_stdlib.h` for `std::string`-based input — no buffer size limits or manual buffer management. Supports optional props: `multiline` (renders `InputTextMultiline`), `password` (`ImGuiInputTextFlags_Password`), `readOnly` (`ImGuiInputTextFlags_ReadOnly`), `numericOnly` (`ImGuiInputTextFlags_CharsDecimal`). Flags are per-instance (not static) to avoid sharing state across widget instances.

## Layout & Scroll

`Element::Render()` calls `ImGui::BeginChild("##", size, ImGuiChildFlags_None)` for each element. Yoga computes layout once per frame from the root node.

**Scroll containers:** Set `overflow: "scroll"` on a Node's style. Children must use `flexShrink: 0` with fixed `height` so Yoga doesn't shrink them to fit — when children overflow the viewport, ImGui shows scrollbars automatically. Do **not** call `YGNodeCalculateLayout` a second time during render; it fights the root layout pass and causes flicker.

## C++ Gotchas

- MSVC does not allow default member initializers in unnamed structs used with `using` typedefs. Use named `struct Foo { ... };` instead of `using Foo = struct { ... };` when fields have defaults.

## Platform Notes

- Desktop rendering: GLFW + OpenGL 3
- WASM rendering: WebGPU only (Chrome, Edge, Firefox Nightly)
- WSL2 requires `export GALLIUM_DRIVER=d3d12`
- WASM output is a single `.mjs` file with embedded WASM (`-s SINGLE_FILE=1`)
