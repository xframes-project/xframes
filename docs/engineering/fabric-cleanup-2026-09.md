# Fabric Stage 1 lifetime cleanup

Status: Stage 1 implemented and locally validated; hosted cleanup changes have not run.
This follows the [historical Stage 0 baseline](fabric-baseline-2026-09.md), which
measured growth of 2,000 native elements, 8,000 Fiber entries and 2,000 widget
registrations per 1,000 ordinary lifecycle cycles. It does not complete Phase 12
Stage 0 coverage or the Stages 0–4 milestone.

## Lifetime contract

Node and Wasm `setChildren(parentId, childrenJson)` now return the same JSON array
of actual destroyed native IDs. `QueueSetChildren` still uses the existing
ReactivePlusPlus serialized subject. Inspection of `subject_state.hpp` confirms
that delivery runs synchronously under its serialization mutex. The handler
collects descendant IDs during destruction, returns after releasing hierarchy
and element locks, and fills the caller-owned result before `on_next` returns.
The subject's bounded replay records keep a weak result reference, so historical
operations do not retain acknowledgment payloads. No callback, cross-thread wait,
notification subscription, polling loop or initialization argument was added.
Existing direct native callers can ignore the return value.

Virtual container 0 owns its child list without owning an Element or Yoga node.
Replacing/removing its children now invokes the same recursive destruction path
as ordinary parents. A result includes each actual element destruction once, in
descendant-first order. Repeated empty unmounts return `[]`. Subject, hierarchy,
Yoga, diagnostic per-element data and widget destructors retain their existing
ownership. Ordinary sibling reorder retains the same widget objects and data.
Stale child-list reads and operations against missing parents no longer create
hierarchy metadata.

Internal-op subject lookup copies ownership under the element mutex and releases
the mutex before subject delivery. The handler verifies the current subject's
weak owner, protecting a replacement from delivery through an old copied subject.
Missing targets do not create subjects or elements.

The adapter consumes destruction results immediately and cleans Fiber entries,
native target records, public mappings and effect registrations. Native-to-public
lookup is direct, so cleanup does not scan every widget. Native numeric identity
is immutable across creation, prop cloning and patching; public ID addition,
change, removal and reuse only change bindings. Ownership tokens invalidate saved
handles by native lifetime. There is no retained collection of dead IDs.

Every imperative component uses shared registration setup/cleanup, including all
plots, Table, Map, Image, clipped text, Canvas engines, Combo, InputText and Slider.
Layout setup captures an owner before passive registration. Each effect receives
its own lease; delayed or duplicate cleanup cannot erase a later lease or a
replacement public binding. Effect cleanup does not kill a still-mounted native
owner, preserving Strict Mode setup/cleanup/setup. String-based service calls
continue to address the current public binding; delayed work must capture the
owner token or returned lease. Saved component handles already do so.

Stale imperative calls are no-ops with a saturating scalar diagnostic counter.
Unrelated serialization/native exceptions propagate. Events require both a
defined Fiber and a live native target at dispatch. Wasm callbacks enqueue at most
256 payloads per drain and use a microtask to leave the native render stack before
dispatch/liveness queries. Overflow, unknown, dead and disposed events are dropped
and counted. Bridge disposal clears queued payloads, closes the RxJS subscription
and releases module, service, clone and dispatcher references.

React completion callbacks follow synchronous destruction acknowledgment and JS
mapping cleanup for their work. Native event queues have independent delivery
order; they are checked at dispatch, not assumed drained by an unmount callback.
A newer observed frame is required for rendering assertions. Wasm wrapper teardown
waits for Fabric unmount before disposing its bridge and requesting module exit.
Node `render` now returns an optional asynchronous unmount function with the same
cleanup boundary, preserving existing callers that ignore its return value.

## Resolved and remaining defects

XF-LIFE-001/002/003/006/007/010 are ordinary passing cleanup/identity assertions.
The fake-binding suite models actual destruction results, delayed and duplicate
acknowledgments, delayed effects, public-ID reuse, queued events, all registered
component families, Strict Mode and bridge disposal. It never resets registries
to manufacture ordinary unmount success.

The following characterizations still execute with narrow signatures:

- XF-LIFE-004: three creates precede the first `completeRoot` in the small fixture.
- XF-LIFE-005: a same-native-ID move destroys the child; the receiving parent's
  hierarchy references it while Yoga has no child. Subsequent container unmount
  now returns to zero instead of retaining the three parents. That secondary
  count change follows the root cleanup fix; the reparent defect remains.
- XF-LIFE-008: React cross-parent remount gets a new ID but retains the old native
  child; six fake nodes, including its synthetic container, remain before unmount.
- XF-LIFE-009: abandoned Suspense work retains one prospective leaf after ordinary
  unmount. Its Fiber and public/native lifetime records also remain because native
  destruction never occurred. This isolated case is excluded from normal cleanup
  stress acceptance and requires Stage 3 staging/discard.

## Validation evidence

Raw output stays under ignored `packages/dear-imgui/npm/build/diagnostics`.

| Gate | Observed result |
| --- | --- |
| Windows native suite | 333 tests passed, VS2022/MSVC 14.44, Ninja Debug. Includes 1,000 direct queue cycles, duplicate-result protection and delayed subject delivery against a reused native ID. |
| JavaScript lifecycle | 14 scenarios passed in isolated development and production processes; three remaining JS characterizations execute. |
| Current-source native artifacts | VS2022 Release addon and optimized Docker/Emscripten Wasm rebuilt. |
| Node/OpenGL stress | 1,000 production cycles passed in one renderer; all count deltas zero. |
| Wasm/WebGPU stress | 1,000 production cycles passed with all count deltas zero, diagnostics-disabled behavior and ordinary wrapper Strict Mode/update/populated-unmount checks. |
| Linux native suite | All 333 tests passed with GCC/Ninja Release in the documented Ubuntu Docker environment. |
| Node full-App smoke | Development and production screenshots and subsequent populated-unmount cleanup passed through the ordinary Node wrapper. |
| Three-repetition production comparisons and 100,000-row cases | Passed on both declared adapters; results and regressions below. |
| Full-App browser smoke | Production Edge/SwiftShader screenshot passed and was inspected for a populated dashboard. |

The empty post-warm-up baseline is zero native elements, subjects, Fibers,
forward/reverse public bindings, native lifetime records and registrations, plus
one empty hierarchy entry for container 0. Both runtimes' observed 1,000-cycle deltas are
zero for all ten reported count fields. Mounted fixtures require 8 native elements,
9 hierarchy entries, 2 subjects, 8 Fibers, 7 forward/reverse public bindings and
2 registered widgets; working data updates and callbacks are also required.
Half the cycles remove a subtree before root unmount; the other half replace a
keyed PlotBar and unmount with both populated widgets attached.

| Post-warm-up count delta, 1,000 cycles | Node/OpenGL | Wasm/WebGPU |
| --- | ---: | ---: |
| Native elements | 0 | 0 |
| Hierarchy entries | 0 | 0 |
| Internal-operation subjects | 0 | 0 |
| Fiber entries | 0 | 0 |
| Forward public-ID mappings | 0 | 0 |
| Reverse public-ID mappings | 0 | 0 |
| Native target records | 0 | 0 |
| Registered widget targets | 0 | 0 |
| Table/plot registration set | 0 | 0 |
| Map/image registration set | 0 | 0 |

## Production measurements

Measured on 8 September 2026, Windows `10.0.26200`, Ryzen 7 5700U, Node 24.14.0,
React 19.2.3 / RN 0.87.0, repository Roboto 16 and a 900×700 surface. Native
artifacts are VS2022/MSVC 14.44 Release on AMD Radeon OpenGL 4.6 (driver
`23.19.23.13.250826`), and Docker/Emscripten 5.0.2 `-O3` on Edge 152 headless
Google SwiftShader/Subzero (`0xc0de`). Hardware OpenGL and software WebGPU remain
separate environments. No task-owned builds ran concurrently with the final
measurements; unrelated host activity was not isolated.

The regular comparison matches Stage 0's 1,000 rows, 128 retained points, eight
seed points, 1-second warm-up, 3-second input intervals, three repetitions, and
2-second idle. Stress runs are separate, so their concurrent build load is not
included in this table. JS intervals measure invocation to observing the latest
sample in a submitted frame, not display presentation or every sample's latency.
All inputs were accounted for as observed or coalesced and no final input timed out.

| Runtime / Hz | Achieved Hz | Observed / produced | JS p50 ms | JS p95 ms | JS p99 ms | Maximum ms |
| --- | --- | --- | --- | --- | --- | --- |
| Node / 20 | 19.94–19.97 | 180 / 180 | 15.43–15.89 | 16.49–18.11 | 18.14–34.48 | 34.48 |
| Node / 60 | 59.84–59.97 | 505 / 540 | 15.48–15.57 | 16.57–17.50 | 17.25–32.05 | 32.43 |
| Node / 120 | 77.45–84.35 | 772 / 1080 | 16.26–16.56 | 17.54–18.04 | 18.57–18.79 | 31.52 |
| Wasm / 20 | 19.89–19.98 | 175 / 180 | 31.05–33.93 | 42.58–49.26 | 44.84–70.53 | 70.53 |
| Wasm / 60 | 59.77–59.99 | 265 / 540 | 11.68–15.16 | 22.97–30.11 | 29.11–37.47 | 37.47 |
| Wasm / 120 | 102.08–119.96 | 282 / 1080 | 7.61–15.25 | 18.48–28.91 | 26.98–34.12 | 35.16 |

Ranges are per-repetition percentiles, not pooled distributions. Compared with
Stage 0, Wasm 60 Hz p95 increased from 17.13–18.63 ms to 22.97–30.11 ms, and 120 Hz
p95 increased from 12.71–15.25 ms to 18.48–28.91 ms. These are material observed
regressions; the run does not isolate native liveness checks, trace overhead,
browser allocation or host contention as their cause. Node 20/60 Hz tails improved,
while its 120 Hz p95 rose from 17.06–17.20 ms to 17.54–18.04 ms. Node achieves
only 65–70% of requested 120 Hz. Wasm reached approximately 120 Hz in two regular
repetitions but slowed to 102.08 Hz in one, so it did not meet the proposed 95%
input-rate target in every repetition either. None of these
hardware-specific timings becomes a universal CI threshold.

Stage 1 adds one numeric-only `isElementAlive` bridge query per imperative call.
Each regular repetition therefore has 180/540/1,080 imperative calls **and** the
same number of liveness calls at 20/60/120 Hz. JSON byte totals remain
11,050/33,160/66,330. The bounded observer now includes these extra boundary calls
explicitly; historical reports did not include a liveness method.

Node idle was 41 frames over 2,001 ms and 6.95% of one CPU core, versus Stage 0's
24 frames and 1.55%. This is another material observed regression, despite no
render-policy change in this slice. Wasm remained at 60 frames over about 2 seconds;
total browser/native CPU and RSS remain unavailable. Idle scheduling and wakeup
profiling belong to Stage 4; cleanup success does not establish idle efficiency.

The separate 100,000-row cases use Stage 0's shorter 1-second input intervals,
200 ms warm-up, three repetitions and 1-second idle. Both passed semantic and
cleanup checks. Population took 1,129 ms on Node and 2,998 ms on Wasm (Stage 0:
980/2,573 ms). Requested 120 Hz achieved 75.38–91.43 Hz / 119.81–119.97 Hz.
Wasm's first 20 Hz repetition had a 217 ms maximum observation interval, compared
with Stage 0's 320 ms; short large-table runs continue to expose warm-up/tail
behavior. This measures initial bulk population plus incremental appends, not
100,000-row replacement or sorting on every input.

Raw reports are `cleanup-{node,wasm}-baseline-final/result.json`,
`cleanup-{node,wasm}-extended-final/result.json` and
`cleanup-{node,wasm}-stress-final/result.json` under the ignored diagnostic directory.
Populated stress and full-App PNGs were inspected. Run `npm run diagnostics:report
-- <node-result.json> <wasm-result.json>` to regenerate the measurement tables.

Earlier measurements are preserved as `cleanup-*-before-read-guard.json`; they
precede the final stale-read metadata guard and show run-to-run variation. The
table above uses the final native binaries, including that guard. One browser
profile removal initially hit a Windows file lock; after the browser exited, the
task-owned profile was removed. This did not affect any runtime cleanup counts.

The comparison options, from the authoritative npm workspace:

```powershell
$env:XFRAMES_DIAGNOSTICS_OPTIONS = '{"durationMs":3000,"warmupMs":1000,"idleMs":2000,"cycles":0}'
npm run diagnostics:node -- --baseline
npm run diagnostics:wasm -- --baseline
$env:XFRAMES_DIAGNOSTICS_OPTIONS = '{"durationMs":1000,"warmupMs":200,"idleMs":1000,"cycles":0}'
npm run diagnostics:node -- --baseline --extended
npm run diagnostics:wasm -- --baseline --extended
Remove-Item Env:XFRAMES_DIAGNOSTICS_OPTIONS
npm run diagnostics:node -- --stress
npm run diagnostics:wasm -- --stress
```

Use distinct `XFRAMES_DIAGNOSTICS_DIR` values to retain each report. Rebuild native
artifacts first using the diagnostic guide; these commands do not compile C++.

The hosted Stage 0 run
[34196614743](https://github.com/xframes-project/xframes/actions/runs/34196614743)
finished with successful JavaScript and Linux native jobs. Windows failed before
compilation because Git converted byte-compared generated Fabric files to CRLF.
Wasm failed because a clean checkout had no generated Leptonica `endianness.h`.
This change pins those generated JS files to LF and generates the Wasm header in
the build directory. Current cleanup changes have not been run on hosted CI.
Both configured rendering gates now include 1,000 cleanup cycles; extended
commands and diagnostic uploads remain available. Configuration is not evidence
of a successful hosted cleanup run.

Reproduction commands and timing limits are in the
[diagnostic guide](../../packages/dear-imgui/npm/diagnostics/README.md). Use its
authoritative workspace lockfile, supported VS2022 toolchain and Docker Wasm build.
Do not use process exit, forced GC, RSS or whole-registry resets as cleanup proof.

## Next slice

Stage 2 should introduce schema version 1, sequence/surface IDs and common Node/
Wasm `ApplyCommit` entry points, with existing methods delegating to single-op
transactions. Add parse/validation/revision tests while preserving current public
behavior. Stage 3 then stages prospective Fabric work until `completeRoot` and
calculates destruction from final committed reachability; XF-LIFE-004/005/008/009
remain its integration gate. Scheduling remains Stage 4. Map/Canvas performance,
hardware WebGPU, ubx-monitor and Electron/GPUIX comparisons remain open.
