# XFrames Roadmap

Last reviewed against repository history and implementation: 8 September 2026.

## Vision

Build [ubx-monitor](https://github.com/andreamancuso/ubx-monitor) as the flagship showcase for XFrames — proving that a React-driven, DOM-free, ImGui-based framework can replace Electron for real-time data-heavy desktop applications. The showcase is built on XFrames + [ubx-parser](https://www.npmjs.com/package/ubx-parser) for sub-millisecond UBX binary protocol parsing (315+ message types).

XFrames is focused on GPU-accelerated technical visualization rather than broad parity with conventional desktop or web UI frameworks. Plot, Table, Map, Canvas, streaming data, and cross-platform React integration are the primary product surface.

The strategic basis for this focus is documented in [XFrames and GPUIX: Technical and Strategic Assessment](docs/strategy/gpuix-comparison-2026-08.md). The proposed runtime work is specified in [Fabric-Compatible Runtime Hardening](docs/architecture/fabric-runtime-hardening.md).

## Next Milestone — Runtime Reliability & Measured Streaming Performance (in progress)

Deliver Phase 12 Stages 0–4: establish lifecycle tests, application-code CI, and performance baselines; make cleanup explicit; publish atomic Fabric transactions; then add revision-aware, invalidation-driven rendering. The first Stage 0 PlotBar/Table slice supplies lifecycle characterizations, opt-in native frame diagnostics, cross-runtime fixture commands, production baselines, and application-code CI configuration. Stage 1 adds explicit native destruction and lifetime cleanup. Stage 2 supplied the first shared native transaction path. Stage 3 now replaces that alpha structural API with prospective Fabric descriptions, one atomic final-tree publication and committed ownership. Its local acceptance audit, native/parity/lifetime gates and production comparisons are complete; hosted coverage for these changes remains unverified. Stage 4 scheduling and full Stage 0 benchmark coverage remain open.

See the [publication record](docs/engineering/fabric-publication-2026-09.md), [historical transaction record](docs/engineering/fabric-transactions-2026-09.md), [cleanup record](docs/engineering/fabric-cleanup-2026-09.md), [historical baseline](docs/engineering/fabric-baseline-2026-09.md), and [reproduction guide](packages/dear-imgui/npm/diagnostics/README.md). All ten lifetime defect IDs now execute passing gates, including abandonment and same-ID moves. Local verification and hosted coverage are recorded separately; consistently green hosted CI remains a milestone criterion.

The GPUIX assessment identifies bridge correctness, lifecycle discipline, automation, and performance evidence as gaps to close while concentrating product work on Plot, Table, Map, and Canvas. The August Fabric upgrade and screenshot smokes provide foundations for this work. End-to-end performance advantages still require measurement.

Use ubx-monitor as the application validation target, with its CNO quality-series panel and configurable UI update rates as bounded showcase work. Review the milestone against the acceptance criteria below before expanding into durable replay and comprehensive automation (Stages 5–6). General shell and rich-text work remain driven by demonstrated application needs; additional language bindings remain out of scope.

---

## Phases 1–1.5 — Core Widget Hardening & Early Adopter Features (done)

Table (sorting, filtering, typed cells, reordering, visibility, column flags, context menus), InputText, Plotting (bar, scatter, heatmap, histogram, pie, candlestick), ProgressBar (fixed: Render now uses Yoga layout width instead of ImGui's fill-available), ColorIndicator, Tab close/reorder, ColorPicker.

---

## Phase 1.75 — Map Widget Integration (done)

Stages 1–10 complete: submodule plumbing, desktop activation, demo dashboard, tile-grid rendering, download pipeline, smooth panning, zoom, GPU texture eviction (512-tile LRU), prefetching, overlays (markers, polylines, accuracy circles), ubx-monitor integration with live GPS tracking.

### Tile Cache Tuning

- [x] Increase the global `TileCache` to 1,024 entries on MapView's first imperative `render` operation via `TileCache::configure(1024, 3600000)`
- [ ] Expose cache configuration via NAPI for runtime tuning from JS

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

PlotBar multi-series support is implemented in XFrames (mirroring PlotLine's existing `series` architecture). The remaining work is the ubx-monitor panel rewrite.

#### PlotBar Multi-Series (XFrames, done)

PlotBar uses a `PlotBarSeries` struct and `std::vector<PlotBarSeries> m_series` (same pattern as `PlotLineSeries` in `plot_line.h`). Backward compatible — constructor creates default series[0].

- [x] `plot_bar.h` — Add `PlotBarSeries` struct (label + xValues/yValues vectors), replace flat `m_xValues`/`m_yValues` with `m_series` vector, add `AppendSeriesData(seriesIndex, x, y)` and `SetSeriesData(json)`, parse `series` prop in `makeWidget`
- [x] `plot_bar.cpp` — Loop `m_series` in `Render()` calling `ImPlot::PlotBars()` per series, add `"setSeriesData"` and `"appendSeriesData"` ops in `HandleInternalOp`, add `series` prop handling in `Patch()` (grow/shrink/relabel per PlotLine pattern)
- [x] `PlotBar.tsx` — Add `series` to destructured props and JSX, add `setSeriesData()` and `appendSeriesData()` to imperative handle
- [x] `types.ts` — Add `PlotBarSeriesDef` type (`{ label: string }`), add `series?: PlotBarSeriesDef[]` to PlotBar props
- [x] `widgetRegistrationService.ts` — Add `setPlotBarSeriesData(id, seriesData)` and `appendPlotBarSeriesData(id, seriesIndex, x, y)`
- [x] `ReactNativePrivateInterface.js` — Add `"series"` to `plot-bar` attribute list

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

- [ ] Measure and improve the data pipeline: serial port → native parser → JS → XFrames render loop, using the Phase 12 baseline and instrumentation
- [ ] Configurable update rates (throttle UI updates independently of message rate)
- [ ] Validate ubx-monitor against repeatable telemetry input and the Phase 12 benchmark harness; distinguish measured data-to-frame latency from presentation timing where available

---

## Phase 4 — Polish & Ecosystem

### Website (xframes.dev)

- [ ] Add a live screenshot or GIF of the Dashboard demo on the homepage

### Performance Story

Establish the internal baseline in Phase 12 Stage 0 and compare each runtime change against it; public comparisons follow reproducible measurements.

- [ ] Publish benchmark: XFrames showcase vs Electron-based equivalent
- [ ] Metrics: startup time, memory footprint, CPU usage at idle, frame rate under load, and p50/p95/p99 data-to-frame latency
- [ ] Include numbers in README and showcase repo

---

## Phases 5–10 — WASM Modernization, Canvas Widgets & Scripting (done)

WASM build migrated to emsdk 5.0.2 + Dawn WebGPU. Three canvas widget engines — JsCanvas (QuickJS-NG), LuaCanvas (Sol2), JanetCanvas (Janet) — each with 19 ImDrawList draw bindings, Canvas 2D API shim, texture pipeline, `setScript`/`setScriptFile`/`setData`/`onScriptError`, and full React integration. External script file loading (desktop `std::ifstream`, WASM `emscripten_fetch`). 200+ unit tests across all engines.

---

## Phase 11 — Performance Optimization (mechanical optimizations done)

Viewport culling, periodic idle waiting (`glfwWaitEventsTimeout`), scroll extent fixes, and the mechanical optimizations below are implemented. Desktop still wakes at least every `1 / 30` second, and Wasm still requests a 30 Hz loop. Phase 12 replaces these policies and measures their effect. Further style/layout representation changes remain deferred until profiling justifies them.

### Stage 1 — Render Thread Unblocking & Hot Path Deduplication (done)

- [x] Switch the 13 NAPI initialization/widget event callbacks from `BlockingCall` to `NonBlockingCall`; shutdown and screenshot callbacks also use nonblocking delivery
- [x] Cache `m_elements[id]` lookups in `RenderElementById`
- [x] Fix fall-through bugs in `HasStyle()`/`GetElementStyleParts()`

### Stage 2 — Layout & Style Optimization (partially done)

- [x] Guard `YGNodeCalculateLayout` with dirty and available-size checks
- [x] Cache `GetChildrenMaxBottom` result with invalidation
- [ ] Pre-parse `ElementStyleParts::styleDef` into typed C++ struct at init time (deferred pending profiling)
- [ ] Pass layout values (left/top/width/height) through the render call chain (deferred pending profiling; `Element::Render()` already reads its values once into locals)

### Stages 3–5 — Widget & Style Micro-Optimizations (done)

Table ColumnType enum, persistent filteredIndices with dirty flag, FormatNumberValue stack buffer, parseCSSColor JSON bypass + DrawContext color cache, canvas textureLookup moved to init, JsCanvas m_hasRenderFunc guard, canvas dimension update guards, PlotPieChart m_labelPtrs cache, ColorIndicator/Slider string-to-bool flags, Image single find(), GetCurrentWindow hoist, StyledWidget GetCustomColorsOrNull/GetCustomStyleVarsOrNull (single traversal replacing Has+Get), Table cell data find().

### Operation Queue & Bridge Efficiency (superseded by Phase 12)

Track the versioned operation envelope, batching of `setChildren`/`appendChild`, and Wasm scheduling in Phase 12. Preserve JSON initially for trace readability; replace it only if post-batching profiles justify a typed/binary representation.

---

## August 2026 — Fabric Embedding & Verification Foundations (done)

Implementation and verification record: [React Native Fabric Embedding](packages/dear-imgui/npm/FABRIC_EMBEDDING.md), last verified 28 August 2026.

- [x] Upgrade the embedded Fabric renderer to React Native 0.87.0 with React 19.2.3 and development/production renderer selection
- [x] Generate deterministic renderer snapshots and matching upstream helpers with source hashes, AST contract checks, and non-mutating verification
- [x] Add `fabric:verify` for snapshot checks, generator/tool typechecking, and host-contract tests
- [x] Consolidate common, Node, and Wasm into one npm workspace with one authoritative lockfile and coordinated React/common dependency boundaries
- [x] Add desktop PNG screenshot capture and screenshot-writer unit tests
- [x] Add minimal screenshot and full-App Node smoke harnesses; verify the full App with development and production renderers
- [x] Rebuild Wasm with Docker/Emscripten and add a headless WebGPU browser smoke harness with readiness, runtime-error checks, and screenshot output

These checks cover embedding compatibility, initialization, and screenshot capture. The later Phase 12 harness adds semantic widget assertions, lifecycle stress, observable-frame checks and application-code CI. Comprehensive input automation and transaction/revision synchronization remain planned below.

### Release & Build Follow-Through (planned)

- [ ] Update `create-xframes-node-app` from its React 18.3.1/XFrames 0.1.0 template and validate a fresh application outside the workspace against the coordinated React 19 package set
- [ ] Verify registry state and complete the coordinated common/Node/Wasm release as needed, preserving the common `^0.1.7` boundary and publishing common first
- [ ] Consolidate shared C++ source lists into reusable CMake targets to prevent drift across desktop, Node, tests, and Wasm builds

---

## Phase 12 — Fabric Runtime Hardening (Stage 1 cleanup implemented; Stage 0 coverage incomplete)

Detailed design: [Fabric-Compatible Runtime Hardening](docs/architecture/fabric-runtime-hardening.md).

This phase preserves the React Native Fabric reconciler and the RxJS/ReactivePlusPlus architecture. It introduces an XFrames-owned transaction boundary at Fabric's `completeRoot`, then uses that boundary for lifecycle correctness, render scheduling, observability, replay, and automation. Stages 0–4 form the next milestone; Stages 5–6 are follow-on work after the milestone review.

### Stage 0 — Lifecycle Characterization, CI & Performance Baseline

Establish reproducible current behavior and measurements before changing runtime semantics. The first PlotBar/Table slice extends the host-contract checks and screenshot smokes with executing expected failures; confirmed defects are not treated as fixed.

- [x] Add real development/production Fabric lifecycle tests with a fake native module
- [x] Add native element/hierarchy/Yoga/subject snapshots and JS Fiber/registration counts, with real native queue tests
- [x] Cover mount, unmount, deep deletion, reorder, keyed replacement, and React cross-parent remount; characterize same-native-ID reparenting separately
- [x] Cover rapid updates, abandoned Suspense work, Strict Mode, and event/imperative operations racing with deletion
- [x] Add application-code CI configuration for `fabric:verify`, package builds, VS2022/Linux native tests, and real Node/Mesa and Docker/Wasm/Chromium fixtures, with failure artifacts
- [x] Capture operation counts, UTF-8 bytes, `completeRoot` observations, and measurable data/native-state-to-frame intervals; document clock scope and coalescing without claiming presentation latency
- [x] Build a shared multi-series PlotBar/typed Table fixture with populated screenshots, 100,000 initial rows, focused native sorting/filtering tests, and 1,000-cycle lifecycle characterization
- [x] Record three-repeat production Windows Node/OpenGL and browser SwiftShader/WebGPU baselines at 20/60/120 Hz, including startup, idle behavior, available memory/CPU, frame counts, and p50/p95/p99/maximum observations
- [x] Record build/hardware/adapter/assets/workload metadata, explicit unavailable metrics, and proposed update-rate/latency targets
- [ ] Extend benchmark coverage to map pan/zoom/tile completion/overlays and telemetry canvas rendering
- [ ] Add hardware WebGPU and ubx-monitor application baselines; compare equivalent Electron/GPUIX implementations before claiming an end-to-end advantage

### Stage 1 — Explicit Cross-Runtime Cleanup

Initial cleanup can follow current native destruction. Final reparent-safe destruction depends on Stage 3's committed reachability calculation.

- [x] Add reverse native-ID/public-ID widget mappings
- [x] Drop and count events whose targets are no longer live
- [x] Return or emit destroyed IDs from native structural application
- [x] Include container-root destruction, including direct populated-root unmount and partial root removal
- [x] Remove destroyed IDs from `fiberNodesMap` and widget registrations idempotently
- [x] Convert the Stage 0 stress characterizations into passing cleanup invariants with zero count growth after warm-up

Both current-source runtimes passed 1,000 cycles in one renderer, alternating subtree removal and keyed replacement/direct populated unmount. Native elements, hierarchy entries, subjects, Fiber entries, forward/reverse mappings and registrations all have zero post-warm-up growth. See the [cleanup record](docs/engineering/fabric-cleanup-2026-09.md) for ordering, remaining defects and the validation matrix. This does not establish speculative-work isolation or reparent-safe destruction.

### Stage 2 — Versioned Native Transaction API

- [x] Define commit schema version 1, sequence, and surface identifiers
- [x] Add a common `ApplyCommit` path for Node and Wasm
- [x] Make current single-operation exports delegate to one-operation transactions during migration
- [x] Parse and validate the complete transaction before published state changes
- [x] Increment one native revision per successfully applied transaction

Historically delivered on surface 0 with native uint64 sequence/revision strings and explicit destroyed IDs. Full preflight prevented invalid prefixes from mutating live state; successful multi-operation batches retained per-operation visibility boundaries. All 343 native tests passed on Windows/Linux, 53 actual-binding transaction results matched, and both runtimes passed 1,000 cleanup cycles. Production overhead, measured regressions, compatibility exceptions and hosted evidence at that stage are in the [transaction record](docs/engineering/fabric-transactions-2026-09.md). Those revisions were not Fabric commit revisions or frame guarantees. The four defect gates remaining at Stage 2 are resolved by Stage 3 below.

### Stage 3 — One Atomic Batch per Fabric Commit

- [x] Stage prospective Fabric work without mutating the live native tree
- [x] Publish one structural transaction from `completeRoot`
- [x] Prevent rendering from observing intermediate transaction state
- [x] Calculate destruction from final reachability so reparenting is safe
- [x] Request rendering at publication scope using the existing backend policy, with no per-operation wakeups
- [x] Verify that abandoned React work never enters the live tree or native call trace

The [publication record](docs/engineering/fabric-publication-2026-09.md) records
schema-v2 replacement of the earlier alpha API, 343 native tests on Windows/Linux,
64 matching binding results, 21 Fabric scenarios in each renderer mode, and 1,000 ordinary lifecycle and native
same-ID move cycles per real runtime. Each ordinary cycle abandons prospective
work; all ten lifetime counters return to baseline. The structural workload
separately verifies 200 publications/calls and 40 bailouts from 240 React updates
per repetition. Desktop publication wakes GLFW once; Wasm retains its existing
frame loop. This does not deliver Stage 4 invalidation scheduling or a presented
frame acknowledgment. The local acceptance audit and three-repetition regular/
100,000-row production comparisons are complete, with measured regressions and
remaining timing limits recorded explicitly. These uncommitted changes have not
run on hosted CI; the last Stage 2 hosted Wasm fixture failed.

### Stage 4 — Invalidation-Driven Rendering and Instrumentation

- [ ] Add an invalidation generation and frame revision
- [ ] Audit commits, imperative operations, input, resources, animations, screenshots, and debug state as invalidation sources
- [ ] Replace desktop `1 / 30` timeout rendering with event/deadline scheduling
- [ ] Replace the 30 Hz Wasm policy with dirty/active `requestAnimationFrame` scheduling
- [ ] Correlate data receipt, Fabric commit, native apply, frame construction, submission, and presentation where available
- [ ] Report p50, p95, p99, maximum, live-object counts, idle frames avoided, and dropped-event counters
- [ ] Re-run the Stage 0 workloads and ubx-monitor telemetry scenario, reporting baseline comparisons and whether the declared update-rate and latency targets are met

### Milestone Review Gate — After Stages 0–4

- [ ] One native structural call and one native revision per accepted Fabric commit
- [ ] Repeated lifecycle tests return JavaScript mappings, widget registrations, native elements, hierarchy entries, and internal-operation subjects to their expected baseline
- [ ] Reparenting preserves the moved node and its native widget state; abandoned React work never changes published native state
- [ ] Idle rendering approaches zero while active interaction is not capped at 30 Hz, with no missed invalidation for asynchronous resources or animations
- [ ] Every accepted transaction can be correlated with a frame containing its revision, including transactions coalesced into the same frame
- [ ] VS2022, Linux, and Wasm application-code CI is consistently green
- [ ] Reproducible benchmarks meet the declared streaming targets and report remaining bottlenecks and measurement limitations
- [ ] ubx-monitor validates sustained real application use and demonstrates the delivery value of XFrames' Plot, Table, Map, and Canvas capabilities

Review these results against the [strategic continuation gates](docs/strategy/gpuix-comparison-2026-08.md#reassessment-gates). If core correctness or performance targets remain unmet, prioritize the measured gaps and reassess scope before expanding the framework. Comparative workloads must reflect equivalent implemented functionality; use an application-composed timeline or grid where a GPUIX comparison is appropriate.

### Stage 5 — Native Operation Recording and Replay (follow-on)

- [ ] Persist versioned committed transactions and imperative widget commands
- [ ] Record initial dimensions, scale, theme, fonts, assets, and logical time inputs
- [ ] Replay native visual state without React
- [ ] Add periodic full-state snapshots for seeking and recovery
- [ ] Use tolerant screenshot assertions plus semantic state and bounds

### Stage 6 — Automation (follow-on)

- [ ] Query by test ID and inspect native type, props, state, bounds, visibility, and revision
- [ ] Inject mouse, wheel, keyboard, text, focus, resize, and controlled-clock input
- [ ] Wait for a transaction, a frame containing a revision, or a stable frame
- [ ] Integrate screenshot capture with revision-aware test results
- [ ] Run representative Node and Wasm functional tests in application-code CI

### Additional Phase 12 Exit Criteria — After Stages 5–6

The Stages 0–4 milestone criteria continue to apply.

- [ ] A recorded representative session recreates the same native tree and widget state
- [ ] Automation can locate, interact with, wait for, and assert a rendered widget

---

## npm Publishing Fixes (done)

Removed broken `prebuild-install` script from `@xframes/node`, verified native addon loads from `dist/`, published 0.1.3 with all DLLs.

## API Cleanup — init() Refactor & onBeforeExit (done)

Refactored Node `init()` from 16 positional arguments to a single options object with named keys (`assetsBasePath`, `fontDefs`, `theme`, `onInit`, `onTextChange`, etc.). Added `onBeforeExit` callback — C++ calls it after the GLFW window closes and TSFNs are released, replacing `std::exit(0)`. JS controls shutdown (default: `process.exit(0)`), enabling cleanup (config persistence, etc.) before exit.

## Low Priority

- [ ] Allow plain numbers for `padding` and `margin` (e.g. `padding: 8` as shorthand for `padding: { all: 8 }`)
- [ ] Restructure `@xframes/node` into platform-specific packages (`@xframes/node-win32-x64`, etc.) following the esbuild/swc pattern — each user only downloads binaries for their platform

---

## Non-Goals (for now)

- Additional language bindings beyond Node.js
- Mobile targets
- Accessibility (important, but not blocking the showcase)
- Full u-center 2 feature parity — this is a focused demo, not a product replacement
