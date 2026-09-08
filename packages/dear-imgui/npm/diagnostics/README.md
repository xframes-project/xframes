# Fabric lifecycle and streaming diagnostics

The shared React PlotBar/Table fixture runs through Fabric and both real native
bindings. Runs require all ten lifetime defect gates, prospective Fabric
publication, final-tree wire checks and native visibility tests. See the
[publication record](../../../../docs/engineering/fabric-publication-2026-09.md),
[historical transaction record](../../../../docs/engineering/fabric-transactions-2026-09.md),
[cleanup record](../../../../docs/engineering/fabric-cleanup-2026-09.md)
and the [historical Stage 0 baseline](../../../../docs/engineering/fabric-baseline-2026-09.md).

## Build from this checkout

Run JavaScript commands from `packages/dear-imgui/npm`, using its lockfile:

```sh
npm ci --ignore-scripts
npm run fabric:verify
npm run build:common
npm run diagnostics:typecheck
npm run test:lifecycle
```

`test:lifecycle` starts isolated development and production processes. Every case
creates a fresh adapter, renderer, registration service, and fake binding, then
stops the surface and unsubscribes the adapter. It exercises real React lifecycle
work; the fake tree is not evidence of native cleanup.

On Windows, use a VS2022 x64 developer terminal with CMake on PATH:

```sh
cmake -S ../cpp/tests -B ../cpp/tests/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build ../cpp/tests/build --target Google_Tests_run --parallel 4
../cpp/tests/build/Google_Tests_run.exe
npm exec --workspace @xframes/node -- cmake-js compile --generator "Visual Studio 17 2022" --arch x64 --parallel 4
npm run build:node
npm run copy-artifacts-to-lib-folder --workspace @xframes/node
```

On Linux, use GCC, Ninja, and the system dependencies listed in the
[application workflow](../../../../.github/workflows/application.yml). Configure
the same native tests, run `Google_Tests_run` without `.exe`, and use
`--generator Ninja` for the Node build. Do not reuse CMake build directories
between operating systems or compiler families.

Build optimized Wasm using the existing Docker path (Git Bash on Windows):

```sh
bash ../cpp/wasm/build-wasm-docker.sh
npm run build:wasm
```

The default explicitly sets `XFRAMES_FAST_BUILD=OFF`, including after a previous
`--fast` build. Wasm `--baseline` rejects a fast-build cache. Native fixture runs
load `node/build/Release/xframes.node`; browser runs load `wasm/src/lib/xframes.mjs`
and `.data`. Rebuild these from current C++ sources before integration runs. CI
always does so, caching dependencies rather than application binaries.

## Run

```sh
npm run diagnostics:node
npm run diagnostics:wasm
npm run diagnostics:node -- --baseline
npm run diagnostics:wasm -- --baseline
npm run diagnostics:node -- --stress
npm run diagnostics:wasm -- --stress
npm run diagnostics:node -- --baseline --extended
npm run diagnostics:wasm -- --baseline --extended
npm run diagnostics:report -- build/diagnostics/node/result.json build/diagnostics/wasm/result.json
npm run test:transactions:parity -- build/diagnostics/node/result.json build/diagnostics/wasm/result.json
```

Default: 1,000 initial rows, 128 retained points per series, 20/60/120 Hz inputs,
200 ms warm-up per rate, 1 second per rate, 500 ms idle, one repetition, and three
stress cycles. `--baseline` selects production React and three repetitions.
`--stress` runs 1,000 cycles in one process and renderer; `--extended` starts with
100,000 rows. Avoid combining stress and extended for routine CI. Streaming
appends one row and two series points per input; the initial table size grows
by the number of warm-up and measured samples.

Override parameters with a JSON `XFRAMES_DIAGNOSTICS_OPTIONS` environment variable.
For example, in PowerShell:

```powershell
$env:XFRAMES_DIAGNOSTICS_OPTIONS = '{"durationMs":5000,"warmupMs":1000,"idleMs":2000,"points":256}'
$env:XFRAMES_DIAGNOSTICS_DIR = 'build/diagnostics/node-baseline'
npm run diagnostics:node -- --baseline
```

In bash prefix the command with those variable assignments. Use separate output
directories for separate runs. Parameters are validated; the fixture bounds
cycles at 1,000 and initial rows/points at 100,000. For comparable performance,
close other benchmarks/builds, record adapter selection, and repeat on a quiet
machine. Timing is informational, not a universal CI threshold.

For browser runs set `XFRAMES_BROWSER` to an installed Edge/Chrome executable
when discovery is insufficient. The default is a headless SwiftShader WebGPU
adapter. `XFRAMES_WEBGPU_ADAPTER=default` selects the normal adapter. Report these
as separate environments. The harness requires port 3011 to be free so it can
build its own fixture with the selected options. Graphics initialization failure
is a failed integration run, never a rendering pass.

Existing full-App regressions remain available:

```sh
npm run smoke:app --workspace @xframes/node
npm run smoke:browser --workspace @xframes/wasm
```

## Assertions and artifacts

Both real runtime commands also execute the same version-1 transaction fixtures
after the ordinary lifecycle/streaming checks. They compare exact results and
error indexes, native ordering across direct batches and compatibility calls,
rejected-prefix immutability, populated PlotBar/Table state, guarded null prop
removal, Yoga ownership, disabled diagnostics and acknowledged destruction followed
by a newer frame. The parity command compares those actual Node/Wasm reports;
the fake Fabric harness is a separate dev/production gate. See the
[transaction record](../../../../docs/engineering/fabric-transactions-2026-09.md)
for the wire schema, compatibility exceptions and current-source validation.

The fixture awaits React completion, imperative refs, populated native content,
matching hierarchy/Yoga ownership, keyed reorder, final table/series data, subtree
subject cleanup, and a newer submitted frame. It captures a populated screenshot
before streaming. Sleeps are only pacing/warm-up/idle intervals, not assertions.
The typed Table native test invokes actual sorting/filtering during ImGui frames.

Stress alternates subtree removal followed by root unmount with keyed PlotBar
replacement followed by direct populated-root unmount. Each mounted fixture must
have 8 elements, 9 hierarchy entries, 2 internal subjects, 8 Fiber entries, 7
forward/reverse public-ID bindings, and 2 widget registrations. Its events and
imperative data updates must work. Every unmount must have zero elements,
subjects, Fiber entries, public bindings, native target records and registrations.
Container 0 retains exactly one empty hierarchy entry and has no Element/Yoga node.
Count deltas from the empty post-warm-up baseline must all be zero. No renderer
reset, GC, or process exit participates in these assertions.

Both runtimes also create, update, dispatch events and destroy with diagnostics
disabled, then enable snapshots to verify the result in a later frame. Browser
runs additionally exercise the ordinary Wasm wrapper through a real DOM root:
Strict Mode, updates, retained data, deferred events and populated unmount.

Artifacts default to ignored `build/diagnostics/`: bounded bridge traces, process
logs, JSON semantic/timing results, and `fixture.png`. Browser JSON wraps the
shared report in a page status record. Native unit tests support Google Test XML.
The browser also saves webpack/page/browser logs; failures preserve the last
published state. Each state wait and process has a watchdog. Node follows the
existing binding's process-exit shutdown convention; the browser closes its
dedicated process and removes only its generated temporary profile. Neither
process exit nor OS memory reclamation is evidence that unmount cleanup works.

All ten XF-LIFE defect IDs execute passing invariants. The Fabric harness covers
speculative host isolation, interleaved clones, callback-only updates, pending
Suspense callbacks and public IDs, reveal, abandonment, explicit failure, Strict
Mode and cleanup. Unexpected React commit errors fail the harness. Native tests
prove same-ID Element/Yoga/subject/data survival separately from React's
cross-parent remount, which creates a new lifetime. Historical Stage 0 report
comparison remains available; no expected-failure gate preserves the old defects.

## Lifetime ordering

Both bindings accept only schema-v2 final-tree publication through applyCommit.
The old setElement, patchElement, setChildren and appendChild exports are removed.
A matching baseRevision prevents stale writers; rootChildren and every reachable
node's child assignment describe the full result. Preflight and application hold
one visibility boundary shared with rendering and synchronous native readers.
The actual destroyedIds array follows previous-tree postorder, filtered to IDs
that do not survive in the final tree. Synchronous subject delivery and cleanup
finish before the acknowledgment leaves native tree locks.

The host installs committed mappings and immutable event-prop snapshots after
acknowledgment. React completion checks reject publication failure. The bridge
then invalidates all usable targets; native runtime failure quarantines partial
state. React may release an empty host tree after terminal failure, counted as
terminalTeardowns rather than a successful publication. That teardown never
reports native recovery or an acknowledged unmount. Disposal is distinct from
successful publication.

Events use committed ancestry and callbacks while speculative work is pending.
Wasm handlers enqueue at most 256 events per drain and dispatch in a microtask
after the native render callback returns. Dead, unknown, disposed and overflow
events are dropped and counted. The queue is cleared on disposal/failure.

Component imperative handles capture a native lifetime token at layout setup.
Deleted handles are no-ops with a saturating diagnostic counter; they never look
up a replacement by public ID. String-based service calls intentionally resolve
the current public binding. Ownership tokens and returned registration leases
must be retained for delayed work/cleanup. Public-ID changes modify mappings only;
effect cleanup releases its own registration lease, while native acknowledgment
invalidates the lifetime. Strict Mode setup/cleanup/setup preserves live handles.
Unrelated serialization and native errors propagate normally.

## Measurement contract

The structural React workload uses 40 cycles per repetition. Each cycle requests
mount, same-element bailout, prop update, reorder, keyed replacement and unmount:
240 requested updates produce 200 completeRoot publications and 40 bailouts.
Every accepted publication must make one actual structural call and advance the
native revision once. Reports include UTF-8 bytes, staging/diff/serialization,
boundary and native parse/validation/application/visibility-lock distributions.
Native validation includes reachability. No native presentation latency is inferred.

The direct wire fixture separately measures three repetitions of 100 publications,
each with four plot patches and full child assignments. It verifies removed-version
rejection, stale revisions, final ownership, moves and actual cleanup across both
bindings. The removed mutation engine is not retained as a benchmark mode; compare
against stored Stage 2 evidence. State queries are excluded from boundary totals.

Stress runs add a pending candidate with conflicting public ID and callback in
every ordinary lifecycle cycle, abandon it, and assert zero growth across the ten
original lifetime fields plus bridge-owned descriptions. After the acknowledged
empty Fabric publication, the bridge is disposed before direct wire fixtures
assume ownership. Native same-ID move cycles preserve populated PlotBar/Table data
and verify Yoga ownership in newer frames. --stress runs 1,000 ordinary cycles and
1,000 native move cycles in the same runtime process.

`getCommitState` exposes always-current sequence/revision with diagnostics off.

The Stage 4 working tree replaces numeric `frame` with decimal-string `frameId`,
`nativeRevision` and `coveredGeneration`. The always-readable `scheduler` reports
the live invalidation generation, completed coverage, submitted/constructed frame
counts, wake/opportunity counts and fixed activity/deadline reasons. Queries do
not request a frame. `observeNativeFrame` captures the target generation and
revision after an operation and accepts any submitted snapshot covering both;
several publications can share a frame. Unchanged or rejected state can already
be covered. Imperative sample assertions still verify the actual data, since a
structural revision alone cannot prove data inclusion. These fields describe
backend submission, not GPU completion or physical presentation.

`waitForNativeIdle` requires clean, renderable state with no active owner or real
deadline. Pending HTTP work can coexist with inactivity. Resource gates separately
wait for actual requests to start, prove inactivity, release deterministic local
responses, and require their completed upload/failure state in a covering frame.
`diagnostics/run.mjs` owns that server and copies the repository fonts and generated
test assets into the ignored report directory. The Node fixture uses local files
for Image/Canvas and controlled HTTP for Map; the browser uses controlled HTTP
for all three producers. No public tile service participates. `resourceState`
exposes live/retired widget textures, queued prefetch events and desktop worker
activity even when expensive snapshots are disabled. Current verification status
is recorded in [the invalidation record](../../../../docs/engineering/fabric-invalidation-2026-09.md).

QuickJS, Lua and Janet Canvas scripts default to continuous execution while
visible. `setContinuous(false)` lets static content settle; script/data/texture
changes still invalidate, and `redraw()` explicitly requests another frame.
Clipped or cleared scripts do not retain continuous activity. Map zoom uses a
finite 150-ms deadline; failed tiles settle until another view request permits
retry. Neither resource downloads nor focused/hovered widgets alone imply a
permanent render loop.
When enabled, its bounded `lastTransaction` is the last enabled sample and may
precede current counters after disabled calls. It is not a durable recorder or
a frame/revision correlation. The native subject retains a weak reference to one
request, not a completed batch or destruction payload.

- `dataToObservedFrameMs`: JS `performance.now()`, from imperative invocation to
  polling a submitted frame whose last series sample matches the input. Includes
  scheduling and poll delay; excludes transport before that invocation.
- `applyToConstructedMs`: C++ `steady_clock`, from the latest internal operation
  handler completion for the plot to draw-data construction. It is a widget-level
  state-age measurement, not per-command acknowledgment latency.
- `constructedAtMs` precedes backend submission. `submittedAtMs` is recorded after
  backend drawing/submission, before desktop buffer swap. Neither is presentation
  time. C++ absolute timestamps are never subtracted from JS timestamps.
- Inputs that are no longer the latest sample when polled are counted as coalesced.
  Some may have appeared in frames between polls. The fixture cannot distinguish
  those from inputs superseded before rendering. A final input missing its deadline
  fails; successful reports account for all inputs and have zero timed-out inputs.
- Distributions include sample count, nearest-rank p50/p95/p99/maximum. They cover
  observed samples only. Requested/achieved input rates are separate from frame
  counts and observed update rates; 120 input Hz does not imply 120 rendered Hz.
- Byte totals count UTF-8 string payloads passed to native methods, before trace
  truncation. They exclude numeric arguments, native allocations and transport
  framing. Complete-root enter/exit records observe publication order; they are
  not native operations or atomic transaction boundaries.
- Operation counts include the Stage 1 `isElementAlive` boundary calls. Streaming
  performs one liveness query per imperative operation; these numeric-only calls
  add no serialized JSON bytes. Historical Stage 0 reports did not have them.
- Node records process RSS and cumulative CPU; idle CPU is expressed as percent
  of one core. Browser total native/browser memory and CPU are unavailable from
  the page and explicitly null. A JS heap measurement would not substitute for RSS.
- Startup includes native-ready time from runner entry and lifecycle milestones
  from fixture entry. Imports/process launch before entry and physical first-pixel
  presentation are excluded. Native GPU metadata is sampled on the render thread.
- Diagnostics are disabled by default. Enabled snapshots retain one completed frame
  plus one frame under construction, cap element records at 4,096, and still count
  all native elements. The bridge retains 256 records with 2,048-character string
  prefixes; totals include dropped records. JS diagnostics copy IDs/counts, not
  Fiber graphs. Snapshot construction, serialization, tracing and polling add cost.

CI fails missing/invalid required observations, correctness changes, crashes, and
hangs. Hosted timing remains informational. The manual workflow's `extended`
input enables large-table and stress cases. Check the engineering record for
which environments actually ran; workflow configuration is not hosted-run proof.

## Stage 4 platform, resource lifetime and measurements

The complete fixture drives real text, held-key repeat, wheel zoom, minimize,
restore, exposure and idle close through a PID-scoped Windows helper or an owned
X11 display/window manager. Linux requires `xvfb`, `xdotool`, `openbox` and
`wmctrl`; run `bash diagnostics/x11-run.sh npm run diagnostics:node`. Browser
input/window state uses Chromium CDP. Native registrations, RAF/deadline handles,
visibility listeners and pending screenshots must return to zero on terminal
cleanup. A bounded DOM audit independently verifies native browser listeners.

`--stress` now runs 1,000 ordinary lifecycle/abandonment cycles and resource
removals, then 1,000 original and resource/activity same-ID moves. Each resource
cycle owns held HTTP completions, all three Canvas engines and texture work.
Acknowledged removal plus an empty covering frame must restore owner, texture,
event and platform counts; late completion cannot invalidate a deleted owner.
Successful runtime termination is a separate gate after those checks.

Use optimized current-source native artifacts and production React. The Node
full-App source entry loads `node/src/lib/xframes.node`; after a manual CMake
build, run `npm run copy-artifacts-to-lib-folder --workspace @xframes/node` before
its smoke. The diagnostics runner loads `node/build/Release/xframes.node`.

Historical comparison parameters remain exact and separate from extra tests:

```powershell
$env:XFRAMES_DIAGNOSTICS_OPTIONS='{"rows":1000,"points":128,"rates":[20,60,120],"durationMs":3000,"warmupMs":1000,"idleMs":2000,"cycles":0}'
npm run diagnostics:node -- --baseline
# Repeat with diagnostics:wasm, in a separate output directory.
$env:XFRAMES_DIAGNOSTICS_OPTIONS='{"rows":100000,"points":128,"rates":[20,60,120],"durationMs":1000,"warmupMs":200,"idleMs":1000,"cycles":0}'
npm run diagnostics:node -- --baseline --extended
# Additional idle/activity and diagnostics-off/on cost observation:
$env:XFRAMES_DIAGNOSTICS_OPTIONS='{"durationMs":1000,"warmupMs":200,"idleMs":10000,"cycles":0,"activityMs":10000}'
npm run diagnostics:node -- --baseline
```

Set `XFRAMES_DIAGNOSTICS_DIR` for every run and run measurement workloads
sequentially without task-owned builds. `activityMs` adds two separate intervals
of a visible moving-rectangle QuickJS Canvas, diagnostics off then on. Reports
distinguish submitted cadence, CPU, opportunities and inactive browser handles;
off/on order and host variation limit instrumentation-cost attribution. Streaming
stage timings cover three imperative calls, diagnostic query/parse, and native
constructed-to-submitted time. The last duration starts after draw construction
and snapshot collection and ends after backend submission; it excludes preceding
preparation/construction and is not GPU execution or presentation.

## Isolated ubx-monitor telemetry

Read the external checkout's `AGENTS.md`; clone it into an ignored validation
directory and install locally packed current-source `@xframes/common` and
`@xframes/node` with React 19.2.3. Keep its original settings and dependency graph
unchanged. The validation harness requires matching application React/native
packages, not the original React 18 published-package graph. The engineering
record lists the narrow style/config/typecheck migrations used by this checkout.

```powershell
$env:NODE_ENV='production'
$env:TSX_TSCONFIG_PATH='diagnostics/tsconfig.json'
$env:XFRAMES_UBX_APP_DIR='C:/path/to/isolated/ubx-monitor'
$env:XFRAMES_DIAGNOSTICS_DIR='C:/path/to/ignored/evidence'
node --import ./common/node_modules/tsx/dist/loader.mjs diagnostics/ubx-telemetry.ts
```

Synthetic checksummed NAV-SAT bytes pass through the actual SerialManager,
UbxParser, `useNavSat` and `SignalStrengthPanel`. Sustained four-band CNO updates,
idle/resume, populated capture and listener cleanup are asserted. No physical
serial device is opened; this evidence does not establish hardware validation.
