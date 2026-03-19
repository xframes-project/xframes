# XFrames Roadmap

## Vision

Build [ubx-monitor](https://github.com/andreamancuso/ubx-monitor) as the flagship showcase for XFrames — proving that a React-driven, DOM-free, ImGui-based framework can replace Electron for real-time data-heavy desktop applications. The showcase is built on XFrames + [ubx-parser](https://www.npmjs.com/package/ubx-parser) for sub-millisecond UBX binary protocol parsing (315+ message types).

---

## Phases 1–1.5 — Core Widget Hardening & Early Adopter Features (done)

Table (sorting, filtering, typed cells, reordering, visibility, column flags, context menus), InputText, Plotting (bar, scatter, heatmap, histogram, pie, candlestick), ProgressBar, ColorIndicator, Tab close/reorder, ColorPicker.

---

## Phase 1.75 — Map Widget Integration (done)

Stages 1–10 complete: submodule plumbing, desktop activation, demo dashboard, tile-grid rendering, download pipeline, smooth panning, zoom, GPU texture eviction (512-tile LRU), prefetching, overlays (markers, polylines, accuracy circles), ubx-monitor integration with live GPS tracking.

### Remaining

- [ ] `TileCache` tuning: increase `maxEntries` via `TileCache::configure()` at MapView init (default 256 is low for tile-grid); expose `configure()` via NAPI for runtime tuning from JS

---

## Phase 2 — Showcase Foundation (ubx-monitor) (done)

Repo: [ubx-monitor](https://github.com/andreamancuso/ubx-monitor). ubx-parser integration, serial connection (SerialManager + useSerialConnection hook + ConnectionPanel with port/baud selection), UBX config commands (CFG-VALSET enabling NAV-PVT/NAV-SAT/NAV-DOP/MON-HW/NAV-STATUS on UART1), Console (raw data), Messages (table with filtering + rate), Navigation (fix type, position, accuracy, satellites, UTC time), Map (live position marker, accuracy overlay, GPS trail via MapView).

---

## Phase 3 — Showcase Visualization

The panels that make the app visually compelling and demonstrate XFrames' rendering performance.

### Satellite Sky View (done)

- [x] Custom polar plot widget (azimuth/elevation projection) — JsCanvas with sky-view.js
- [x] Satellite markers colored by constellation (GPS, GLONASS, Galileo, BeiDou)
- [x] Satellite PRN labels
- [x] Signal strength color coding on markers (dot radius scales with CNO)
- [x] Used-in-fix vs tracked distinction (filled vs hollow markers)

### Signal Strength — Color Coding (in progress)

Requires PlotBar multi-series support in XFrames (mirroring PlotLine's existing `series` architecture), then ubx-monitor panel rewrite.

#### PlotBar Multi-Series (XFrames)

Add `PlotBarSeries` struct and `std::vector<PlotBarSeries> m_series` to PlotBar (same pattern as `PlotLineSeries` in `plot_line.h`). Backward compatible — constructor creates default series[0].

- [ ] `plot_bar.h` — Add `PlotBarSeries` struct (label + xValues/yValues vectors), replace flat `m_xValues`/`m_yValues` with `m_series` vector, add `AppendSeriesData(seriesIndex, x, y)` and `SetSeriesData(json)`, parse `series` prop in `makeWidget`
- [ ] `plot_bar.cpp` — Loop `m_series` in `Render()` calling `ImPlot::PlotBars()` per series, add `"setSeriesData"` and `"appendSeriesData"` ops in `HandleInternalOp`, add `series` prop handling in `Patch()` (grow/shrink/relabel per PlotLine pattern)
- [ ] `PlotBar.tsx` — Add `series` to destructured props and JSX, add `setSeriesData()` and `appendSeriesData()` to imperative handle
- [ ] `types.ts` — Add `PlotBarSeriesDef` type (`{ label: string }`), add `series?: PlotBarSeriesDef[]` to PlotBar props
- [ ] `widgetRegistrationService.ts` — Add `setPlotBarSeriesData(id, seriesData)` and `appendPlotBarSeriesData(id, seriesIndex, x, y)`
- [ ] `ReactNativePrivateInterface.js` — Add `"series"` to `plot-bar` attribute list

#### CNO Quality Color Coding (ubx-monitor)

- [ ] Rewrite `SignalStrengthPanel.tsx` — split satellites into 4 quality-level series by CNO threshold: Weak (<20 dBHz), Moderate (20–30), Good (30–40), Excellent (>40). Each satellite appears in exactly one series. Use `PlotBar` with `series` prop, `showLegend=true`. Call `setSeriesData()` on each NAV-SAT update.

### Signal Strength — Multi-Signal (blocked)

Per-signal CNO bars (L1/L2/L5) require UBX-NAV-SIG with per-signal `cno`. Blocked on upstream bug: `cc.ublox.generated` ([commschamp/cc.ublox.commsdsl](https://github.com/commschamp/cc.ublox.commsdsl)) is missing the `Cno` (U1) field between `PrRes` and `QualityInd` in the NavSig element definition. This causes a binary alignment bug — element size is 15 bytes instead of 16, misaligning all fields after `prRes` and all elements after the first. Issue filed upstream.

Once fixed:
- [ ] Update `cc.ublox.generated` submodule in ubx-parser, add `cno: number` to NavSig list element in `types.d.ts`, rebuild
- [ ] Add `CFG-MSGOUT-UBX_NAV_SIG_UART1` to `enableUbxNavMessages()` in `ubx-commands.ts`
- [ ] Create `useNavSig` hook — per-signal data with `gnssId`, `svid`, `sigId`, `freqId`, `cno`
- [ ] Show per-signal bars grouped by satellite using PlotBar multi-series (L1/L2/L5 as series, offset X positions)

### Position Tracking (done)

- [x] Real-time position scatter plot (2D: East/North deviation from mean) — PlotScatter
- [x] CEP (circular error probable) statistics (CEP₅₀, CEP₉₅)
- [x] Altitude over time line plot — PlotLine
- [x] Speed over time line plot — PlotLine

### Streaming Architecture

- [ ] Efficient data pipeline: serial port → native parser → JS → XFrames render loop
- [ ] Configurable update rates (throttle UI updates independently of message rate)
- [ ] Benchmark harness: measure end-to-end latency from byte arrival to pixel

---

## Phase 4 — Polish & Ecosystem

### Website (xframes.dev)

- [ ] Add a live screenshot or GIF of the Dashboard demo on the homepage

### Performance Story

- [ ] Publish benchmark: XFrames showcase vs Electron-based equivalent
- [ ] Metrics: startup time, memory footprint, CPU usage at idle, frame rate under load
- [ ] Include numbers in README and showcase repo

---

## Phases 5–10 — WASM Modernization, Canvas Widgets & Scripting (done)

WASM build migrated to emsdk 5.0.2 + Dawn WebGPU. Three canvas widget engines — JsCanvas (QuickJS-NG), LuaCanvas (Sol2), JanetCanvas (Janet) — each with 19 ImDrawList draw bindings, Canvas 2D API shim, texture pipeline, `setScript`/`setScriptFile`/`setData`/`onScriptError`, and full React integration. External script file loading (desktop `std::ifstream`, WASM `emscripten_fetch`). 200+ unit tests across all engines.

---

## Phase 11 — Performance Optimization (mechanical optimizations done)

Viewport culling, idle sleep (`glfwWaitEventsTimeout`), and scroll extent fixes are done. Stages 3–5 completed all mechanical optimizations (Table ColumnType enum, persistent filteredIndices, FormatNumberValue stack buffer, parseCSSColor bypass + color cache, canvas textureLookup moved to init, JsCanvas m_hasRenderFunc, PlotPieChart label pointer cache, StyledWidget single-traversal GetCustomColorsOrNull/GetCustomStyleVarsOrNull, Table cell data find()). Remaining items below are architectural changes — deferred until profiling shows they're the bottleneck.

### Stage 1 — Render Thread Unblocking & Hot Path Deduplication (deferred)

- [ ] Switch all 13 NAPI event callbacks from `BlockingCall` to `NonBlockingCall`
- [ ] Cache `m_elements[id]` lookups in `RenderElementById`
- [ ] Fix fall-through bugs in `HasStyle()`/`GetElementStyleParts()`

### Stage 2 — Layout & Style Optimization (deferred)

- [ ] Guard `YGNodeCalculateLayout` with dirty check
- [ ] Pre-parse `ElementStyleParts::styleDef` into typed C++ struct at init time
- [ ] Cache `GetChildrenMaxBottom` result
- [ ] Pass layout values (left/top/width/height) through `Render()`

### Stages 3–5 — Widget & Style Micro-Optimizations (done)

Table ColumnType enum, persistent filteredIndices with dirty flag, FormatNumberValue stack buffer, parseCSSColor JSON bypass + DrawContext color cache, canvas textureLookup moved to init, JsCanvas m_hasRenderFunc guard, canvas dimension update guards, PlotPieChart m_labelPtrs cache, ColorIndicator/Slider string-to-bool flags, Image single find(), GetCurrentWindow hoist, StyledWidget GetCustomColorsOrNull/GetCustomStyleVarsOrNull (single traversal replacing Has+Get), Table cell data find().

### Stage 4 — Operation Queue & Bridge Efficiency (deferred)

- [ ] Replace `ElementOpDef` JSON payload with typed discriminated union
- [ ] Remove `setChildren` JSON round-trip: pass `vector<int>` directly
- [ ] `QueueAppendChild`: use typed struct instead of JSON object
- [ ] WASM: pass `0` to `emscripten_set_main_loop` for `requestAnimationFrame`

---

## npm Publishing Fixes (done)

Removed broken `prebuild-install` script from `@xframes/node`, verified native addon loads from `dist/`, published 0.1.3 with all DLLs.

## Low Priority

- [ ] Allow plain numbers for `padding` and `margin` (e.g. `padding: 8` as shorthand for `padding: { all: 8 }`)
- [ ] Restructure `@xframes/node` into platform-specific packages (`@xframes/node-win32-x64`, etc.) following the esbuild/swc pattern — each user only downloads binaries for their platform

---

## Non-Goals (for now)

- Additional language bindings beyond Node.js
- Mobile targets
- Accessibility (important, but not blocking the showcase)
- Full u-center 2 feature parity — this is a focused demo, not a product replacement
