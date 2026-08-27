# XFrames and GPUIX: Technical and Strategic Assessment

- Status: decision record
- Assessment date: 28 August 2026
- Repositories assessed: XFrames at `C:\dev\xframes`, GPUIX at `C:\dev\gpuix`

## Executive decision

XFrames remains worth developing, provided its scope is focused.

GPUIX is currently the stronger implementation for a general React desktop application shell. Its bridge batching, native text and input support, lifecycle discipline, automation, tests, continuous integration, and implemented macOS backend are ahead of XFrames.

XFrames is currently the stronger implementation for technical and data-heavy applications. It already provides tables, seven plot families, maps, canvas scripting, and imperative high-rate data APIs that GPUIX does not provide. Replacing XFrames in an application such as `ubx-monitor` would require rebuilding the most valuable and differentiated parts of XFrames.

The resulting direction is:

> Develop XFrames as a GPU-accelerated technical visualization runtime for React and other bindings, rather than trying to compete broadly as a conventional desktop application framework.

Dear ImGui remains an appropriate foundation for operator consoles, telemetry, finance, monitoring, and engineering tools. It becomes a disadvantage when XFrames attempts to compete in rich text, accessibility, conventional application layout, or highly polished native shell behavior.

This decision does not call for replacing React Native Fabric, ReactiveX/RxJS, ReactivePlusPlus, Yoga, or Dear ImGui. It calls for hardening the relatively thin integration layer around them and concentrating product work on XFrames' specialized widgets.

## Important qualifications

### XFrames uses a battle-tested reconciler

XFrames does not use a newly invented React reconciler. It vendors the React Native 0.74.1 Fabric production renderer and provides its own `nativeFabricUIManager` implementation.

The reconciler itself is proven at React Native scale. The rough area is the XFrames host adapter, particularly its current node cloning, destruction, and operation publication behavior. The relevant implementation is [`nativeFabricUiManager.ts`](../../packages/dear-imgui/npm/common/src/lib/react-native/nativeFabricUiManager.ts).

It is therefore inaccurate to describe GPUIX as having a categorically better reconciler. A more precise comparison is:

- GPUIX currently has the cleaner custom host bridge.
- XFrames starts from the more battle-tested reconciliation machinery.
- XFrames can harden its bridge without replacing its reconciler architecture.

### XFrames is reactive on both sides of the boundary

The JavaScript side uses RxJS for native event delivery. The C++ side uses ReactivePlusPlus serialized replay subjects for element and widget operation delivery. The native retained tree, hierarchy, and Yoga state remain the authoritative render state.

This provides useful architectural leverage:

- Commit operations can be grouped and published atomically.
- Transactions can be observed for instrumentation.
- Committed operations can be recorded and replayed.
- High-rate widget operations can be ordered, coalesced, or subjected to back-pressure policies.
- Rendering, diagnostics, recording, and test synchronization can consume the same transaction identifiers.

GPUIX's one-batch-per-commit implementation is a useful pattern, not a capability that XFrames lacks.

### macOS is unvalidated rather than architecturally unsupported

XFrames has not yet been built and validated on macOS because the maintainer does not have access to Mac hardware. Its principal technologies—GLFW, OpenGL, Dear ImGui, Yoga, CMake, and Node-API—are already proven on macOS.

The likely porting work is in the last mile:

- Application and event-loop integration.
- Retina scaling and multi-display behavior.
- Keyboard layouts, shortcuts, clipboard, and IME behavior.
- Native package production, signing, notarization, and universal binaries.
- CI and artifact distribution.

The core renderer is technically portable with high confidence, but macOS support must not be claimed as complete until it is built and exercised. In comparisons, XFrames should be marked **unvalidated on macOS**, not fundamentally incompatible with it.

### GPUIX's Zed fork is a strategic risk

GPUIX does not merely consume a small, independently versioned GPUI package. Its `zed` submodule tracks the `gpuix` branch of `remorses/zed`, and its project instructions explicitly make that fork part of the GPUIX implementation boundary. Missing GPUI behavior is expected to be fixed in the fork before the GPUIX submodule is advanced.

At assessment time, the GPUIX-pinned fork commit was 35 commits ahead of and 32 commits behind upstream Zed `main`. The fork was active and received four additional commits shortly after the commit pinned by the GPUIX checkout. This is not evidence of abandonment; it is evidence of an ongoing integration obligation.

The principal failure mode is likely to be cumulative maintenance drag rather than sudden failure:

- Upstream GPUI continues changing for Zed's requirements.
- GPUIX-specific patches continue accumulating.
- Toolchain and API updates require repeated merging and adaptation.
- New contributors must understand both GPUIX and a large editor fork.
- Releases become coupled to successful integration of two rapidly changing codebases.

The fork remains manageable if its delta stays narrow, changes are upstreamed, and GPUI eventually becomes an independently versioned stable dependency. It becomes an existential risk if the private patch surface grows continuously.

Dependency count alone is not the correct comparison. XFrames has many submodules, but most have bounded roles and do not carry large private forks. One central, fast-moving fork can present more strategic risk than many ordinary libraries.

## Assessment method and limitations

The assessment included:

- Repository history, contributor concentration, tags, package metadata, and current activity.
- React-to-native data flow and reconciliation boundaries.
- Native retained-tree ownership and render traversal.
- Layout, styling, events, input, text, scrolling, virtualization, and custom elements.
- Browser and native platform implementations.
- Tests, CI, automation, benchmarks, and documentation.
- Packaging, dependency topology, build requirements, and maintenance model.
- XFrames' existing local work, including the in-progress screenshot implementation.

GPUIX could not be rebuilt in the assessment environment because the freshly cloned `zed` submodule was uninitialized and Bun, Cargo, and Rust were not installed. Its source was inspected in full where relevant, and the exact GPUIX root commit in the checkout had a successful multi-platform GitHub Actions run.

XFrames was assessed with Visual Studio 17/2022 as the supported Windows toolchain. TypeScript compilation succeeded. The C++ test target configured and compiled through the link stage; the current local screenshot work was not present in the test target's duplicated source list, which prevented the final link. That is a build-manifest issue in work in progress, not evidence that the existing test sources fail to compile.

No source files were changed as part of the assessment.

## Repository and adoption snapshot

These figures are volatile signals, not proof of production use.

| Measure, 27 August 2026 | GPUIX | XFrames |
| --- | ---: | ---: |
| Current package line | `0.5.1` | npm registry `0.1.13`; assessed checkout `0.1.12` |
| Repository commits | 267 | 342 |
| Dominant-author commits | 251 | Effectively single-maintainer |
| GitHub stars | 1,286 | 21 |
| GitHub forks | 34 | 0 |
| Open GitHub issues | 8 | 16 |
| Recent npm downloads | 1,149 for `@gpuix/react` | 71 for `@xframes/node` |

GPUIX's activity was extremely bursty: 204 of its 267 commits landed during August 2026. That demonstrates energy and rapid execution, but also means APIs and documentation are moving quickly. At least one browser-event statement in the README had already become stale relative to the current implementation.

Both projects currently have a bus factor close to one. GPUIX additionally instructs external contributors to open an issue and wait for approval before opening a pull request; unsolicited pull requests are closed.

Public sources:

- [GPUIX repository](https://github.com/remorses/gpuix)
- [XFrames repository](https://github.com/xframes-project/xframes)
- [GPUI README and stability status](https://github.com/zed-industries/zed/blob/main/crates/gpui/README.md)
- [GPUIX CI](https://github.com/remorses/gpuix/actions)
- [`@gpuix/react` on npm](https://www.npmjs.com/package/@gpuix/react)
- [`@xframes/node` on npm](https://www.npmjs.com/package/@xframes/node)

## Architecture comparison

### GPUIX

GPUIX uses React's `react-reconciler` package with a custom TypeScript host configuration. Current GPUIX does not serialize an entire UI tree on every update.

Host nodes are described in JavaScript during React work and materialized only from accepted commit paths. Mutations are accumulated in JavaScript and sent through one `JSON.stringify` and one native `applyBatch` call per React commit. Rust parses the typed operation stream, interns styles, and mutates a retained tree of numeric-ID nodes.

The Rust retained tree stores parent/child relationships, styles, event registrations, props, and custom element state. Each GPUI frame recursively constructs ephemeral GPUI elements from this retained state. The tree is therefore retained at the GPUIX boundary while GPUI element construction remains frame-oriented.

Strengths include:

- One native boundary crossing per React commit.
- Better protection against abandoned concurrent React work.
- Explicit subtree destruction and event-handler cleanup.
- Rust-side style interning and sweeping.
- Strong focus, scroll, motion, and custom-element cache pruning.

Risks include:

- A central `renderer.rs` of approximately 5,651 lines.
- A tree lock held during broad render construction work.
- Global or singleton assumptions and no multiple-window support.
- Native custom elements requiring Rust changes and package rebuilding.
- Dependence on the forked Zed/GPUI implementation.

Assessed GPUIX source revision: [`64241ce`](https://github.com/remorses/gpuix/tree/64241ce81f7429c83c9b52bfe2c7367fcfd873e4).

### XFrames

XFrames uses the React Native Fabric production renderer with a custom JavaScript `nativeFabricUIManager`. Fabric calls `createNode`, clone methods, child-set methods, and `completeRoot`; the adapter translates those operations to Node-API or Emscripten calls.

C++ maintains:

- An element registry owning `unique_ptr<Element>` instances.
- A parent/child hierarchy.
- Yoga nodes for retained flexbox layout.
- ReactivePlusPlus subjects for structural and widget-internal operations.
- Dear ImGui/ImPlot state and platform GPU resources.

Each frame, XFrames calculates Yoga layout and traverses the retained hierarchy, issuing Dear ImGui and ImPlot commands. This is also a hybrid architecture: retained React, element, hierarchy, and layout state feeding an immediate-mode rendering API.

The current bridge sends individual JSON calls for create, patch, and child operations. It does not yet publish a single coherent native transaction at Fabric's `completeRoot` boundary. Native mutations can therefore become visible individually, and parsing/boundary overhead scales with the number of operations.

The correct conclusion is not that XFrames chose the wrong reconciler. It is that XFrames has not yet fully mirrored Fabric's separation between speculative shadow-tree construction and committed mounting transactions.

## Capability comparison

| Area | GPUIX | XFrames | Current lead |
| --- | --- | --- | --- |
| Reconciliation foundation | Custom host over `react-reconciler` | React Native Fabric production renderer | XFrames foundation; GPUIX bridge implementation |
| Commit bridge | One typed batch per accepted commit | Separate JSON/native calls | GPUIX |
| Reactive architecture | Event callback and retained-tree mutation system | RxJS plus ReactivePlusPlus on opposite sides | XFrames has more leverage than its current bridge uses |
| General application shell | Strong focus, overlays, motion, scrolling, virtual lists | Basic controls and Dear ImGui-oriented behavior | GPUIX |
| Technical visualization | No general plots, data grid, map, or canvas | Tables, plots, map, and three scripted canvas engines | XFrames |
| Text and input | Selection, IME, clipboard, undo/redo, grapheme handling, search | Dear ImGui-based text and input controls | GPUIX |
| Rich content | Native code, diff, GitHub-flavored Markdown | Basic text and scripted drawing | GPUIX |
| High-rate domain data | Virtual lists and application-level culling | Imperative append and batch APIs for plots, tables, maps, and canvas | XFrames API model; performance still needs proof |
| Layout and style | Native flex-style subset with inheritance and motion | Yoga plus Dear ImGui styling | GPUIX is more coherent for conventional UI |
| Browser | WebGPU/WebGL2, new event support, shared-memory Wasm | More established WebGPU Wasm and functional widget events | XFrames today |
| Native platforms | macOS, Windows, Linux | Windows and Linux; macOS not yet validated | GPUIX implementation coverage |
| Accessibility | No meaningful GPUIX semantics layer found | Explicitly not implemented | Neither |
| Native extensibility | Rust custom element plus rebuild | C++ widget plus bindings | XFrames for specialized widgets |
| Multiple windows | Not implemented | Effectively single-window application runtime | Neither |
| Canvas/custom drawing | Planned, not implemented | Canvas 2D-style APIs through QuickJS, Lua, and Janet | XFrames |
| Automation | Locator APIs, input injection, deterministic clock, screenshots | Screenshot support in progress; no full automation layer | GPUIX |
| Performance evidence | Concrete serialization and example frame budgets | No published end-to-end benchmark; render loop currently capped at 30 Hz | GPUIX |

## Detailed strengths of GPUIX

### Text, input, and content

GPUIX implements native input and textarea behavior including caret movement, selection, IME, clipboard, undo/redo, keyboard navigation, focus, and grapheme-aware deletion. It also supports selection across text elements, text search and highlighting, syntax-highlighted code, diffs, and GitHub-flavored Markdown.

This is the clearest example of a domain that XFrames should not attempt to reproduce without a concrete application requirement.

### Virtualization and scrolling

GPUIX provides a variable-height virtual list with anchoring and follow-tail behavior. In children mode, React and Rust retain all rows while only visible GPUI elements are built. An item-count/window-start mode also permits application-level React windowing.

The implementation is not unlimited: children mode still touches or probes every child per frame and nested scrolling is unsupported. Extremely large datasets require application-owned windowing.

### Testing and automation

Static source counts found approximately 201 Rust test attributes and 357 TypeScript test declarations. GPUIX exercises events, styles, highlighting, code, Markdown, selection, wrapping, virtual lists, inputs, lifecycle behavior, examples, and performance cases.

Its test client provides Playwright-like locators, clicking, filling, key presses, dragging, wheel input, screenshots, bounds, painted text, and deterministic motion timing. GPU-backed renderer tests run on Windows and macOS. Linux headless GPU testing was not present at assessment time.

### Performance work

GPUIX documents a realistic chat serialization workload. For a 10,000-turn case it reports approximately 221,764 operations, 72,010 elements, and 13.05 MB of JSON. Its documented optimizations reduced parse-and-apply time from 127.1 ms to 30.1 ms, allocation churn from approximately 900.5 MB to 104 MB, and retained-tree memory from 224.5 MB to 42.6 MB.

The benchmarks are project-authored rather than independent, and several budgets are hardware-specific. Nevertheless, they are substantially better evidence than XFrames currently provides.

### Platform implementation

GPUIX uses Metal on macOS, Direct3D on Windows, and Vulkan/wgpu on Linux. Published `0.5.1` native packages included macOS ARM64/x64, Linux x64/ARM64, and Windows x64/ARM64 variants. The source CI matrix for the next release had narrowed to macOS ARM64, Linux x64, and Windows x64, so the precise supported artifact matrix was in flux.

The browser build uses Wasm plus WebGPU/WebGL2 and requires a nightly Rust toolchain, shared-memory Wasm, and cross-origin isolation headers. The README describes an approximately 19 MB Wasm module. Browser callbacks had recently landed, making this area promising but very new.

## Detailed strengths of XFrames

### Specialized widgets

XFrames already implements functionality absent from GPUIX:

- Table sorting, filtering, selection, reordering, visibility controls, typed fields, and context menus.
- Line, bar, scatter, heatmap, histogram, pie, and candlestick plots.
- OSM tile rendering, caches, markers, polylines, accuracy ellipses, and prefetching.
- Canvas 2D-style rendering backed by Dear ImGui draw lists.
- QuickJS, Lua, and Janet canvas runtimes.
- Imperative batch and append APIs intended for streaming data.

This is the product moat. GPUIX cannot replace it without a substantial custom drawing and widget effort.

### Domain fit

XFrames is well aligned with `ubx-monitor`: real-time protocol data, tables, signal plots, position plots, sky views, maps, and high-rate append operations. GPUIX is better suited to the surrounding conventional shell, but it cannot currently embed or replace the technical content without a rewrite.

A near-term hybrid containing both UI runtimes in one window is not recommended. They have different surface ownership, render loops, input systems, and no mature cross-runtime embedding contract. Backend-neutral domain models are a better future integration seam.

### Dependency sustainability

XFrames' dependency graph is broad, and its duplicated build manifests create maintenance work. However, its central dependencies have bounded responsibilities, and XFrames owns its native integration layer. It is not currently coupled to a large private fork of another application.

This may make XFrames' underlying architecture more sustainable for its intended technical-visualization domain even though GPUIX's current implementation is more polished.

## Current XFrames weaknesses

### Non-atomic bridge operations

Fabric construction operations currently cross the JavaScript/native boundary separately. This creates avoidable JSON parsing, boundary traffic, and the possibility of the renderer observing a partially applied commit.

Fabric's `completeRoot` should become the publication boundary for a versioned XFrames-owned transaction. The design is specified in [Fabric Runtime Hardening](../architecture/fabric-runtime-hardening.md).

### Incomplete cross-runtime destruction

C++ recursively removes native subtrees and associated native resources. The JavaScript `fiberNodesMap` has no corresponding deletion path, and widget ID registrations are not consistently unlinked by component cleanup.

Cleanup should be derived from the final committed tree. A native commit result should identify destroyed IDs so JavaScript mappings and registrations can be removed explicitly.

### Fixed 30 Hz rendering

Desktop uses `glfwWaitEventsTimeout(1.0 / 30.0)`, while the Emscripten macro requests a 30 Hz main loop. This limits response and animation cadence while still waking and rendering during idle periods.

The existing calls to `glfwPostEmptyEvent()` after native operations already provide part of the required invalidation mechanism. XFrames should use an event/deadline-driven scheduler that sleeps indefinitely while idle and renders at display cadence while interactive or animated.

### Test distribution

XFrames has approximately 319 C++ test cases, but they are concentrated in Yoga, QuickJS, Lua, Janet, and canvas behavior. There is little coverage of:

- Fabric commit lifecycle.
- Reparenting and keyed replacement.
- Cross-runtime destruction.
- Event dispatch after deletion.
- Repeated mount/unmount stress.
- Renderer and screenshot integration.
- Node/Wasm parity.

### CI and build topology

The public repository did not have application-code CI at assessment time; its GitHub Actions history was limited to Pages deployment. Core C++ sources are repeated across desktop, Node, tests, and Wasm manifests. The current screenshot work compiling in one target but missing from the test target is a concrete example of this risk.

### Scope and package drift

The repository contains many language bindings and three scripting runtimes. That breadth is impressive but competes with hardening the core runtime. Package versions and examples have also drifted from one another.

New generic widgets, languages, and scripting engines should be deferred until the transaction, lifecycle, render scheduling, CI, and performance foundations are measurable and reliable.

### Accessibility

Neither project currently solves accessibility. XFrames must not market itself as accessible until it has a native semantics tree, platform adapters, keyboard navigation behavior, and automated accessibility checks.

## Strategic focus

### Primary product surface

XFrames should prioritize:

- Plot, Table, Map, and Canvas.
- Streaming-friendly native data storage and update APIs.
- Technical interaction patterns: crosshairs, annotations, filters, selection, tooltips, and data inspection.
- Desktop and browser rendering from the same application model.
- Backend-neutral domain data models where doing so does not compromise performance.

### Secondary product surface

Develop only when demanded by a real application:

- Conventional form controls.
- Rich text and Markdown.
- General animation systems.
- Application menus and shell conventions.
- Multiple windows.

### Explicit non-decisions

This assessment does not recommend:

- Replacing Fabric with a new custom reconciler.
- Removing RxJS or ReactivePlusPlus.
- Replacing Dear ImGui or Yoga.
- Porting XFrames wholesale to GPUI/GPUIX.
- Embedding GPUIX and XFrames into the same native window in the near term.
- Claiming GPUIX will inevitably fail.
- Claiming macOS support before it is built and validated.

## Immediate engineering priorities

1. Add lifecycle integration tests around the current Fabric adapter.
2. Make JavaScript/native destruction and registration cleanup explicit.
3. Introduce a versioned atomic commit batch at `completeRoot`.
4. Replace the fixed 30 Hz loop with event/deadline-driven invalidation.
5. Instrument commit-to-frame and data-to-frame latency.
6. Persist and replay committed native transactions.
7. Extend the screenshot work into query and input automation.
8. Consolidate core CMake targets and add VS2022, Linux, and Wasm CI.
9. Build the benchmark harness already listed in the roadmap.

## Benchmarks required for a continuation decision

The benchmark suite should measure startup, resident memory, CPU at idle, and p50/p95/p99 byte-to-pixel latency for:

- A rapidly updating multi-series plot.
- A 100,000-row sortable and filterable table.
- Map pan, zoom, tile completion, and overlays.
- Canvas rendering under a representative telemetry workload.
- 20 Hz, 60 Hz, and 120 Hz input streams.
- Windows native and browser builds.

Where a GPUIX comparison is useful, it should use a representative application-composed timeline or grid rather than pretending GPUIX already has equivalent plot and map widgets.

## Reassessment gates

After a focused hardening cycle, continue active XFrames development when the following are true:

- Repeated mount, unmount, reparent, and keyed replacement tests retain no stale JavaScript or native nodes.
- VS2022, Linux, and Wasm application-code CI is consistently green.
- Streaming views sustain documented target update rates with acceptable p95 and p99 latency.
- XFrames materially shortens development of at least one real technical application.
- Plot, Table, Map, and Canvas remain substantially ahead of composing equivalent functionality in a general GPU UI runtime.

Consider freezing the general framework layer or moving conventional application work to another runtime when:

- Actual users primarily require a conventional desktop shell rather than technical visualization.
- The specialized widgets no longer provide a material delivery or performance advantage.
- Core lifecycle and performance gates cannot be met in a focused development cycle.
- Another runtime gains a stable custom drawing API and equivalent production-quality technical widgets.

## Final judgment

GPUIX's rapid progress is a reason to sharpen XFrames, not a reason to abandon it.

GPUIX has the more polished general application implementation today. XFrames has a proven Fabric reconciler, reactive bindings on both sides, control of its native abstraction, and a differentiated technical-widget surface. Those are credible foundations for a sustainable project if development is concentrated on bridge correctness, runtime observability, and technical visualization rather than broad framework parity.
