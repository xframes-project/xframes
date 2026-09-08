# Fabric Stage 2 native transactions

Status: Stage 2 implemented and locally validated, 8 September 2026. This follows the
[Stage 1 cleanup](fabric-cleanup-2026-09.md) and
[Stage 0 baseline](fabric-baseline-2026-09.md). Stage 2 is a native transaction
boundary; Fabric still publishes individual operations before `completeRoot`.
Native revisions below are neither Fabric commit revisions nor frame guarantees.

## Version 1 wire contract

Both native modules expose `applyCommit(jsonString): jsonString` and
`getCommitState(): jsonString`. The parser, validation model, dispatch and result
serialization are shared C++. TypeScript definitions live in
`common/src/lib/nativeCommit.ts`.

```json
{
  "schemaVersion": 1,
  "surfaceId": 0,
  "correlationId": "example",
  "operations": [
    {"op":"create","id":1,"elementType":"node","props":{"root":true}},
    {"op":"create","id":2,"elementType":"plot-bar","props":{"dataPointsLimit":128}},
    {"op":"appendChild","parentId":1,"childId":2},
    {"op":"setChildren","parentId":0,"childrenIds":[1]},
    {"op":"patch","id":2,"props":{"showLegend":true}}
  ]
}
```

On a new instance this returns:

```json
{"schemaVersion":1,"surfaceId":0,"status":"applied","nativeSequence":"1","nativeRevision":"1","correlationId":"example","destroyedIds":[]}
```

The envelope requires `schemaVersion`, `surfaceId`, and an array `operations`.
Create requires `id`, `elementType` and an object `props`; patch requires `id`
and `props`; child operations require the fields shown above. Unknown envelope
or operation fields are rejected. Props retain widget-specific extensibility:
unknown props are ignored by the existing widgets. Known synchronously consumed
props are checked before application, including nested styles, columns, series,
resource URL shapes, numeric conversion ranges and required constructor fields.
`props.id` and `props.type` are forbidden: public string IDs remain JS mappings.
`root` is a boolean creation property of `node`, never a patchable identity field.

Numbers consumed as floats must be finite and within `float` range. Integer
props use signed 32-bit bounds, further constrained where consumed: plot point
limits ≥1, decimal digits 0–9, legend location 0–15, marker style −1–9, axis scale
0–4, colormap 0–15, histogram bins ≥−4, map zoom 0–22, slider value count 2–4,
clip rows ≥0, line count ≥1 and initial selection ≥−1. Style color/variable keys
must index the compiled ImGui enum. Colors accept a CSS string or `[string, alpha]`
with alpha 0–1. Table creation requires nonempty columns with string `fieldId`
and `heading`; series labels, when supplied, are strings. Guarded optional null
props preserve native removal behavior, including no-op series, patched table
columns, guarded numeric options and widget colors. Optional style font/colors/
vars/round-corner removals retain their existing behavior. Null is not a blanket
substitute for required or unconditionally consumed props.
JSON duplicate object keys follow the decoder's last-value policy.

Native IDs are JSON integers from 1 through 2,147,483,647. Parent ID 0 is reserved
for the virtual container. It owns a child list but no Element or Yoga node.
The ordinary Fabric wrappers use container 0, which is also the sole supported
surface. Other surface values or types are explicitly rejected. No additional
initialization argument or multi-window support is introduced.

An optional `correlationId` is an opaque string of at most 128 UTF-8 bytes. It
does not allocate order, deduplicate requests or reject stale/gapped requests.
Caller ordering fields such as `sequence`, `nativeSequence`, and `jsSequence`
are unknown fields and rejected. Native sequence and revision start at zero,
are unsigned 64-bit counters per XFrames instance, and travel as decimal strings.
Consumers must use `BigInt` rather than lossy Number conversion for arithmetic.
Both counters are available with diagnostics disabled; diagnostic toggles and
idempotent subject setup never reset them. Teardown releases the instance; a new
instance starts from zero. Exhaustion rejects further transactions before mutation.

Every accepted empty batch, duplicate append, empty patch, repeated empty unmount,
or stale compatibility no-op advances both counters once. Rejections advance
neither. A runtime application failure consumes its allocated sequence but does
not advance the successful native revision.

## Rejection and application boundaries

JSON is decoded once into owned values. A validation-only copy of current node
identity, root/measure flags and hierarchy simulates all operations in order.
It creates no live Elements, Yoga nodes, subjects or widget resources. References
to earlier creates are valid. Forward references and missing targets are rejected.
Duplicate creates and duplicate child lists are rejected. Duplicate append is an
accepted no-op. Self-links, cycles, multiple parents, children of measured Yoga
leaves and roots attached below an ordinary parent are rejected.

Removal retains Stage 1's immediate descendant-first destruction. A later
reference to a removed node is rejected, including removal of an ancestor whose
descendant a replacement list tries to retain. Destroying and recreating the
same numeric ID in one batch is rejected so an acknowledgment cannot invalidate
a newly created owner. Reusing a destroyed ID in a later transaction is allowed.

For example, after the successful example above:

```json
{"schemaVersion":1,"surfaceId":0,"operations":[{"op":"setChildren","parentId":1,"childrenIds":[]},{"op":"patch","id":2,"props":{}}]}
```

returns this rejection and leaves the plot, hierarchy, Yoga, widget data, subjects,
destruction output and revision unchanged:

```json
{"schemaVersion":1,"surfaceId":0,"status":"rejected","nativeSequence":null,"nativeRevision":"1","destroyedIds":[],"error":{"code":"destroyed_id","message":"Operation target is not live","operationIndex":1}}
```

Errors contain a stable `code`, a human-readable `message`, and zero-based
`operationIndex` (`null` for envelope/JSON/counter errors). Codes are:

| Class | Codes |
| --- | --- |
| JSON/envelope | `invalid_json`, `missing_field`, `invalid_field`, `unknown_field`, `unsupported_version`, `unsupported_surface`, `unsupported_operation` |
| Node/props | `invalid_id`, `invalid_element_type`, `invalid_props`, `immutable_identity` |
| Relationships | `duplicate_id`, `duplicate_child`, `missing_target`, `destroyed_id`, `cycle`, `multiple_parents`, `invalid_relationship` |
| Runtime | `counter_overflow`, `application_error`, `runtime_not_ready` |

Messages are explanatory rather than a stable matching interface. Correlation is
echoed after the complete envelope/operation parse succeeds; malformed input that
fails during parsing does not promise an echo. Rejected responses identify the
supported surface 0 even when the request specified an unsupported surface.

A separate structural dispatch mutex spans preflight and application. Lock order
is dispatch, ReactivePlusPlus subject, hierarchy, elements. Private mutation
helpers continue to take/release their own tree locks for each operation; they
never dispatch recursively. Rendering may observe intermediate successful
operations. Atomic visibility and final committed reachability remain Stage 3.

The serialized subject delivers synchronously. Results are completed only after
private helpers have released tree locks. Exceptions cannot terminate the
subscription silently. Predictable input errors are rejections; unrelated
exceptions during operation application return `status: "failed"` with
`application_error`, the allocated sequence, unchanged successful revision and
completed destruction collected so far. A prefix may have applied. Arbitrary
failure rollback is outside this slice; a failed result is never success.
Allocation/setup/result-serialization failures outside that application loop can
propagate as binding exceptions; constructing an error result itself requires
memory. No exception path is treated as an applied acknowledgment.

The caller owns the batch and result through a synchronous request. A one-record
replay buffer retains only a weak request reference, so completed payloads and
acknowledgments are not retained. The subscription is explicitly disposed during
XFrames teardown. Existing internal widget subjects retain their Stage 1 ownership.

## Compatibility mapping

`setElement`, `patchElement`, `setChildren`, `appendChild`, and external `Queue*`
structural methods build one-operation owned transactions and use the same
preflight and application subscriber. Their successful JS argument and return
conventions remain: `setChildren` returns a destroyed-ID JSON array; the other
three return void. Cloned legacy `id`/`type`/`root` metadata is normalized at the
native adapter boundary; numeric ID overwrite is rejected.

Stale legacy patch/parent/append calls remain no-ops. Legacy `setChildren` retains
missing child references, including XF-LIFE-005's dangling hierarchy signature;
the explicit API rejects them. Legacy duplicate virtual-root entries remain
accepted, with each actual destruction acknowledged once. These compatibility
exceptions are native-only policy, not fields accepted by `applyCommit`.

Invalid compatibility calls throw through their existing binding exception
mechanism (Node errors include a stable `code` for native contract errors).
Callers needing portable rejection data use the explicit JSON result API.
Compatibility cannot bypass ID/range or synchronously consumed prop validation.

The ordinary Fabric wrappers still issue those compatibility calls. The adapter's
explicit `applyCommit` method feeds `destroyedIds` to the same idempotent Fiber,
mapping and registration cleanup. Widget commands remain outside the wire schema
and sequence domain; synchronous internal commands and clipped-text appends share
the dispatch lock to preserve ordering with synchronous structural calls.
Asynchronous resources/events do not gain a total ordering guarantee.

Partial patches to Child/Window/TextWrap now check field presence and numeric
width/height types before consumption; their previous string guards could throw
mid-application. Image URL shape validation is shared before construction, with
relative paths resolved by the existing resource loaders.

Preflight copies only node identity/flags and child lists, not widget datasets.
This currently costs a tree traversal for each compatibility call; there is no
coalescing or per-`completeRoot` batching. Diagnostics retain only the last enabled
transaction sample and may lag the always-current sequence/revision after calls
made with diagnostics disabled.

## Validation

VS2022/MSVC 14.44 Debug and Docker Ubuntu 24.04/GCC 13 Release native runs:
all 343 tests passed on each, including ten new transaction tests (concurrency,
partial-root removal and guarded null props included) and all Stage 1 queue tests.
The common build (including Fabric snapshot/host verification), diagnostic
typecheck, Node/Wasm package builds, and 15 lifecycle scenarios in each isolated
development/production process passed. The final Node Release addon passed 53
shared transaction results and 1,000 ordinary lifecycle cycles, with zero growth
in all ten lifetime counters. Full-App development/production Node smokes passed;
both populated screenshots were inspected. Optimized Wasm on headless Edge 152
SwiftShader also passed 1,000 cycles, all 53 transaction results and the ordinary
wrapper Strict Mode/update/event/unmount check. Actual-binding parity passed for
results, errors, relative revisions, populated widget/Yoga state and empty state.
Both runtimes passed disabled-diagnostics checks; their populated fixture images
were inspected. Three-repetition regular and 100,000-row production runs also
passed on both runtimes. Raw logs/reports stay
under ignored `npm/build/diagnostics/transaction-*`; generated binaries are not
source deliverables.

`getCommitState` also reports `initialized`. Before a binding owns an XFrames
instance it reports zero counters and `initialized: false`; `applyCommit` returns
`runtime_not_ready`. Callers retain the existing initialization/ready-callback
requirement. No new initialization argument or asynchronous shutdown contract is
introduced.

The shared real-binding fixture runs inside both ordinary diagnostic commands.
It checks exact results, mixed ordering, malformed final operations, native
PlotBar/Table content, hierarchy/Yoga, diagnostics-disabled calls and later-frame
cleanup. `test:transactions:parity` compares the two reports' semantic results.
CI runs these with existing cleanup stress and compares artifacts in a dependent
parity job; configured jobs are not evidence of hosted success.

Current hosted Stage 1 run [34207382459](https://github.com/xframes-project/xframes/actions/runs/34207382459)
completed with JavaScript, Windows native and Linux native/Node gates passing.
Wasm built successfully but Chromium lost its WebGPU device during the real
fixture (`A valid external Instance reference no longer exists`). Its browser
smoke did not run. Stage 2 changes have not run on hosted CI.

Each stress run uses one process/renderer, alternating subtree removal and keyed
replacement/direct populated unmount. After acknowledgment and a newer frame,
all ten deltas are zero: elements, hierarchy entries, internal subjects, Fibers,
forward mappings, reverse mappings, native targets, registration leases, table/plot
registrations and map/image registrations. The empty baseline has one hierarchy
entry for container 0 and no Element/Yoga node. Mounted fixtures retain the Stage 1
8 elements, 9 hierarchy entries, 2 subjects, 8 Fibers, 7 public mappings and 2
registrations. No GC, renderer reset or process exit substitutes for cleanup.

Full-App browser production smoke also passed; its populated dashboard screenshot
was inspected. Reproduction commands use the authoritative npm workspace and the
VS2022/Docker build paths in the [diagnostic guide](../../packages/dear-imgui/npm/diagnostics/README.md).
The parity invocation for the final stress reports is:

```sh
npm run test:transactions:parity -- build/diagnostics/transaction-node-stress-final/result.json build/diagnostics/transaction-wasm-stress-final/result.json
```

## Production measurements

Measured on 8 September 2026 from the working tree based on `62f46a4`: Windows
10.0.26200, Ryzen 7 5700U, Node 24.14.0, unchanged React 19.2.3/RN 0.87.0, Roboto
16 and 900×700 surfaces. Node uses VS2022/MSVC 14.44 Release and hardware AMD
Radeon OpenGL 4.6, driver `23.19.23.13.250826`. Wasm uses Docker/Emscripten 5.0.2
`-O3` and Edge 152 headless Google SwiftShader/Subzero (`0xc0de`). These are
separate hardware/software graphics environments. Final performance runs were
sequential after native builds and stress completed; unrelated host activity was
not isolated. Timing results are observations, not universal CI thresholds.

Regular production runs match Stage 1: 1,000 initial rows, 128 retained points,
eight seed points, 1-second warm-up, 3-second input intervals, 2-second idle,
three repetitions and no stress cycles in the measured process. Every input is
accounted for as observed or coalesced, with zero timed-out final inputs.
Ranges below are per-repetition percentiles, not pooled distributions.

| Runtime / Hz | Achieved Hz | Observed / produced | JS p50 ms | JS p95 ms | JS p99 ms | Maximum ms |
| --- | --- | --- | --- | --- | --- | --- |
| Node / 20 | 19.94–19.98 | 180 / 180 | 15.40–15.82 | 16.09–17.82 | 16.28–18.41 | 18.41 |
| Node / 60 | 59.83–60.02 | 491 / 540 | 15.67–16.61 | 16.90–19.17 | 23.08–30.75 | 31.60 |
| Node / 120 | 80.30–81.36 | 789 / 1080 | 16.50–16.52 | 18.23–18.45 | 18.79–19.10 | 31.41 |
| Wasm / 20 | 19.87–19.99 | 180 / 180 | 31.95–32.73 | 38.22–42.43 | 43.70–45.14 | 45.14 |
| Wasm / 60 | 59.98–59.99 | 254 / 540 | 6.61–12.17 | 19.69–22.77 | 24.41–27.52 | 27.52 |
| Wasm / 120 | 119.68–119.98 | 250 / 1080 | 6.52–9.46 | 11.55–15.86 | 14.36–21.45 | 21.45 |

Node 60 Hz p95 increased from Stage 1's 16.57–17.50 ms to 16.90–19.17 ms;
120 Hz p95 increased from 17.54–18.04 ms to 18.23–18.45 ms. Its 120 Hz input rate
remains below the proposed 95% target. Final Node idle CPU was 5.47% of one core
(45 frames / 2,010 ms), versus Stage 1's 6.95% (41 / 2,001 ms), still above Stage
0's 1.55%. An earlier run before the final guarded-null fix measured 14.82% idle
CPU and a 31.14 ms 60 Hz p95. Those material earlier regressions are retained in
`transaction-node-baseline-before-scalar-nulls.json`; the corresponding Wasm report
is retained alongside it. Streaming/idle structural call counts are zero, so
these runs do not establish that the guarded-null change caused the difference.
Dispatch locking, diagnostics, wakeups and host contention were not isolated.

Wasm's 60/120 Hz p95 improved from Stage 1's 22.97–30.11 / 18.48–28.91 ms, and
all regular repetitions reached approximately 120 input Hz; this is not 120
observed or presented updates per second. Wasm idle remains 60 frames / 2,008 ms.
Browser/native total CPU and RSS remain unavailable. Scheduling and wakeup
investigation remain Stage 4; this variability is not proof of a resolved cause.

The separate 100,000-row runs retain Stage 1's 1-second input intervals, 200 ms
warm-up, 1-second idle and three repetitions. Both passed semantics, cleanup and
transaction parity. Population reached the mounted milestone in 854 ms on Node
and 3,018 ms on Wasm (Stage 1: 1,129 / 2,998 ms). Wasm's largest observation
interval increased from 217 to 278.50 ms, a material observed tail regression;
Node's maximum was 37.22 ms. Requested 120 Hz achieved 77.44–81.73 Hz on Node and
119.55–119.83 Hz on Wasm. These runs measure initial
bulk population plus incremental appends, not replacement/sorting of all rows on
every input, and do not isolate transaction validation as the source of variance.

Streaming boundary accounting is unchanged from Stage 1: per repetition at
20/60/120 Hz there are 180/540/1,080 imperative calls and the same number of
numeric-only liveness queries; JSON bytes are 11,050/33,160/66,330. Structural
transaction measurements run afterward and do not contaminate those totals.

Each of three transaction repetitions applies 400 plot patches per mode. Direct
batches use 100 calls and 28,400 UTF-8 string bytes; compatibility uses 400 calls
and 7,600 string bytes. Numeric arguments and diagnostic state queries are excluded
from those totals. This measures both the larger envelope and the reduced call
count without claiming that ordinary Fabric is batched.

| Runtime / mode | Boundary p95 µs | Parse p95 µs | Envelope p95 µs | Validation p95 µs | Application p95 µs |
| --- | --- | --- | --- | --- | --- |
| Node / four-op batch | 110.60–163.60 | 20.90–24.90 | included in parse | 12.50–14.20 | 8.10–9.30 |
| Node / one-op compatibility | 53.30–132.50 | unsampled separately | 6.50–7.50 | 7.90–9.60 | 4.80–5.50 |
| Wasm / four-op batch | 270–760 | 100–270 | included in parse | 30–75 | 45–110 |
| Wasm / one-op compatibility | 130–310 | unsampled separately | 30–65 | 15–35 | 20–45 |

These are per-call intervals: batch application includes four operations.
Compatibility envelope timing begins after legacy JSON decoding; its boundary
interval includes that decode. Boundary measurements include native work and
result decoding. Raw reports also retain JS serialization distributions. Native
and JS absolute clocks are never subtracted; neither interval proves presentation.
Enabled instrumentation, polling and clock granularity add cost. This slice does
not require or establish an end-to-end Fabric batching performance win.

Regular reports are `transaction-{node,wasm}-baseline-final/result.json`; extended
reports are `transaction-{node,wasm}-extended-final/result.json` under ignored
`npm/build/diagnostics/`. Use `npm run diagnostics:report -- <reports...>` for
streaming and transaction tables. The exact options match Stage 1:

```powershell
$env:XFRAMES_DIAGNOSTICS_OPTIONS = '{"durationMs":3000,"warmupMs":1000,"idleMs":2000,"cycles":0}'
npm run diagnostics:node -- --baseline
npm run diagnostics:wasm -- --baseline
$env:XFRAMES_DIAGNOSTICS_OPTIONS = '{"durationMs":1000,"warmupMs":200,"idleMs":1000,"cycles":0}'
npm run diagnostics:node -- --baseline --extended
npm run diagnostics:wasm -- --baseline --extended
```

Set a distinct `XFRAMES_DIAGNOSTICS_DIR` before each command to retain each report.
Clear `XFRAMES_DIAGNOSTICS_OPTIONS` before using the default stress workload.

## Remaining integration work

XF-LIFE-001/002/003/006/007/010 remain passing invariants. The executing narrow
XF-LIFE-004/005/008/009 characterizations remain the Stage 3 integration gate.
Their signatures are unchanged: three early creates (004), same-ID reparent
destruction with dangling hierarchy (005), stale cross-parent clone child and six
fake nodes (008), and one abandoned prospective leaf (009). No XPASS was hidden
or signature widened. Native and fake fixtures retain their separate assertions.
Stage 3 should stage prospective work per surface and publish one transaction at
`completeRoot`, provide atomic native visibility, and destroy from final
reachability. Invalidation scheduling and frame/revision correlation remain Stage
4. Map/Canvas performance, hardware WebGPU, ubx-monitor and comparative benchmarks
remain open; this does not complete Stage 0 or the Stages 0–4 milestone.
