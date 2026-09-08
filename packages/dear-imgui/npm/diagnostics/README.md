# Fabric lifecycle and streaming diagnostics

This is the first Phase 12 Stage 0 slice. The same React PlotBar/Table fixture runs
through the embedded Fabric renderer and either real native binding. It observes
existing publication and cleanup behavior; it does not fix lifecycle defects.
See the [engineering record](../../../../docs/engineering/fabric-baseline-2026-09.md)
for results, expected failures, and the next cleanup slice.

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

The fixture awaits React completion, imperative refs, populated native content,
matching hierarchy/Yoga ownership, keyed reorder, final table/series data, subtree
subject cleanup, and a newer submitted frame. It captures a populated screenshot
before streaming. Sleeps are only pacing/warm-up/idle intervals, not assertions.
The typed Table native test invokes actual sorting/filtering during ImGui frames.

Stress first removes the content subtree, then unmounts its root. This deliberately
distinguishes working recursive subtree cleanup from broken container-0 cleanup.
After the initial lifecycle warm-up it reports element, subject, Fiber and widget
registration deltas without resetting the renderer. Native direct queue stress
uses a real parent and separately proves its counts return to baseline.

Artifacts default to ignored `build/diagnostics/`: bounded bridge traces, process
logs, JSON semantic/timing results, and `fixture.png`. Browser JSON wraps the
shared report in a page status record. Native unit tests support Google Test XML.
The browser also saves webpack/page/browser logs; failures preserve the last
published state. Each state wait and process has a watchdog. Node follows the
existing binding's process-exit shutdown convention; the browser closes its
dedicated process and removes only its generated temporary profile. Neither
process exit nor OS memory reclamation is evidence that unmount cleanup works.

Expected failures execute the intended invariant and then require a specific
known defect signature. XPASS, a changed signature, timeout, or any unexpected
assertion fails the run. The C++ same-ID reparent characterization avoids drawing
a known dangling hierarchy and is distinct from React's cross-parent remount.
No crash-prone case runs inside the shared rendering process.

## Measurement contract

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
