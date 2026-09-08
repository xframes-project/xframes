# Fabric lifecycle and streaming baseline — September 2026

This first Phase 12 Stage 0 slice establishes executable lifecycle observations
and a shared PlotBar/Table measurement path. It does not implement Stage 1 cleanup
or atomic Fabric publication. Commands and measurement definitions are in the
[diagnostics guide](../../packages/dear-imgui/npm/diagnostics/README.md).

## Confirmed lifecycle defects

Every expected failure executes its intended invariant and requires the listed
signature. An unexpected pass or changed failure requires review. There are no
blanket skips or `continue-on-error` test jobs. The fake native binding isolates
Fabric behavior; queue tests and the shared real fixture establish native behavior.

| ID | Intended invariant and current observed signature | Follow-up |
| --- | --- | --- |
| XF-LIFE-001 | Unmount releases Fiber mappings. The adapter retains one entry for every created host node. | Stage 1 destruction acknowledgment and JS cleanup. |
| XF-LIFE-002 | Unmount releases widget registrations. Real PlotBar/Table effect registrations remain; two accumulate per fixture mount. Public-ID mappings also remain. | Stage 1 effect cleanup and registration lifetime. |
| XF-LIFE-003 | A saved imperative handle cannot target a destroyed widget. The registration service still forwards the operation to its old numeric ID. | Stage 1 invalidate handles/remove registrations; preserve an explicit stale-handle contract. |
| XF-LIFE-004 | Native publication follows accepted React work. Initial native creates occur before `completeRoot`. | Stage 2 transaction API and Stage 3 staging/publication. |
| XF-LIFE-005 | An explicit same-native-ID reparent preserves the child. Detaching it from its old parent destroys it; the new parent's hierarchy references the missing child while Yoga is empty. | Stage 3 atomic ownership/reachability evaluation. Separate from React key semantics. |
| XF-LIFE-006 | Deleted/missing event targets are rejected at the host boundary. Two late events are forwarded for old/unknown IDs, and the deleted widget's callback executes once after unmount. | Stage 1 target cleanup/stale-event handling, then revision validation. |
| XF-LIFE-007 | Removing a public string ID preserves native numeric identity. Prop removal spills `id: null` into a cloned node and root child publication. | Host identity normalization at the Stage 1/2 seam; public IDs must not overwrite native IDs. |
| XF-LIFE-008 | React cross-parent remount removes the old native child. React generates a different ID as expected, but adapter clone staging leaves the old child attached. | Stage 3 complete child-change staging/atomic commit. |
| XF-LIFE-009 | Abandoned Suspense work creates no retained native nodes. A prospective leaf created before the fallback remains after unmount. | Stage 3 speculative staging/discard and accepted-tree reachability. |
| XF-LIFE-010 | Unmounting container 0 destroys its native roots. `SetChildren(0, [])` does not perform recursive destruction because container 0 has no actual element. | Stage 1 explicit root destruction; Stage 3 reachability must also cover container roots. |

The bridge suite covers mounts, state/prop updates, insertion/removal/reorder,
keyed replacement, rapid updates, deep deletion, imperative races, late events,
Strict Mode effects, cross-parent React remount, and a suspended branch abandoned
after fallback. It does not claim to cover every concurrent scheduler interleaving
or the later protocol for rejecting stale revisioned commits.

The native queue suite exercises actual ReactivePlusPlus handlers and ImGui frame
construction, including numeric table sorting and typed boolean/numeric filters.
Its 1,000-cycle case removes children from a real retained parent and returns
native element/hierarchy/subject counts to baseline. The real React stress fixture
first removes its expensive content subtree and then unmounts the root: internal
subjects return to zero, while XF-LIFE-010 leaves a root and title per unmount.
These are two different operations, not contradictory results.

Both real runtimes completed 1,000 additional lifecycle cycles in one renderer.
Relative to the initial unmounted fixture, each accumulated 2,000 native elements,
8,000 Fiber mappings, and 2,000 widget registrations; internal subjects stayed at
zero. Final counts were 2,002 / 8,008 / 2,002 / 0 respectively. These exact signatures
are expected failures of cleanup invariants, not acceptable lifetime targets.

## Baseline results

Measured 8 September 2026 on Windows `10.0.26200`, Ryzen 7 5700U, Node 24.14.0,
React 19.2.3 / embedded RN 0.87.0 Fabric. Source: `c0b382117d8964863ef769a06317fd8285a39e24`
plus the uncommitted diagnostics slice described here. Both native modules were
rebuilt from these sources. Native: VS2022/MSVC 14.44 Release, AMD Radeon graphics,
OpenGL `4.6.0 ... 23.19.23.13.250826`. Browser: Edge 152 headless, Emscripten 5.0.2
Docker optimized `-O3`, Google SwiftShader/Subzero (`0xc0de`). These are different
graphics backends/adapters; the numbers are not a hardware-normalized comparison.
Task-owned Linux compilation containers were paused during performance collection;
other system activity was not isolated. Timing variation remains informational.

Each regular baseline uses production React, 1,000 initial rows, a 128-point
retention limit per series (eight seed points), 1-second warm-up and 3 seconds of requested input per rate,
three repetitions, 2 seconds of idle, local Roboto 16, and a 900×700 native surface.
The producer sends a fixed sample count at deadlines; missed pacing extends the
actual production interval, which is why achieved Hz can be below requested Hz.

| Runtime / input Hz | Achieved Hz | Observed / produced | JS p50 ms | JS p95 ms | JS p99 ms | JS maximum ms |
| --- | --- | --- | --- | --- | --- | --- |
| Node / 20 | 19.96 | 180 / 180 | 16.06–30.52 | 32.58–47.43 | 47.99–55.82 | 55.82 |
| Node / 60 | 59.84–59.99 | 247 / 540 | 15.98–16.18 | 31.64–32.53 | 32.38–59.22 | 59.22 |
| Node / 120 | 77.13–80.94 | 371 / 1080 | 16.29–16.35 | 17.06–17.20 | 17.29–31.60 | 32.34 |
| Wasm / 20 | 19.92–19.96 | 178 / 180 | 30.96–33.39 | 39.31–40.89 | 42.80–44.11 | 44.11 |
| Wasm / 60 | 59.97–59.99 | 251 / 540 | 7.94–9.45 | 17.13–18.63 | 24.01–27.65 | 27.65 |
| Wasm / 120 | 119.94–119.98 | 258 / 1080 | 6.77–8.05 | 12.71–15.25 | 19.26–23.64 | 23.64 |

Ranges are per-repetition values, not pooled percentiles. JS time is invocation
to polling a submitted frame containing the sample, not display presentation.
The unobserved balance is explicitly coalesced; no final sample timed out. The
lower Wasm latency at higher input rates reflects observing only the latest sample
in a roughly 30 Hz frame stream. It does not mean every update reached pixels
faster. Native widget-state-age p95 ranges were 1.09–32.07 ms on Node and
7.89–36.73 ms on Wasm. Native absolute timestamps were never mixed with JS clocks.

Node idle: 24 frames / 2,004 ms, 1.55% of one core, RSS 190.15→190.78 MiB.
Wasm idle: 60 frames / 2,004 ms; total browser/native RSS and CPU unavailable from
the page. Native-ready milestones were 263 ms for Node and 1,002 ms for Wasm;
populated-state frame observation took another 125 ms / 165 ms from fixture entry.
These omit module/process loading before runner entry and physical first-pixel
presentation. Enabled diagnostics add snapshot, serialization, tracing and polling
cost, so these are instrumented baselines.

Per regular repetition, 20/60/120 Hz produced 180/540/1,080 native imperative
calls and 11,050/33,160/66,330 serialized UTF-8 bytes in both runtimes. Native
frames over those measured/drain intervals were 77–79 / 78–88 / 121–129 on
Node and 88 / 83–85 / 85–87 on Wasm. Node's 120 Hz production intervals were longer
because pacing fell behind; frame counts must be read with elapsed durations.

The separate 100,000-row cases also passed with production React and three
repetitions (1-second input intervals, 200 ms warm-up, 1-second idle, no stress).
Population took 980 ms on Node and 2,573 ms on Wasm from fixture entry. At 120 Hz,
achieved input was 76.70–82.66 Hz / 119.78–119.95 Hz respectively. Wasm's first
20 Hz repetition had a 320 ms maximum observation interval: the short large-table
run exposes a tail requiring longer warm-up/profiling before attributing a cause.
This workload initializes a large table then appends individual rows; it does not
measure full 100,000-row replacement or sorting on every input.

Raw regular baselines are `npm/build/diagnostics/{node,wasm}-baseline-final/result.json`;
extended results use `{node,wasm}-extended-final`. They include operation/byte counts,
native frames, timing sample counts, resources and bounded traces. The guide's
`diagnostics:report` command regenerates readable tables. PNGs were inspected for
populated content; semantic assertions, not image existence, determined success.

The current evidence makes two follow-ups concrete: the Windows Node run falls
short of the proposed 120 Hz input target, and both runtimes keep
rendering while idle. Frame policy, synchronous bridge work, timer pacing and
diagnostic overhead need profiling to separate their contributions. This slice
does not claim a measured performance advantage over Electron or GPUIX.

## Validation performed

| Check | Environment and observed result |
| --- | --- |
| Fabric extraction/host verification, common build, diagnostic TypeScript, real lifecycle suite | Windows Node 24.14.0 and a clean Linux Node 22.16.0 install passed. Each lifecycle mode ran eight scenarios and reproduced eight identified defects. |
| All native unit tests | 329 passed on Windows with VS2022/Ninja Debug and on Linux with GCC/Ninja Release, including six new queue/diagnostic cases. |
| Optimized native artifacts and package builds | VS2022 Release Node addon, Docker/Emscripten optimized Wasm, and common/Node/Wasm package builds passed from the current source. |
| Real fixture and baseline | Windows Node/OpenGL and Edge/SwiftShader WebGPU passed semantic assertions, three-repetition production baselines, and the 100,000-row case. |
| Linux Node rendering gate | A freshly built GCC 13.3.0 Release addon passed the production fixture and full-App smoke under Xvfb/Mesa llvmpipe; both screenshots were inspected. |
| Lifecycle stress | Both real runtimes completed 1,000 cycles with the same three expected accumulation signatures and zero remaining internal subjects. |
| Existing full-App regressions | Windows Node and headless Edge passed; both screenshots were inspected for a populated dashboard. |

The local Linux integration used Ubuntu 24.04 in Docker on WSL2
`6.18.33.2-microsoft-standard-WSL2`, Mesa 25.2.8 / llvmpipe LLVM 20.1.2, and the same
Ryzen CPU. Its shorter baseline used 1-second input intervals, 200 ms warm-up,
500 ms idle and three repetitions. It sustained 119.96–120.12 Hz at requested
120 Hz, observing all 360 measured samples; idle consumed 76.33% of one core over
501 ms while submitting 12 frames. Software GL CPU cost and the different OS,
timer behavior and workload duration make this a separate environment, not an
improvement over the Windows reference. Raw results are in
`npm/build/diagnostics/linux-node/result.json`; the shared report command also
works on this file.

Linux reproduction exposed missing Janet math-library linkage and the Node static
library's missing position-independent code setting; the CMake fixes are limited
to those targets. The workspace lockfile now includes ten missing Linux x64
optional esbuild/Rollup binary entries, preserving every preexisting locked entry
and dependency version. A clean Linux `npm ci --ignore-scripts` verified the repair.

The complete native suite also exposed an obsolete QuickJS test expecting opaque
alpha although the existing CSS parser preserves it. Only that expectation was
corrected. Full-App demos now use a tracked image instead of a missing/local-only
texture; webpack warnings stay in logs without covering the browser screenshot.
Neither change alters runtime lifecycle or rendering policy.

The application workflow is configured, but no hosted Actions run was executed in
this session. Its Node pin is 24.14.0; the local Linux container used 22.16.0.
Windows local native unit results above are Debug, while the configured CI unit
builds are Release. Hardware WebGPU, macOS, and external application/comparative
benchmarks were not validated by this slice.

## Proposed targets, separate from observations

Use a quiet machine with a declared adapter, optimized native code, production
React, 1,000 initial rows, 128 retained points per series, and at least three
repetitions. Targets for later stages, not current correctness gates:

- Sustain at least 95% of requested 20/60/120 Hz input rates with every input
  accounted for as observed or coalesced, and no final sample timing out.
- At 60/120 Hz input, achieve observed-frame p95 below 50 ms and p99 below 100 ms
  on the declared reference environment. Report presentation latency separately
  only after actual presentation timestamps become observable.
- After Stage 1, return native/Fiber/registration/subject counts to the post-warm-up
  baseline across 1,000 lifecycle cycles. Convert each resolved XFAIL to a passing
  invariant; do not keep expected-leak counts as the desired behavior.
- After invalidation scheduling, render no repeated idle frames once state has
  settled, and target idle CPU below 1% of one core over a 10-second idle interval.

The 100,000-row case characterizes scale; it is not a claim that complete table
replacement or sorting 100,000 rows sustains 120 Hz. Map/Canvas, ubx-monitor CNO
integration, hardware WebGPU, Electron/GPUIX comparative implementations, transport
byte arrival, and physical display presentation remain separate follow-up coverage.

## Next implementation slice

Implement Stage 1 explicit destruction and lifetime cleanup, including container
roots, Fiber mappings, widget registrations, and stale event/imperative targets.
Use the existing diagnostics to require count deltas of zero for the cleanup cases.
Keep runtime identity separate from public widget IDs. Retain XF-LIFE-004/005/008/009
until transaction staging and accepted-tree reachability are implemented; local
deletion patches alone cannot make speculative work or same-ID moves atomic.

The new application workflow retains Fabric verification and common/Node/Wasm
package builds, adds Windows VS2022 and Linux native unit gates, and configures
Linux Mesa/Xvfb Node rendering plus Docker/Emscripten Chromium SwiftShader WebGPU
rendering. Timings are informational; missing graphics, invalid observations,
crashes, hangs, and unexpected lifecycle results fail. Dependency caches never
substitute for rebuilding native application sources. Artifacts upload on failure.
