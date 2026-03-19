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
- **WASM** (`cpp/wasm/src/main.cpp`): Uses Emscripten `embind`. `EM_ASM` for C++ → JS callbacks via `Module.eventHandlers`.

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

**Event callbacks:** `onSort`, `onFilter`, `onRowClick`, `onItemAction`, `onPrefetchProgress` — each follows the same pipeline: C++ Render() → XFrames callback → NAPI TSFN / WASM EM_ASM → JS `dispatchEvent`. The `init()` function takes 16 arguments (indices 0–15).

**Context menu:** Set `contextMenuItems` prop (array of `{ id, label }`) to show a right-click context menu on table rows. `onItemAction` fires with `{ rowIndex, actionId }` when a menu item is clicked.

## PlotBar Widget

Wraps `ImPlot::PlotBars`. Same data model as PlotLine: parallel `double` vectors for X (positions) and Y (heights). Props: `axisAutoFit`, `dataPointsLimit`. Internal ops: `setData` (array of `{x, y}`), `appendData` (single `x, y`), `resetData`, `setAxesAutoFit`. The `setData` op accepts a JSON array of `{x, y}` objects. Existing `appendDataToPlotLine` and `resetPlotData` on `WidgetRegistrationService` can be reused since they send generic ops by widget ID; `setPlotBarData` is the dedicated batch setter.

## PlotScatter Widget

Wraps `ImPlot::PlotScatter`. Identical data model and API to PlotBar — parallel `double` vectors for X/Y, same props (`axisAutoFit`, `dataPointsLimit`), same internal ops (`setData`, `appendData`, `resetData`, `setAxesAutoFit`). `setPlotScatterData` is the dedicated batch setter on `WidgetRegistrationService`.

## PlotHeatmap Widget

Wraps `ImPlot::PlotHeatmap`. Different data model from other plots: flat 1D array of `rows × cols` doubles (row-major). Props: `axisAutoFit`, `scaleMin`, `scaleMax` (0/0 = auto-scale), `colormap` (ImPlotColormap enum, default `Viridis`). Internal ops: `setData` (with `rows`, `cols`, `values` array), `resetData`, `setAxesAutoFit`. No `appendData` — heatmaps are batch-only. `setPlotHeatmapData(id, rows, cols, values)` is the dedicated setter on `WidgetRegistrationService`. Bounds are set to `(0,0)-(cols,rows)` so cells map 1:1 to plot coordinates.

## PlotHistogram Widget

Wraps `ImPlot::PlotHistogram`. Unlike PlotBar (X/Y pairs), histogram takes a single `std::vector<double>` of raw values and auto-bins them. Props: `axisAutoFit`, `dataPointsLimit`, `bins` (int, default `ImPlotBin_Sturges`; negative values select algorithm, positive values set explicit bin count). Internal ops: `setData` (flat array of doubles), `appendData` (single `value`), `resetData`, `setAxesAutoFit`. `setPlotHistogramData(id, values)` and `appendDataToPlotHistogram(id, value)` are the dedicated methods on `WidgetRegistrationService`.

## PlotPieChart Widget

Wraps `ImPlot::PlotPieChart`. Takes parallel label (`std::vector<std::string>`) and value (`std::vector<double>`) arrays, renders labeled slices with `ImPlotFlags_Equal` for square aspect ratio. Props: `labelFormat` (printf-style, default `"%.1f"`, `""` to hide), `angle0` (starting angle in degrees, default `90`), `normalize` (bool, forces full circle), `showLegend` (default `true`), `legendLocation`. Axes use `NoDecorations` with fixed limits `(0,1,0,1)`, center `(0.5,0.5)`, radius `0.4`. Internal ops: `setData` (array of `{label, value}` objects), `resetData`. Batch-only (no `appendData`). `setPlotPieChartData(id, data)` is the dedicated setter on `WidgetRegistrationService`.

## ColorIndicator Widget

Renders a filled colored shape via `ImDrawList`. Props: `color` (CSS color string, parsed via `extractColor`), `shape` (optional `"rect"` | `"circle"`, default `"rect"`). Size is controlled entirely by Yoga layout style props (`width`, `height`). Uses `ImGui::Dummy` after drawing to advance the cursor. No events, no imperative handle.

## ProgressBar Widget

Wraps `ImGui::ProgressBar`. Simple props-only widget (no imperative handle). Props: `fraction` (0.0–1.0), `overlay` (optional text displayed on top of the bar, e.g. "75%"). Uses Yoga layout width; height is auto-measured from font size + frame padding.

## InputText Widget

Uses `imgui_stdlib.h` for `std::string`-based input — no buffer size limits or manual buffer management. Supports optional props: `multiline` (renders `InputTextMultiline`), `password` (`ImGuiInputTextFlags_Password`), `readOnly` (`ImGuiInputTextFlags_ReadOnly`), `numericOnly` (`ImGuiInputTextFlags_CharsDecimal`). Flags are per-instance (not static) to avoid sharing state across widget instances.

## Layout & Scroll

`Element::Render()` calls `ImGui::BeginChild("##", size, ImGuiChildFlags_None)` for each element. Yoga computes layout once per frame from the root node.

**Scroll containers:** Set `overflow: "scroll"` on a Node's style. Children must use `flexShrink: 0` with fixed `height` so Yoga doesn't shrink them to fit — when children overflow the viewport, ImGui shows scrollbars automatically. Do **not** call `YGNodeCalculateLayout` a second time during render; it fights the root layout pass and causes flicker.

## Tab Widget

TabBar supports `reorderable` prop (`ImGuiTabBarFlags_Reorderable`). TabItem supports `closeable` prop (renders close button via `p_open` parameter). Close fires `onBooleanValueChange(id, false)` on the open→closed transition only (tracked via `wasOpen` flag). The tab stays closed C++-side until the element is removed.

**Known issue:** ImGui logs a `SetCursorPos extends window/parent boundaries` warning on first tab bar render. This is cosmetic — no crash, no visual glitch. The warning comes from `SetCursorPos(0, 25.f)` in `TabItem::Render()` positioning content below the tab headers. The same `SetCursorPos` pattern is used throughout the codebase without issue; the tab context triggers it for reasons not yet diagnosed.

## MapView Widget

Interactive slippy map rendering OpenStreetMap raster tiles via a tile-grid model. Each visible 256×256 tile is fetched individually, decoded, uploaded to the GPU, and rendered with `ImDrawList::AddImage()`.

**Tile-grid architecture:** `Render()` computes the visible tile range from center coordinates, zoom level, and viewport size. Each tile is positioned via fractional tile coordinates → screen pixels. Old-zoom tiles render as scaled placeholders while current-zoom tiles load. Zoom debouncing (150ms) prevents tile fetch storms during rapid scroll.

**Three-tier cache (desktop):** GPU textures (LRU, max 512 tiles) → `TileCache` (in-memory, 1024 entries) → `DiskTileCache` (filesystem, no TTL) → network (libcurl). WASM skips the memory and disk tiers — relies on browser HTTP cache and GPU LRU only.

**Platform split:**
- **Desktop:** `FetchMissingTiles()` spawns a detached `std::thread` that calls `fetchTile()` (blocking libcurl). Decoded PNG bytes go to `m_pendingTiles` (mutex-protected). Render thread uploads via `glGenTextures`/`glTexImage2D`. Post-render eviction prunes non-nearby tiles via `glDeleteTextures`.
- **WASM:** `FetchMissingTiles()` calls `fetchTile()` directly (async `emscripten_fetch`, callback fires on main thread). Render thread uploads via `ImGuiRenderer::LoadTexture()` (WebGPU: `wgpuDeviceCreateTexture` + `wgpuQueueWriteTexture`). Post-render eviction is **disabled** on WASM — `wgpuTextureViewRelease()` immediately invalidates handles still pending in ImGui's draw list, unlike OpenGL which defers deletion.

**Props:** `tileUrlTemplate` (URL with `{z}`, `{x}`, `{y}` tokens), `tileRequestHeaders`, `attribution`, `minZoom`, `maxZoom`, `cachePath` (desktop only).

**Imperative handle (`MapImperativeHandle`):** `render(centerX, centerY, zoom)` where centerX=longitude, centerY=latitude. `setMarkers`/`clearMarkers`, `setPolylines`/`clearPolylines`/`appendPolylinePoint`, `setOverlays`/`clearOverlays`, `prefetchTiles`. All dispatch JSON ops via `HandleInternalOp()`.

**Overlays:** Markers are filled circles with optional labels. Polylines support `pointsLimit` for FIFO streaming trails. Overlays render circles or ellipses (`radiusMinorMeters > 0`) sized in meters, auto-scaled with zoom.

**Events:** `onChange` (zoom level via `m_onNumericValueChange`), `onPrefetchProgress` (completed/total via `m_onPrefetchProgress`).

**Key files:** `map_view.h/.cpp` (widget), `tiledownloader.h/.cpp` (platform-abstracted fetch), `tilecache.h/.cpp` (in-memory LRU), `disk_tile_cache.h/.cpp` (filesystem cache), `MapView.tsx` (React component + imperative handle).

**Warning:** `prefetchTiles()` bulk-downloads tiles. This violates the [OpenStreetMap tile usage policy](https://operations.osmfoundation.org/policies/tiles/). Only use with tile servers that permit bulk downloading.

## QuickJS Draw Bindings

QuickJS-NG is embedded for scripted canvas rendering. The bindings expose ImDrawList drawing primitives to JavaScript running in a QuickJS context.

**Key file:** `cpp/tests/quickjs_draw_bindings.h` (header-only, lives in tests/ until Stage 4 integrates into app/).

**`DrawContext`** struct is stored as the QuickJS context opaque pointer (`JS_SetContextOpaque`/`JS_GetContextOpaque`). It holds an `ImDrawList*` (set per-frame by the Canvas widget's `Render()`) and an `ImVec2 offset` (screen-space translation from `GetCursorScreenPos`).

**14 bound JS functions:** `drawLine`, `drawRect`, `drawRectFilled`, `drawCircle`, `drawCircleFilled`, `drawTriangle`, `drawTriangleFilled`, `drawText`, `drawPolyline`, `drawBezierCubic`, `drawNgon`, `drawNgonFilled`, `drawEllipse`, `drawEllipseFilled`. Each extracts args from JSValue, parses CSS color strings, applies coordinate offset, and calls `ImDrawList::AddXxx()`.

**Color handling:** CSS color strings are parsed via `extractColor()` from `color_helpers.h`, then converted to `ImU32` via `ImGui::ColorConvertFloat4ToU32()` (pure math, no ImGui context needed).

**Testing pattern:** `DrawContext.recording = true` with `drawList = nullptr` captures call parameters into a `std::vector<DrawCall>` for verification without needing an ImGui/GL context. Tests verify argument parsing, color conversion, offset application, and null-safety.

**vcpkg:** `quickjs-ng` is in `cpp/tests/vcpkg.json`. Will be added to `cpp/app/vcpkg.json` in Stage 4.

## Canvas 2D API Shim

Wraps the 15 ImDrawList draw bindings in an HTML5 Canvas 2D-style API. Created by `canvas2d_shim.h` (JS source as C++ raw string literal), auto-evaluated in `Canvas::InitQuickJS()` after `registerDrawBindings()`. Available as `globalThis.ctx` in every Canvas widget script alongside the raw `drawXxx` functions.

**Key file:** `cpp/app/include/canvas2d_shim.h`

**State properties:** `fillStyle`, `strokeStyle`, `lineWidth`, `globalAlpha`, `font` (parses px size), `textAlign`, `textBaseline`, `lineDashOffset`.

**State stack:** `save()` / `restore()` push/pop all properties, transform matrix, dash pattern, and clip rects.

**Basic drawing:** `fillRect()` → `drawRectFilled`, `strokeRect()` → `drawRect`, `clearRect()` (fills black — ImDrawList is write-only), `fillText()` → `drawText` (with alignment offsets from `textAlign`/`textBaseline`).

**Path API:** `beginPath()`, `moveTo()`, `lineTo()`, `closePath()`, `arc()` (tessellated), `bezierCurveTo()`, `quadraticCurveTo()` (elevated to cubic), `rect()`. `stroke()` → `drawPolyline`, `fill()` → `drawConvexPolyFilled` (convex paths only).

**Transforms:** 3x2 affine matrix in JS. `translate()`, `rotate()`, `scale()`, `transform()`, `setTransform()`, `resetTransform()`, `getTransform()`. Coordinates transformed via `_tx(x,y)` before reaching draw calls. Rotated/scaled `fillRect`/`strokeRect` emit `drawConvexPolyFilled`/`drawPolyline` (4 transformed corners); identity/translation-only fast path uses `drawRectFilled` directly.

**Text measurement:** `measureText(text)` → C++ `__measureText` binding → `ImFont::CalcTextSizeA()`. `DrawContext::currentFont` set per-frame in `Canvas::Render()`.

**Dashed lines:** `setLineDash(segments)` / `getLineDash()` / `lineDashOffset` — pure JS polyline decomposition into dash/gap segments.

**Clipping:** `clip()` computes axis-aligned bounding box of current path → `__pushClipRect`. `restore()` pops clips pushed since matching `save()`.

**New C++ bindings (4 total):** `drawConvexPolyFilled(points, color)`, `__measureText(text)`, `__pushClipRect(x1, y1, x2, y2)`, `__popClipRect()`.

## Canvas Script Loading & Error Reporting

**`setScriptFile(path)`** loads canvas scripts from external `.js` files instead of inline strings. Desktop: `std::ifstream` (synchronous in `HandleInternalOp`). WASM: `emscripten_fetch` (async, queued into `m_pendingScripts`, evaluated next `Render()` frame). Shared `Canvas::SetScriptFromString()` handles wrapping, evaluation, and error extraction.

**`onScriptError` callback** surfaces QuickJS errors (compilation + runtime) back to React. Pipeline: `JS_GetException` → `JS_ToCString` → `m_view->m_onScriptError(m_id, msg)` → NAPI TSFN / WASM EM_ASM → `dispatchEvent(id, "onScriptError", { errorMessage })`. Three error sites: shim evaluation (`InitQuickJS`), script compilation (`SetScriptFromString`), per-frame execution (`Render`).

**`init()` now takes 16 arguments** (indices 0–15). Index 15 is `onScriptError`.

**Demo scripts** live in `npm/node/src/scripts/` (filesystem paths) and `npm/wasm/public/assets/scripts/` (web-relative URLs). Both dashboards use `setScriptFile()` instead of inline `setScript()` template strings.

**Canvas dimensions:** `ctx.canvas.width` / `ctx.canvas.height` read from `__canvasWidth` / `__canvasHeight` globals, set per-frame in `Canvas::Render()` via `JS_SetPropertyStr`.

**Limitations:** `clearRect` fills with black (no true erase), `globalAlpha` stored but not applied to colors, `font` parses px size but doesn't switch ImGui fonts, `fill()` only correct for convex paths, `clip()` is axis-aligned bounding box only.

## Font Loading

Font files (.ttf) must be placed in `{assetsBasePath}/fonts/` (e.g. `assets/fonts/roboto-regular.ttf`). The C++ font loader resolves paths as `fmt::format("{}/fonts/{}.ttf", m_assetsBasePath, fontName)` — the font `name` in `fontDefs` must match the filename without extension. Font Awesome 6 Solid is automatically merged into every loaded font. Per-widget font is set via style: `font: { name: "roboto-mono", size: 14 }`. The font name+size must be declared in `fontDefs` at startup or it won't be loaded.

## C++ Gotchas

- MSVC does not allow default member initializers in unnamed structs used with `using` typedefs. Use named `struct Foo { ... };` instead of `using Foo = struct { ... };` when fields have defaults.

## Platform Notes

- Desktop rendering: GLFW + OpenGL 3
- WASM rendering: WebGPU only (Chrome, Edge, Firefox Nightly)
- WSL2 requires `export GALLIUM_DRIVER=d3d12`
- WASM output is a single `.mjs` file with embedded WASM (`-s SINGLE_FILE=1`)
