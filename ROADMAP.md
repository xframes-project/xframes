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
- [ ] Expand intro.md beyond "Hello, world" — explain `render()` args (assets path, fontDefs, theme), root Node pattern, basic layout
- [ ] Layout guide — Yoga flexbox basics (row/column, flex, padding/margin objects, percentage widths), scroll containers (`overflow: "scroll"`, `flexShrink: 0`)
- [ ] Fonts guide — fontDefs shape, available font sizes, `font: { name, size }` in style rules, Font Awesome icons

#### Widget Catalog (grouped doc pages with props, events, imperative handles, code examples)
- [ ] Layout components — Node, Child, Group, DIWindow, CollapsingHeader, ItemTooltip, TextWrap
- [ ] Text components — UnformattedText, BulletText, DisabledText, SeparatorText, Separator, ClippedMultiLineTextRenderer
- [ ] Form controls — Button, Checkbox, Combo, InputText (multiline/password/readOnly/numericOnly), Slider, MultiSlider, ColorPicker
- [ ] Display components — ColorIndicator (shapes), ProgressBar, Image
- [ ] Table — columns config, typed cells (string/number/boolean), sorting, filtering (text/boolean/number), row selection, column reordering/visibility, context menus, imperative handle
- [ ] Navigation — TabBar (reorderable), TabItem (closeable), TreeNode, TreeView
- [ ] Plots — PlotLine (multi-series, streaming), PlotBar, PlotScatter, PlotHeatmap, PlotHistogram, PlotPieChart, PlotCandlestick; cover data models, imperative handles, axis/legend config

#### Styling & Theming
- [ ] Flesh out general-styling-concepts.md — style/hoverStyle/activeStyle/disabledStyle, RWStyleSheet.create(), style composition
- [ ] Flesh out yoga-layout-properties.md — add usage examples and gotchas (padding/margin objects, not plain numbers)
- [ ] Flesh out base-drawing-style-properties.md — backgroundColor, border, rounding with examples
- [ ] Theming guide — built-in themes (Dark/Light/Ocean), XFramesStyleForPatching type, ImGuiCol enum, runtime switching via patchStyle

#### API Reference
- [ ] Event types reference — CheckboxChangeEvent, InputTextChangeEvent, ComboChangeEvent, SliderChangeEvent, MultiSliderChangeEvent, TabItemChangeEvent, TableSortEvent, TableFilterEvent, TableRowClickEvent, TableItemActionEvent
- [ ] Imperative handle reference — TableImperativeHandle, PlotLineImperativeHandle, PlotBarImperativeHandle, PlotScatterImperativeHandle, PlotHeatmapImperativeHandle, PlotHistogramImperativeHandle, PlotPieChartImperativeHandle, PlotCandlestickImperativeHandle, ComboImperativeHandle, InputTextImperativeHandle
- [ ] ImGui enums reference — ImGuiCol, ImGuiStyleVar, ImGuiDir, ImPlotMarker, ImPlotScale, ImPlotColormap

### Website & Positioning (xframes.dev)
- [ ] Refocus homepage messaging on TypeScript/React — lead with the primary supported stack rather than 20+ language logos
- [ ] Move multi-language FFI showcase to a dedicated "Language Bindings" page (experimental/community status) instead of homepage hero
- [ ] Update homepage feature cards to highlight concrete developer benefits: React components, Yoga layout, live theming, ImPlot charts
- [ ] Add a live screenshot or GIF of the Dashboard demo on the homepage
- [ ] Consolidate intro.md to focus on the Node.js/TypeScript getting-started path; move other languages to a separate "Other Languages" doc page with appropriate "experimental" labels

### create-xframes-node-app

- [x] Add a "demo" template option that scaffolds a meaningful example (table + plot + form)
- [ ] Pin and track latest package versions
- [ ] Print `npm start` instruction after scaffold completes
- [ ] Replace raw `curl` downloads with bundled templates for reliability

### Performance Story

- [ ] Publish benchmark: XFrames showcase vs Electron-based equivalent
- [ ] Metrics: startup time, memory footprint, CPU usage at idle, frame rate under load
- [ ] Include numbers in README and showcase repo

### Theming

- [x] Ship 3 polished built-in themes (Dark, Light, Ocean) with accent colors
- [x] Theme switching at runtime in the showcase app

---

## Non-Goals (for now)

- Additional language bindings beyond Node.js
- Mobile targets
- Accessibility (important, but not blocking the showcase)
- Full u-center 2 feature parity — this is a focused demo, not a product replacement
