# XFrames Roadmap

## Vision

Build a lightweight u-center 2 alternative as the flagship showcase for XFrames — proving that a React-driven, DOM-free, ImGui-based framework can replace Electron for real-time data-heavy desktop applications. The showcase is built on XFrames + a native npm module wrapping [cc.ublox.generated](https://github.com/commschamp/cc.ublox.generated) for sub-millisecond UBX message parsing.

---

## Phase 1 — Core Widget Hardening (done)

Table (sorting, filtering, typed cells, reordering, visibility, column flags), InputText (unlimited buffer, multiline, password, readOnly, numericOnly), Plotting (bar, scatter, heatmap, multi-series, axis labels, legends, candlestick color props), ProgressBar, ColorIndicator, Unicode font glyphs.

---

## Phase 1.5 — Early Adopter Features (done)

Tab close/reorder, ColorPicker, Histogram, PieChart, Table context menus, `onItemAction` callback.

---

## Phase 1.75 — Map Widget Integration

Stages 1–7 and Stage 10 complete: submodule plumbing, desktop activation, demo dashboard, tile-grid rendering, download pipeline, smooth panning, zoom, documentation.

### Stage 8 — Optimization & Resource Management

- [x] GPU texture eviction, VRAM budget (512-tile LRU), prefetching, request deduplication
- [ ] `TileCache` tuning: increase `maxEntries` via `TileCache::configure()` at MapView init (default 256 is low for tile-grid); expose `configure()` via NAPI for runtime tuning from JS; consider multi-zoom-level prefetching to warm cache for adjacent zoom levels

### Stage 9 — Map Overlays & u-center lite Integration

- [x] Pin marker overlay, route/polyline overlay
- [ ] Wire into u-center lite Position Tracking panel (Phase 3) — live position dot on map updated from NAV-PVT messages
- [ ] Map center follows GPS fix when in "follow" mode

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

### Documentation (xframes.dev) — done

Getting Started (intro, layout guide, fonts guide), Widget Catalog (layout, text, form controls, display, table, navigation, plots), Styling & Theming (style concepts, yoga layout, base drawing, theming guide), API Reference (event types, imperative handles, ImGui enums).

### Website & Positioning (xframes.dev)

- [x] Refocused homepage on TypeScript/React, multi-language to dedicated page, updated feature cards, consolidated intro.md
- [ ] Add a live screenshot or GIF of the Dashboard demo on the homepage

### create-xframes-node-app (done)

Demo template, pinned versions, post-scaffold instructions, bundled templates.

### Performance Story

- [ ] Publish benchmark: XFrames showcase vs Electron-based equivalent
- [ ] Metrics: startup time, memory footprint, CPU usage at idle, frame rate under load
- [ ] Include numbers in README and showcase repo

### Theming (done)

3 polished themes (Dark, Light, Ocean) with runtime switching.

---

## Phase 5 — WASM Build Modernization (done)

Migrated from emsdk 3.1.60 to 5.0.2. Replaced `-sUSE_WEBGPU=1` with `--use-port=emdawnwebgpu` (Dawn WebGPU). Updated imgui overlay port (1.90.6 → 1.92.6), migrated `imgui_renderer` (swap chain → surface, type renames, callback signatures), migrated `LoadTexture()` WASM path, migrated `main.cpp` embind entry point.

---

## Phase 6 — Canvas Widget (done)

Embedded [QuickJS-NG](https://github.com/quickjs-ng/quickjs) for scripted ImDrawList rendering. 15 bound drawing primitives (line, rect, circle, triangle, text, polyline, bezier, ngon, ellipse + filled variants + drawImage). Texture loading on both desktop (OpenGL, file read) and WASM (emscripten_fetch, WebGPU). React integration with `CanvasImperativeHandle` (setScript, setData, clear, loadTexture, unloadTexture, reloadTexture). 168 unit tests.

### Reference

- [QuickJS-NG](https://github.com/quickjs-ng/quickjs) — ES2023 embeddable JS engine (~1 MB, MIT)
- [quickjspp](https://github.com/ftk/quickjspp) — Header-only C++ wrapper for QuickJS
- [CANVAS.md](./CANVAS.md) — Original design document (draw commands approach, superseded by QuickJS embedding)
- [imgui-react-runtime](https://github.com/tmikov/imgui-react-runtime) — Prior art: React + ImGui + Hermes (validates the concept)

---

## Phase 7 — Canvas 2D API Wrapper (done)

Wrapped the 15 ImDrawList bindings in an HTML5 Canvas 2D-style API (`ctx.fillRect()`, `ctx.beginPath()`, `ctx.arc()`, etc.). JS shim stored as C++ raw string literal in `canvas2d_shim.h`, auto-evaluated in `InitQuickJS()`. 4 new C++ bindings (`drawConvexPolyFilled`, `__measureText`, `__pushClipRect`, `__popClipRect`). State machine (styles, transforms, paths, clips, dashes) runs in pure JS. Raw `drawXxx` functions remain available alongside `ctx`.

### Stages completed:

1. **Core State Machine + Basic Drawing** — `fillStyle`, `strokeStyle`, `lineWidth`, `globalAlpha`, `font`, `save()`/`restore()`, `fillRect()`, `strokeRect()`, `clearRect()`, `fillText()`, `ctx.canvas.width/height`
2. **Path API** — `beginPath()`, `moveTo()`, `lineTo()`, `closePath()`, `arc()` (tessellated), `bezierCurveTo()`, `quadraticCurveTo()`, `stroke()` → `drawPolyline`, `fill()` → `drawConvexPolyFilled` (convex only)
3. **Transform Stack** — 3x2 affine matrix, `translate()`, `rotate()`, `scale()`, `setTransform()`, `resetTransform()`, `getTransform()`, rotated rects emit transformed quads
4. **Text Measurement + Alignment** — `measureText()` via `ImFont::CalcTextSizeA()`, `textAlign` (left/center/right/start/end), `textBaseline` (top/middle/bottom/alphabetic)
5. **Dashed Lines, rect(), Clip** — `setLineDash()`/`getLineDash()`/`lineDashOffset`, `rect()` path method, `clip()` (AABB only) via `__pushClipRect`/`__popClipRect`
6. **Documentation** — CLAUDE.md updated with Canvas 2D shim section

---

## Low Priority

- [ ] Allow plain numbers for `padding` and `margin` (e.g. `padding: 8` as shorthand for `padding: { all: 8 }`)

---

## Non-Goals (for now)

- Additional language bindings beyond Node.js
- Mobile targets
- Accessibility (important, but not blocking the showcase)
- Full u-center 2 feature parity — this is a focused demo, not a product replacement
