# XFrames Roadmap

## Vision

Build a lightweight u-center 2 alternative as the flagship showcase for XFrames — proving that a React-driven, DOM-free, ImGui-based framework can replace Electron for real-time data-heavy desktop applications. The showcase is built on XFrames + a native npm module wrapping [cc.ublox.generated](https://github.com/commschamp/cc.ublox.generated) for sub-millisecond UBX message parsing.

---

## Phase 1 — Core Widget Hardening

The current widget set needs to mature before it can support a real application. Priority is on the widgets the showcase will exercise hardest.

### Table

The Table widget today renders string-only cells with no interactivity beyond virtual scrolling. It needs:

- [x] Column sorting (ascending/descending toggle, sort callback to JS)
- [x] Column filtering / search (per-column text filter via `ImGuiTextFilter`, `filterable` prop)
- [x] Row selection with click event emitted to JS
- [x] Typed cell values (numbers, booleans, icons — not just strings)
- [x] Column reordering (`ImGuiTableFlags_Reorderable`)
- [x] Column visibility toggles (`ImGuiTableFlags_Hideable`)
- [x] Configurable column flags from JS (width mode, default sort, etc.)

### InputText

- [x] No buffer size limit (switched from `char[]` to `std::string` via `imgui_stdlib.h`)
- [x] Multiline input (`ImGui::InputTextMultiline`)
- [x] Password masking (`ImGuiInputTextFlags_Password`)
- [x] Read-only mode (`ImGuiInputTextFlags_ReadOnly`)
- [x] Numeric-only mode (`ImGuiInputTextFlags_CharsDecimal`)

### Plotting

PlotLine and PlotCandlestick exist. The showcase needs more chart types from ImPlot:

- [x] Bar chart (`ImPlot::PlotBars`)
- [x] Scatter plot (`ImPlot::PlotScatter`)
- [x] Heatmap (`ImPlot::PlotHeatmap`) — useful for signal quality grids
- [x] Multi-series support for PlotLine (multiple Y datasets sharing one X axis)
- [x] Axis label configuration from JS
- [x] Legend control
- [x] Fix: wire PlotCandlestick `bullColor`/`bearColor` props through to C++ (currently ignored)

### Misc

- [x] Increase PlotLine/PlotCandlestick data point cap (currently 6,000 — may need to be configurable)
- [x] Progress bar widget
- [x] Color indicator widget (for fix status, signal health)
- [x] Load Unicode-capable font glyphs so Table boolean columns can render ✓/✗ instead of Yes/No

---

## Phase 1.5 — Early Adopter Features

Desktop interactivity patterns and additional widgets that make XFrames feel production-ready.

### Tabs

- [x] Tab close buttons (`closeable` prop, `p_open` parameter)
- [x] Tab reordering (`ImGuiTabBarFlags_Reorderable`)

### New Widgets

- [x] ColorPicker widget (`ImGui::ColorEdit4`) with onChange hex string callback
- [x] Histogram chart (`ImPlot::PlotHistogram`) — auto-binned distribution plot
- [x] Pie chart (`ImPlot::PlotPieChart`) — labeled slice chart

### Table Enhancements

- [x] Right-click context menu on table rows (declarative `contextMenuItems` prop)

### Callback Infrastructure

- [x] New `onItemAction` callback type for context menu actions (adds 14th init arg)

---

## Phase 1.75 — Map Widget Integration

The [osm-static-map-generator](https://github.com/andreamancuso/osm-static-map-generator) submodule provides OSM raster tile compositing for both desktop (libcurl + Leptonica) and browser (Emscripten Fetch + Leptonica). A MapView widget already exists in C++ and TypeScript but is currently disabled on desktop. This phase enables it end-to-end and adds it to the demo dashboard, laying groundwork for the u-center lite position tracking panel.

### Stage 1 — Submodule & Build Plumbing

- [x] Update osm-static-map-generator submodule to latest (`93d12b5`) — brings in LRU tile cache with TTL, curl-based native downloader with retry/exponential backoff, Emscripten 5.0.2 upgrade
- [x] Add `curl` to `vcpkg.json` (`cpp/app/vcpkg.json`)
- [x] Add `find_package(CURL REQUIRED)` and link `CURL::libcurl` in `CMakeLists.txt` (`cpp/app/CMakeLists.txt`)
- [x] Add `tilecache.cpp` to `OSM_STATIC_MAP_GENERATOR_SRC` (new file in updated library)
- [x] Uncomment `map_view.cpp` in `XFRAMES_SRC`
- [x] Uncomment `${OSM_STATIC_MAP_GENERATOR_SRC}` in the `add_library` call
- [x] Mirror all build changes to `npm/node/CMakeLists.txt` and `npm/node/vcpkg.json` (Node addon has its own separate CMake build)
- [x] Generate `endianness.h` for Leptonica and add `HAVE_LIBJPEG`/`HAVE_LIBTIFF`/`HAVE_LIBPNG` compile definitions
- [x] Add `NOMINMAX` compile definition to prevent Windows `min`/`max` macro conflicts with `ada` headers

### Stage 2 — Desktop Widget Activation

- [x] Remove `#ifdef __EMSCRIPTEN__` guard around MapView include and registration in `xframes.cpp`
- [x] Update `map_view.cpp` `HandleInternalOp()` to read Yoga layout dimensions instead of hardcoded 600×600
- [x] Add `User-Agent` header to tile requests for OSM tile server compliance
- [x] Fix GL Error 1282: deferred texture upload via mutex-protected pending buffer (OpenGL calls must run on the GL render thread, not the MapGenerator callback thread)
- [x] Background tile download via `std::thread` to avoid blocking the render loop
- [x] Flicker-free texture swap: store pending offset alongside pending texture data, apply atomically

### Stage 3 — Demo Dashboard Integration

- [x] Add MapView row to `Dashboard.tsx` with render button and zoom slider (1–17)
- [x] Auto-render a default location (London: `−0.1276, 51.5074`, zoom 13) on mount
- [x] Follow existing imperative handle pattern (`useRef<MapImperativeHandle>` + `useEffect` + `setTimeout`)
- [x] Basic pan support: 3x oversized texture buffer with drag-end re-render at new center

### Stage 4 — Tile-Grid Foundation

Replace the monolithic composited-PNG approach with individual tile textures rendered in a grid. This eliminates visual jumps on pan and enables incremental tile loading.

- [x] Define `TileKey` struct (`int x, y, zoom`) and `TileEntry` struct (raw PNG bytes + decode state)
- [x] Create GPU texture registry: `std::unordered_map<TileKey, GLuint>` for uploaded textures, separate from the existing `TileCache` (which stores raw PNG bytes)
- [x] Implement visible-tile calculation in MapView: given center (lon, lat), zoom, and viewport size, compute the set of `TileKey`s that overlap the viewport (port the grid math from `MapGenerator::DrawLayer()`)
- [x] Implement per-tile screen positioning: `TileKey` → screen rect `(x0, y0, x1, y1)` using the `XToPx`/`YToPx` pattern from MapGenerator
- [x] Render loop: iterate visible tiles, call `ImDrawList::AddImage()` once per tile with its GL texture and screen rect

### Stage 5 — Tile Download & Decode Pipeline

Wire up individual tile fetching, decoding, and GPU upload with proper threading.

- [x] Create a `TileGridDownloader` class (or extend `TileDownloader`) that accepts a list of `TileKey`s, checks `TileCache` first, then downloads missing tiles via libcurl multi (desktop) or Emscripten Fetch (WASM)
- [x] Background thread: download + decode tiles (PNG bytes → RGBA pixels via `stbi_load_from_memory`, avoiding Leptonica entirely for decode-only)
- [x] Render-thread upload: pending decoded tiles are picked up each frame and uploaded via `glTexImage2D` (same mutex + pending-queue pattern as current `PendingTexture`)
- [x] Populate the GPU texture registry as tiles arrive — partial renders are fine (show available tiles, leave gaps for pending ones)
- [x] Placeholder tile: render a solid gray rect for tiles not yet loaded

### Stage 6 — Smooth Panning

Sub-pixel smooth panning with incremental edge-tile fetching.

- [x] Track center as fractional tile coordinates (`double m_centerTileX, m_centerTileY`) — update continuously during drag via pixel delta → tile delta conversion
- [x] On each frame during drag: recalculate visible tile set, kick off downloads for any new tiles that scrolled into view
- [x] Keep existing tiles in the GPU registry as long as they're nearby — don't evict on every pan
- [x] No full re-render on drag end — panning is continuous, tiles load incrementally as the viewport moves
- [x] Tile clipping: when a tile is partially off-screen, clip via UV coordinates (existing pattern)

### Stage 7 — Zoom

Scroll-wheel zoom with tile-level transitions.

- [x] `ImGui::GetIO().MouseWheel` on the map canvas increments/decrements zoom level (clamped 1–17)
- [x] On zoom change: recalculate visible tile set at new zoom level, start fetching new tiles
- [x] While new-zoom tiles load, scale the old-zoom tiles as a placeholder (render at 2x or 0.5x size via adjusted screen rects)
- [x] Once all new-zoom tiles arrive, drop old-zoom textures from the registry
- [x] Expose zoom level to React via an `onZoomChange` callback

### Stage 8 — Optimization & Resource Management

VRAM and memory management for long panning sessions.

- [x] GPU texture eviction: discard textures for tiles more than 2 tile-widths outside the viewport
- [x] VRAM budget tracking: cap total uploaded textures (e.g., 512 tiles × 256×256×4 ≈ 128MB) with LRU eviction
- [x] Prefetching: when panning in a direction, preemptively fetch 1 row/column of tiles ahead of the viewport edge
- [ ] `TileCache` tuning: increase `maxEntries` via `TileCache::configure()` at MapView init (default 256 is low for tile-grid); expose `configure()` via NAPI for runtime tuning from JS; consider multi-zoom-level prefetching to warm cache for adjacent zoom levels
- [x] Deduplicate in-flight requests: if a tile download is already pending, don't queue a duplicate

### Stage 9 — Map Overlays & u-center lite Integration

- [x] Pin marker overlay (render markers at given lat/lon coordinates on top of the tile grid)
- [x] Route/polyline overlay (connect sequential positions)
- [ ] Wire into u-center lite Position Tracking panel (Phase 3) — live position dot on map updated from NAV-PVT messages
- [ ] Map center follows GPS fix when in "follow" mode

### Stage 10 — Documentation

- [x] Add MapView to widget catalog on xframes.dev (props, imperative handle, tile-grid architecture, code example)
- [x] Add `MapImperativeHandle` to imperative handle reference page
- [x] Update CLAUDE.md with MapView widget section (tile-grid model, TileCache, GPU texture registry)

---

## Phase 2 — Showcase Foundation (u-center lite)

Integrate the UBX native npm module and build the core panels of the showcase app.

### UBX Module Integration

- [ ] Publish the cc.ublox.generated npm wrapper (or integrate as a local dependency)
- [ ] Define TypeScript types for key UBX message classes (NAV-PVT, NAV-SAT, NAV-STATUS, MON-VER, CFG-VALSET, etc.)
- [ ] Serial port connection management (device selection, baud rate, connect/disconnect)

### Message View

- [ ] Live UBX message table — columns: timestamp, class, ID, length, summary
- [ ] Message filtering by class/ID
- [ ] Message rate indicator (messages/sec)
- [ ] Hex dump detail view for selected message (ClippedMultiLineTextRenderer)

### Navigation Status Panel

- [ ] Fix type indicator (No Fix / 2D / 3D / DGNSS / RTK Float / RTK Fixed) with color coding
- [ ] Position display: latitude, longitude, altitude (MSL + ellipsoid)
- [ ] Accuracy estimates: horizontal, vertical, speed
- [ ] Time display: UTC, GPS time, time accuracy
- [ ] Number of satellites used / in view

---

## Phase 3 — Showcase Visualization

The panels that make the app visually compelling and demonstrate XFrames' rendering performance.

### Satellite Sky View

- [ ] Custom polar plot widget (azimuth/elevation projection)
- [ ] Satellite markers colored by constellation (GPS, GLONASS, Galileo, BeiDou)
- [ ] Satellite PRN labels
- [ ] Signal strength color coding on markers
- [ ] Used-in-fix vs tracked distinction (filled vs hollow markers)

### Signal Strength

- [ ] Per-satellite C/N0 bar chart, grouped by constellation
- [ ] Color coding by signal quality threshold
- [ ] Multi-signal support (L1, L2, L5 — multiple bars per satellite)

### Position Tracking

- [ ] Real-time position scatter plot (2D: lat/lon deviation from mean)
- [ ] CEP (circular error probable) overlay
- [ ] Altitude over time line plot
- [ ] Speed over time line plot

### Streaming Architecture

- [ ] Efficient data pipeline: serial port → native parser → JS → XFrames render loop
- [ ] Configurable update rates (throttle UI updates independently of message rate)
- [ ] Benchmark harness: measure end-to-end latency from byte arrival to pixel

---

## Phase 4 — Polish & Ecosystem

### Documentation (xframes.dev Docusaurus site)

#### Getting Started
- [x] Expand intro.md beyond "Hello, world" — explain `render()` args (assets path, fontDefs, theme), root Node pattern, basic layout
- [x] Layout guide — Yoga flexbox basics (row/column, flex, padding/margin objects, percentage widths), scroll containers (`overflow: "scroll"`, `flexShrink: 0`)
- [x] Fonts guide — fontDefs shape, available font sizes, `font: { name, size }` in style rules, Font Awesome icons

#### Widget Catalog (grouped doc pages with props, events, imperative handles, code examples)
- [x] Layout components — Node, Child, Group, DIWindow, CollapsingHeader, ItemTooltip, TextWrap
- [x] Text components — UnformattedText, BulletText, DisabledText, SeparatorText, Separator, ClippedMultiLineTextRenderer
- [x] Form controls — Button, Checkbox, Combo, InputText (multiline/password/readOnly/numericOnly), Slider, MultiSlider, ColorPicker
- [x] Display components — ColorIndicator (shapes), ProgressBar, Image
- [x] Table — columns config, typed cells (string/number/boolean), sorting, filtering (text/boolean/number), row selection, column reordering/visibility, context menus, imperative handle
- [x] Navigation — TabBar (reorderable), TabItem (closeable), TreeNode, TreeView
- [x] Plots — PlotLine (multi-series, streaming), PlotBar, PlotScatter, PlotHeatmap, PlotHistogram, PlotPieChart, PlotCandlestick; cover data models, imperative handles, axis/legend config

#### Styling & Theming
- [x] Flesh out general-styling-concepts.md — style/hoverStyle/activeStyle/disabledStyle, RWStyleSheet.create(), style composition
- [x] Flesh out yoga-layout-properties.md — add usage examples and gotchas (padding/margin objects, not plain numbers)
- [x] Flesh out base-drawing-style-properties.md — backgroundColor, border, rounding with examples
- [x] Theming guide — built-in themes (Dark/Light/Ocean), XFramesStyleForPatching type, ImGuiCol enum, runtime switching via patchStyle

#### API Reference
- [x] Event types reference — CheckboxChangeEvent, InputTextChangeEvent, ComboChangeEvent, SliderChangeEvent, MultiSliderChangeEvent, TabItemChangeEvent, TableSortEvent, TableFilterEvent, TableRowClickEvent, TableItemActionEvent
- [x] Imperative handle reference — TableImperativeHandle, PlotLineImperativeHandle, PlotBarImperativeHandle, PlotScatterImperativeHandle, PlotHeatmapImperativeHandle, PlotHistogramImperativeHandle, PlotPieChartImperativeHandle, PlotCandlestickImperativeHandle, ComboImperativeHandle, InputTextImperativeHandle
- [x] ImGui enums reference — ImGuiCol, ImGuiStyleVar, ImGuiDir, ImPlotMarker, ImPlotScale, ImPlotColormap

### Website & Positioning (xframes.dev)
- [x] Refocus homepage messaging on TypeScript/React — lead with the primary supported stack rather than 20+ language logos
- [x] Move multi-language FFI showcase to a dedicated "Language Bindings" page (experimental/community status) instead of homepage hero
- [x] Update homepage feature cards to highlight concrete developer benefits: React components, Yoga layout, live theming, ImPlot charts
- [ ] Add a live screenshot or GIF of the Dashboard demo on the homepage
- [x] Consolidate intro.md to focus on the Node.js/TypeScript getting-started path; move other languages to a separate "Other Languages" doc page with appropriate "experimental" labels

### create-xframes-node-app

- [x] Add a "demo" template option that scaffolds a meaningful example (table + plot + form)
- [x] Pin and track latest package versions
- [x] Print `npm start` instruction after scaffold completes
- [x] Replace raw `curl` downloads with bundled templates for reliability

### Performance Story

- [ ] Publish benchmark: XFrames showcase vs Electron-based equivalent
- [ ] Metrics: startup time, memory footprint, CPU usage at idle, frame rate under load
- [ ] Include numbers in README and showcase repo

### Theming

- [x] Ship 3 polished built-in themes (Dark, Light, Ocean) with accent colors
- [x] Theme switching at runtime in the showcase app

---

## Phase 5 — WASM Build Modernization (emsdk 3.1.60 → 5.0.2)

Docker-based WASM build infrastructure is in place (`packages/dear-imgui/cpp/wasm/Dockerfile.wasm` + `build-wasm-docker.sh`), now pinned to emsdk 5.0.2. The emsdk 5.x line removed the legacy `-sUSE_WEBGPU=1` flag entirely, replacing it with `--use-port=emdawnwebgpu` (Dawn WebGPU) which ships an incompatible `webgpu.h`.

### Stage 1 — imgui overlay port (done)

- [x] Bump overlay port version from 1.90.6 to 1.92.6 (aligns with imgui submodule)
- [x] Update overlay CMakeLists.txt to pass `--use-port=emdawnwebgpu` and define `IMGUI_IMPL_WEBGPU_BACKEND_DAWN` on Emscripten
- [x] Update main WASM CMakeLists.txt: replace `-s USE_WEBGPU=1` with `--use-port=emdawnwebgpu` in link flags
- [x] Note: imgui 1.92.6 has a version detection bug — `(__EMSCRIPTEN_TINY__ >= 10)` fails for emsdk 5.0.x (TINY=2). Overlay port forces `IMGUI_IMPL_WEBGPU_BACKEND_DAWN` to work around it.

### Stage 2 — Migrate `imgui_renderer.h/.cpp`

The Dawn WebGPU API removes the swap chain abstraction and renames many types. All `#ifdef __EMSCRIPTEN__` blocks in `imgui_renderer.h/.cpp` need updating.

**Type renames:**
- [x] `WGPUSwapChain` → remove (surface configuration replaces swap chains)
- [x] `WGPUSwapChainDescriptor` → `WGPUSurfaceConfiguration`
- [x] `wgpuDeviceCreateSwapChain()` → `wgpuSurfaceConfigure()`
- [x] `wgpuSwapChainRelease()` → `wgpuSurfaceUnconfigure()` (or just reconfigure)
- [x] `wgpuSwapChainGetCurrentTextureView()` → `wgpuSurfaceGetCurrentTexture()` + create view from texture
- [x] `WGPUImageCopyTexture` → `WGPUTexelCopyTextureInfo`
- [x] `WGPUTextureDataLayout` → `WGPUTexelCopyBufferLayout`
- [x] String label fields (`const char*`) → `WGPUStringView` (struct with `.data` + `.length`)

**Function signature changes:**
- [x] `wgpuDeviceSetUncapturedErrorCallback()` — new signature with `WGPUUncapturedErrorCallbackInfo`
- [x] `WGPURenderPassColorAttachment` — `depthSlice` field may be removed or restructured
- [x] `wgpu::Instance::CreateSurface()` — C++ wrapper API changed
- [x] `surface.GetPreferredFormat()` → `wgpuSurfaceGetCapabilities()` + select format

**Swap chain → surface pattern:**
- [x] Replace `CreateSwapChain()` method with `ConfigureSurface(int width, int height)`
- [x] In `PerformRendering()`: get current texture from surface, create view, render, present
- [x] In `HandleScreenSizeChanged()`: reconfigure surface instead of recreating swap chain

### Stage 3 — Migrate callback signatures

- [x] `wgpu::Instance::RequestAdapter()` callback — new signature in Dawn API (uses `WGPURequestAdapterCallbackInfo` struct instead of raw function pointer)
- [x] `wgpu::Adapter::RequestDevice()` callback — same pattern change
- [x] `WGPURequestAdapterStatus_Success` → verify enum name/value still matches
- [x] Review `wgpu::Adapter::Acquire()` and `wgpu::Surface::MoveToCHandle()` — may have renamed or changed semantics

### Stage 4 — Migrate `LoadTexture()` (WASM path)

- [x] `WGPUTextureDescriptor.label` — change from `const char*` to `WGPUStringView`
- [x] `tex_desc.viewFormatCount` / `tex_desc.viewFormats` — may be restructured
- [x] `WGPUImageCopyTexture` → `WGPUTexelCopyTextureInfo`
- [x] `WGPUTextureDataLayout` → `WGPUTexelCopyBufferLayout`

### Stage 5 — Migrate `wasm/src/main.cpp` (embind entry point)

- [x] Audit all direct WebGPU calls in `main.cpp` for Dawn API compatibility
- [x] Replace deprecated `EM_ASM_ARGS` with `EM_ASM` (variadic args, drop-in replacement since emsdk 4.0)
- [x] Test the full JS → WASM → WebGPU pipeline end-to-end

### Stage 6 — Flip to emsdk 5.0.2

- [x] Update `Dockerfile.wasm`: `FROM emscripten/emsdk:3.1.60` → `FROM emscripten/emsdk:5.0.2`
- [x] Delete stale `build-wasm/` directory and rebuild from scratch
- [x] Update `packages/dear-imgui/cpp/wasm/README.md` emscripten version reference
- [x] Verify `npm run start` in `npm/wasm/` loads correctly in Chrome/Edge
- [x] Test MapView tile loading, table rendering, plot rendering in browser

### Additional fixes during migration

- imgui 1.92.6 enum renames applied in `main.cpp` (`ImGuiDir` cast, `LegacySize`, etc.)
- libtiff configure step added to `CMakeLists.txt` (required by Leptonica in osm-static-map-generator)
- `tilecache.cpp` added to source list (new file in updated osm-static-map-generator)
- Obsolete linker flags removed: `--no-heap-copy`, `-s WASM=1`, `--emit-tsd`, `-s STANDALONE_WASM=0`
- `EM_ASM_ARGS` → `EM_ASM` deprecation fix (13 occurrences in `main.cpp`)

### Reference

- imgui 1.92.6 changelog: "2025-10-16: Update to compile with Dawn and Emscripten's 4.0.10+ `--use-port=emdawnwebgpu`"
- Dawn WebGPU migration: types renamed, swap chain removed, string labels → `WGPUStringView`, callback signatures restructured
- Emscripten port info: `tools/ports/emdawnwebgpu.py` in emsdk source
- XFrames files modified: `cpp/app/include/imgui_renderer.h`, `cpp/app/src/imgui_renderer.cpp`, `cpp/wasm/src/main.cpp`, `cpp/wasm/CMakeLists.txt`

---

## Phase 6 — Canvas Widget (QuickJS-NG)

A generic custom drawing surface that exposes ImGui's ImDrawList primitives to JavaScript. Instead of sending draw commands as JSON across the NAPI/WASM bridge (which scales poorly with visual complexity), we embed [QuickJS-NG](https://github.com/quickjs-ng/quickjs) so rendering scripts run directly in C++ with zero-overhead ImDrawList calls. Data (small, semantic) crosses the bridge; rendering logic (potentially hundreds of draw calls per frame) runs in-process.

**Why QuickJS-NG:** Full ES2023, ~1 MB binary, MIT license, compiles as C source files (fits existing build pattern), [quickjspp](https://github.com/ftk/quickjspp) provides clean C++ class/function binding. Amazon LLRT and txiki.js validate it at production scale. Already proven with ImGui ([qjs-imgui](https://github.com/rsenn/qjs-imgui) bindings exist).

**Architecture:** React sends raw data (e.g. satellite positions) via `elementInternalOp` → C++ passes data into the QuickJS context → JS rendering script calls bound ImDrawList methods directly (addLine, addCircleFilled, etc.) → ImGui renders. Data crosses the bridge once when it changes; the render function executes at frame rate in-process with zero serialization.

### Stage 1 — Submodule & Build Plumbing

- [ ] Add quickjs-ng as git submodule (`cpp/deps/quickjs`)
- [ ] Add quickjspp (header-only C++ wrapper) as git submodule (`cpp/deps/quickjspp`)
- [ ] Add quickjs source files to all 4 CMakeLists.txt (`cpp/app`, `npm/node`, `cpp/wasm`, `cpp/tests`)
- [ ] Verify clean compilation on Windows (MSVC) and confirm no conflicts with existing deps

### Stage 2 — Basic Embedding Proof (Unit Tests)

- [ ] Unit test: construct/destroy QuickJS runtime + context (no leak, no crash)
- [ ] Unit test: evaluate `1 + 1`, verify int result
- [ ] Unit test: evaluate arrow function with destructuring (confirm ES2023 features work)
- [ ] Unit test: expose a C++ function to JS, call it from JS, verify side effects

### Stage 3 — ImDrawList Bindings

- [ ] Create `quickjs_bindings.h/.cpp` — exposes ImDrawList drawing methods to JS
- [ ] Bind core primitives: addLine, addRect, addRectFilled, addCircle, addCircleFilled, addTriangle, addTriangleFilled, addText, addPolyline, addBezierCubic, addNgon, addEllipse, addEllipseFilled
- [ ] Color handling: JS passes CSS color strings, binding layer converts via `extractColor()` once per call
- [ ] Coordinate translation: all JS coordinates are canvas-local (0,0 = top-left of widget), binding adds `ImGui::GetCursorScreenPos()` offset
- [ ] Unit tests for each bound function

### Stage 4 — Canvas Widget Skeleton

- [ ] New widget files: `canvas.h`, `canvas.cpp` (extends StyledWidget)
- [ ] Holds `JSRuntime*` + `JSContext*` per widget instance
- [ ] `makeWidget()` creates QuickJS runtime, registers ImDrawList bindings
- [ ] `Render()` each frame: gets `ImDrawList*`, sets it in JS context, calls the stored JS render function
- [ ] `HasInternalOps()` returns true
- [ ] `HandleInternalOp()` supports `setScript` (compile + store JS render function) and `setData` (update data global in JS context)
- [ ] Register as `"canvas"` in `SetUpElementCreatorFunctions()`
- [ ] Data persists in QuickJS context between frames — only re-sent when it changes

### Stage 5 — React Integration

- [ ] TypeScript types: `CanvasProps`, `CanvasImperativeHandle` (with `setScript`, `setData`, `clear`)
- [ ] React component `Canvas.tsx` in `@xframes/common`
- [ ] `WidgetRegistrationService` methods: `setCanvasScript(id, jsCode)`, `setCanvasData(id, data)`
- [ ] NAPI + WASM bindings (same `elementInternalOp` path as PlotLine/Table)

### Stage 6 — Small-Scale Example

- [ ] Satellite sky view rendering script (TypeScript, transpiled to ES2023)
- [ ] Polar grid (azimuth/elevation circles + radial lines) drawn from JS
- [ ] Satellite markers (colored circles + PRN labels) driven by data
- [ ] Data shape: `{ satellites: [{ prn, azimuth, elevation, snr, constellation }] }`
- [ ] React component sends mock satellite data, script renders the full visualization
- [ ] Demonstrates: static geometry (grid computed once in JS), dynamic data (satellites update), zero bridge overhead for rendering

### Reference

- [QuickJS-NG](https://github.com/quickjs-ng/quickjs) — ES2023 embeddable JS engine (~1 MB, MIT)
- [quickjspp](https://github.com/ftk/quickjspp) — Header-only C++ wrapper for QuickJS
- [CANVAS.md](./CANVAS.md) — Original design document (draw commands approach, superseded by QuickJS embedding)
- [imgui-react-runtime](https://github.com/tmikov/imgui-react-runtime) — Prior art: React + ImGui + Hermes (validates the concept)

---

## Low Priority

- [ ] Allow plain numbers for `padding` and `margin` (e.g. `padding: 8` as shorthand for `padding: { all: 8 }`)

---

## Non-Goals (for now)

- Additional language bindings beyond Node.js
- Mobile targets
- Accessibility (important, but not blocking the showcase)
- Full u-center 2 feature parity — this is a focused demo, not a product replacement
