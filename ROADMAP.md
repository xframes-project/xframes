# XFrames Roadmap

## Vision

Build a lightweight u-center 2 alternative as the flagship showcase for XFrames — proving that a React-driven, DOM-free, ImGui-based framework can replace Electron for real-time data-heavy desktop applications. The showcase is built on XFrames + a native npm module wrapping [cc.ublox.generated](https://github.com/commschamp/cc.ublox.generated) for sub-millisecond UBX message parsing.

---

## Phase 1 — Core Widget Hardening

The current widget set needs to mature before it can support a real application. Priority is on the widgets the showcase will exercise hardest.

### Table

The Table widget today renders string-only cells with no interactivity beyond virtual scrolling. It needs:

- [ ] Column sorting (ascending/descending toggle, sort callback to JS)
- [ ] Column filtering / search (per-column text filter)
- [ ] Row selection with click event emitted to JS
- [ ] Typed cell values (numbers, booleans, icons — not just strings)
- [ ] Column reordering (`ImGuiTableFlags_Reorderable`)
- [ ] Column visibility toggles (`ImGuiTableFlags_Hideable`)
- [ ] Configurable column flags from JS (width mode, default sort, etc.)

### InputText

Currently limited to single-line, 100-character buffer, no masking. Needs:

- [ ] Configurable buffer size (expose from JS props)
- [ ] Multiline input (`ImGui::InputTextMultiline`)
- [ ] Password masking (`ImGuiInputTextFlags_Password`)
- [ ] Read-only mode (`ImGuiInputTextFlags_ReadOnly`)
- [ ] Numeric-only mode

### Plotting

PlotLine and PlotCandlestick exist. The showcase needs more chart types from ImPlot:

- [ ] Bar chart (`ImPlot::PlotBars`)
- [ ] Scatter plot (`ImPlot::PlotScatter`)
- [ ] Heatmap (`ImPlot::PlotHeatmap`) — useful for signal quality grids
- [ ] Multi-series support for PlotLine (multiple Y datasets sharing one X axis)
- [ ] Axis label configuration from JS
- [ ] Legend control
- [ ] Fix: wire PlotCandlestick `bullColor`/`bearColor` props through to C++ (currently ignored)

### Misc

- [ ] Increase PlotLine/PlotCandlestick data point cap (currently 6,000 — may need to be configurable)
- [ ] Progress bar widget
- [ ] Color indicator widget (for fix status, signal health)

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

### Documentation

- [ ] Widget catalog with code examples (interactive or static — at minimum a markdown reference)
- [ ] Getting-started guide beyond "Hello, world"
- [ ] API reference for TypeScript types and component props

### create-xframes-node-app

- [ ] Add a "demo" template option that scaffolds a meaningful example (table + plot + form)
- [ ] Pin and track latest package versions
- [ ] Print `npm start` instruction after scaffold completes
- [ ] Replace raw `curl` downloads with bundled templates for reliability

### Performance Story

- [ ] Publish benchmark: XFrames showcase vs Electron-based equivalent
- [ ] Metrics: startup time, memory footprint, CPU usage at idle, frame rate under load
- [ ] Include numbers in README and showcase repo

### Theming

- [ ] Ship 2-3 polished built-in themes (dark, light, high-contrast)
- [ ] Theme switching at runtime in the showcase app

---

## Non-Goals (for now)

- Additional language bindings beyond Node.js
- Mobile targets
- Accessibility (important, but not blocking the showcase)
- Full u-center 2 feature parity — this is a focused demo, not a product replacement
