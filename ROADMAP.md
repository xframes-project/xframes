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

- [ ] Define `TileKey` struct (`int x, y, zoom`) and `TileEntry` struct (raw PNG bytes + decode state)
- [ ] Create GPU texture registry: `std::unordered_map<TileKey, GLuint>` for uploaded textures, separate from the existing `TileCache` (which stores raw PNG bytes)
- [ ] Implement visible-tile calculation in MapView: given center (lon, lat), zoom, and viewport size, compute the set of `TileKey`s that overlap the viewport (port the grid math from `MapGenerator::DrawLayer()`)
- [ ] Implement per-tile screen positioning: `TileKey` → screen rect `(x0, y0, x1, y1)` using the `XToPx`/`YToPx` pattern from MapGenerator
- [ ] Render loop: iterate visible tiles, call `ImDrawList::AddImage()` once per tile with its GL texture and screen rect

### Stage 5 — Tile Download & Decode Pipeline

Wire up individual tile fetching, decoding, and GPU upload with proper threading.

- [ ] Create a `TileGridDownloader` class (or extend `TileDownloader`) that accepts a list of `TileKey`s, checks `TileCache` first, then downloads missing tiles via libcurl multi (desktop) or Emscripten Fetch (WASM)
- [ ] Background thread: download + decode tiles (PNG bytes → RGBA pixels via `stbi_load_from_memory`, avoiding Leptonica entirely for decode-only)
- [ ] Render-thread upload: pending decoded tiles are picked up each frame and uploaded via `glTexImage2D` (same mutex + pending-queue pattern as current `PendingTexture`)
- [ ] Populate the GPU texture registry as tiles arrive — partial renders are fine (show available tiles, leave gaps for pending ones)
- [ ] Placeholder tile: render a solid gray rect for tiles not yet loaded

### Stage 6 — Smooth Panning

Sub-pixel smooth panning with incremental edge-tile fetching.

- [x] Track center as fractional tile coordinates (`double m_centerTileX, m_centerTileY`) — update continuously during drag via pixel delta → tile delta conversion
- [x] On each frame during drag: recalculate visible tile set, kick off downloads for any new tiles that scrolled into view
- [x] Keep existing tiles in the GPU registry as long as they're nearby — don't evict on every pan
- [x] No full re-render on drag end — panning is continuous, tiles load incrementally as the viewport moves
- [ ] Tile clipping: when a tile is partially off-screen, clip via UV coordinates (existing pattern)

### Stage 7 — Zoom

Scroll-wheel zoom with tile-level transitions.

- [x] `ImGui::GetIO().MouseWheel` on the map canvas increments/decrements zoom level (clamped 1–17)
- [x] On zoom change: recalculate visible tile set at new zoom level, start fetching new tiles
- [ ] While new-zoom tiles load, scale the old-zoom tiles as a placeholder (render at 2x or 0.5x size via adjusted screen rects)
- [x] Once all new-zoom tiles arrive, drop old-zoom textures from the registry
- [ ] Expose zoom level to React via an `onZoomChange` callback

### Stage 8 — Optimization & Resource Management

VRAM and memory management for long panning sessions.

- [x] GPU texture eviction: discard textures for tiles more than 2 tile-widths outside the viewport
- [ ] VRAM budget tracking: cap total uploaded textures (e.g., 512 tiles × 256×256×4 ≈ 128MB) with LRU eviction
- [ ] Prefetching: when panning in a direction, preemptively fetch 1 row/column of tiles ahead of the viewport edge
- [ ] `TileCache` tuning: increase `maxEntries` via `TileCache::configure()` at MapView init (default 256 is low for tile-grid); expose `configure()` via NAPI for runtime tuning from JS; consider multi-zoom-level prefetching to warm cache for adjacent zoom levels
- [x] Deduplicate in-flight requests: if a tile download is already pending, don't queue a duplicate

### Stage 9 — Map Overlays & u-center lite Integration

- [ ] Pin marker overlay (render markers at given lat/lon coordinates on top of the tile grid)
- [ ] Route/polyline overlay (connect sequential positions)
- [ ] Wire into u-center lite Position Tracking panel (Phase 3) — live position dot on map updated from NAV-PVT messages
- [ ] Map center follows GPS fix when in "follow" mode

### Stage 10 — Documentation

- [ ] Add MapView to widget catalog on xframes.dev (props, imperative handle, tile-grid architecture, code example)
- [ ] Add `MapImperativeHandle` to imperative handle reference page
- [ ] Update CLAUDE.md with MapView widget section (tile-grid model, TileCache, GPU texture registry)

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

## Low Priority

- [ ] Allow plain numbers for `padding` and `margin` (e.g. `padding: 8` as shorthand for `padding: { all: 8 }`)

---

## Non-Goals (for now)

- Additional language bindings beyond Node.js
- Mobile targets
- Accessibility (important, but not blocking the showcase)
- Full u-center 2 feature parity — this is a focused demo, not a product replacement
