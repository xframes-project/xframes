# Fabric Stage 3 publication

Status: implementation and local acceptance complete, 8 September 2026. Shared
native publication, prospective Fabric collection, committed ownership and the
ordinary wrappers are implemented. Native suites, isolated Fabric tests, real
Node/Wasm integration, expanded stress, production comparisons and full-App smokes
pass locally. The requirement audit is recorded below. These uncommitted changes
have not run on hosted CI; local success does not establish consistently green
hosted coverage.

This follows the [Stage 2 transaction record](fabric-transactions-2026-09.md),
[Stage 1 cleanup record](fabric-cleanup-2026-09.md) and
[Stage 0 baseline](fabric-baseline-2026-09.md).

## Alpha structural API replacement

XFrames has been alpha from the outset, without a stability commitment. Stage 3
deliberately removes the version-1 structural engine and its `setElement`,
`patchElement`, `setChildren` and `appendChild` binding exports. There is one
native publication model, with final ownership and destruction. Old wire versions
are rejected; no compatibility adapter or second destruction engine is retained.
Public React components, imperative widget APIs and initialization/ready callbacks
remain in scope for preservation.

The native `QueueCreateElement`, `QueuePatchElement`, `QueueSetChildren` and
`QueueAppendChild` methods are removed. Hierarchy storage and traversal helpers
are private; Element/Widget traversal participates in `Render`'s lock scope.
Repository Node/Wasm callers, declarations and wire fixtures now use schema v2.
The removed queue methods were used by Node/Wasm bindings and native tests; those
callers were migrated. The experimental Python module in this repository is a
pybind11 add/subtract scaffold and does not call XFrames structural APIs. Historical
built website bundles are release artifacts, not a second supported source engine;
no generated bundles are rewritten or published as part of this source change.

## Version 2 wire contract

Both bindings continue to expose `applyCommit(jsonString): jsonString` and
`getCommitState(): jsonString`, backed by the same C++ implementation. Only
schema version 2 is accepted. A first publication can be:

```json
{
  "schemaVersion": 2,
  "surfaceId": 0,
  "baseRevision": "0",
  "correlationId": "mount",
  "rootChildren": [1],
  "operations": [
    {"op":"create","id":1,"elementType":"node","props":{"root":true}},
    {"op":"create","id":2,"elementType":"plot-bar","props":{}},
    {"op":"setChildren","parentId":1,"childrenIds":[2]},
    {"op":"setChildren","parentId":2,"childrenIds":[]}
  ]
}
```

The result is:

```json
{"schemaVersion":2,"surfaceId":0,"status":"applied","nativeSequence":"1","nativeRevision":"1","correlationId":"mount","destroyedIds":[]}
```

`schemaVersion`, `surfaceId`, `baseRevision`, `rootChildren` and `operations` are
required. `correlationId` is optional, opaque and limited to 128 UTF-8 bytes.
Unknown envelope/operation fields are rejected. Duplicate JSON object keys use
the decoder's last-value policy. Public string IDs belong to JavaScript and are
forbidden as native `props.id`; `props.type` is also forbidden.

Surface 0 is the only supported surface. Node IDs and operation parent IDs are
integers from 1 through 2,147,483,647. The virtual container's complete ordered
child list belongs in `rootChildren`; a `setChildren` operation cannot target 0.
Container 0 always retains one hierarchy entry and has no Element or Yoga node.

`baseRevision` is a canonical unsigned-64-bit decimal string: `"0"`, or digits
without a leading zero, bounded by `18446744073709551615`. It must match the
current native successful revision. A competing publication therefore cannot be
silently overwritten by a stale candidate. This is a consistency guard, not
caller-allocated ordering or correlation-based deduplication.

Supported operations are:

| Operation | Required fields | Meaning |
| --- | --- | --- |
| `create` | `op`, `id`, `elementType`, object `props` | Create a new final reachable lifetime. |
| `patch` | `op`, `id`, object `props` | Patch a final reachable lifetime. |
| `setChildren` | `op`, `parentId`, array `childrenIds` | Declare one complete final child list. |

Every final node requires exactly one child assignment, including leaves with
empty lists. Multiple assignments are rejected. All creates are validated and
applied before patches; patches then retain their envelope order. Thus child
declarations and patches may reference a create later in the envelope. Missing
references are rejected. Duplicate creates, including attempts to recreate an
existing lifetime that would otherwise be removed, are rejected. An explicitly
new lifetime may reuse an ID in a later publication after destruction.

The validator checks all declarations before application: duplicate children,
final multiple ownership, self-links, disconnected cycles, measured Yoga leaves
with children and root-marked nodes attached below an ordinary parent are
rejected. Root-marked nodes can attach only to container 0. Every operation must
target a final reachable node; speculative unattached creates are rejected.
Validation order is deterministic: creates in envelope order, other operations
in envelope order, root relationships, then parent declarations in envelope
order, cycle checks and reachability.

Known synchronously consumed props retain Stage 2's shared widget validation,
numeric bounds, required constructor fields and guarded-null removal rules.
Unknown widget props remain extensible. `root` is a boolean creation property
of `node`, never a patchable identity property. Details of individual prop ranges
remain in the transaction record and `commit_props.cpp`.

## Ownership, visibility and completion

The native instance retains only the IDs owned by its last successful
publication, alongside the live native tree. A request owns its complete
validation tree and final reachability/deletion plan until synchronous completion.
It cannot adopt an existing unowned object or overwrite an unowned virtual root.
Cleanup visits only the previously managed tree; unrelated disconnected unowned
objects are not swept. Direct callers must use the same publication protocol and
revision authority. There is no mixed version-1/new-protocol mode.

Lock order is structural dispatch, serialized ReactivePlusPlus subject,
hierarchy, then elements. The last two locks span model capture, validation,
creates/patches, Yoga detachment/reassignment, hierarchy replacement, deletion
and successful revision advancement. `Render` and CPU diagnostics acquire both;
`GetChildren` and `IsElementAlive` acquire the corresponding tree lock. Neither
can observe an intermediate successful publication. Completed-frame diagnostics
retain the previously captured coherent frame until another frame is submitted.
They do not claim that the latest publication has been presented.

All previous managed Yoga parent links are removed before final attachments.
Surviving Elements and Yoga nodes are preserved, as are their internal subjects
and populated PlotBar/Table data. Destruction is previous-tree depth-first
postorder, respecting previous root/sibling order, filtered to exclude final
survivors. Moving a descendant out of a removed ancestor therefore destroys only
the unreachable ancestors/siblings. Destruction calls the existing resource
destructor chain once per actually removed Element.

Successful empty, unmount and identical-tree publications each advance native
sequence and revision once. Rejection advances neither. Diagnostics toggles and
idempotent subject setup do not reset ordering. Counter exhaustion rejects before
mutation. Native uint64 counters travel as decimal strings. Results and the one
existing desktop wake notification follow tree-lock release. The renderer's
existing cadence is unchanged. There are no per-operation wake notifications.

An unexpected application exception consumes its sequence, leaves the successful
revision unchanged, returns `failed` with actual completed destructions, and
quarantines the surface before releasing the tree locks. Normal rendering then
produces no tree content; CPU tree reads reject and liveness checks return false.
Further publications reject with `surface_quarantined`. Recovery requires a new
native runtime. This is explicit failure containment, not arbitrary-allocation
rollback. If even constructing a failure result cannot allocate, quarantine is
already established before the exception propagates.

The structural replay subject retains one weak request reference. Completed
batches, validation trees and results are not retained by that subject.
Imperative widgets still use their existing subject/API and structural dispatch
lock ordering; asynchronous resources do not acquire a total-order protocol.

## Results and diagnostics

Results always identify the supported schema 2 and surface 0. They contain
`status`, nullable string `nativeSequence`, string `nativeRevision`, and
`destroyedIds`. A parsed correlation is echoed. Errors add `error.code`,
`error.message` and nullable zero-based `error.operationIndex`. Messages are
explanatory; codes are the stable classification. Parsing failures do not promise
a correlation echo. Runtime failures outside a specific operation use a null
operation index.

Codes include `invalid_json`, `missing_field`, `invalid_field`, `unknown_field`,
`unsupported_version`, `unsupported_surface`, `unsupported_operation`,
`invalid_id`, `invalid_element_type`, `invalid_props`, `immutable_identity`,
`duplicate_id`, `duplicate_child`, `duplicate_assignment`, `missing_target`,
`missing_children`, `cycle`, `multiple_parents`, `invalid_relationship`,
`unreachable_operation`, `ownership_conflict`, `stale_revision`,
`counter_overflow`, `application_error`, `surface_quarantined` and
`runtime_not_ready`.

`getCommitState` reports the counters, `initialized`, `managedCount` and
`surfaceStatus` (`uninitialized`, `healthy`, or `quarantined`). Enabled diagnostics
also retain one `lastTransaction` with operation/byte counts, parse, validation,
application, visibility-lock wait/held and total intervals. Reachability currently
forms part of validation time, not an independently sampled interval. No durable
trace or frame/revision correlation is introduced.

## Native test migration and current evidence

The 32 old XFrames tests were replaced by 32 publication tests through the real
serialized subject, including the final Canvas bootstrap failure test. The other
311 native tests are retained. This migration
preserves coverage rather than the old immediate-mutation API or test count:

| Previous coverage | Publication coverage |
| --- | --- |
| Individual/recursive removal, sibling preservation, orphan lists, replacement, unchanged lists | Single/deep unmount, partial roots, replacement, reorder/insertion and identical/empty trees. |
| Global format storage and internal subjects | Global formats, actual widget subjects, delayed subject delivery and populated cleanup. |
| Dangling same-ID reparent characterization | XF-LIFE-005 exact Element/Yoga/subject/data survival and moving descendants out of removed ancestors. |
| Duplicate roots and stale no-ops in the earlier API | Duplicate/final ownership rejection, actual destruction once, missing-target rejection and repeat empty publication. |
| 1,000 subtree cycles and populated-root unmount cycles | 1,000 cycles combining populated plots/tables, same-ID moves, alternate subtree removal and keyed replacement/direct populated unmount. |
| v1 operation/parser/prop/range/ordering checks | Strict v2 envelopes, removed-version rejection, ranges, complete assignments, forward references, full rejection immutability, null props, stale revision recovery and competing writers. |
| Prefix-applied failure recovery | Failed publication quarantine, blocked reads/liveness/drawing and rejected subsequent publication. |
| Opt-in frames, table sort/filter, weak requests and lossless counters | Retained actual frame/table tests, expired request proof, diagnostic toggles, subject setup and uint64/overflow checks. |

New deterministic visibility coverage pauses a real subject-delivered publication
between creates. Separate reader and renderer threads rendezvous, prove both tree
locks unavailable, and attempt their actual operations. They complete only after
the publication is released, seeing final hierarchy/Yoga and completed deletion.
No sleep is used to establish exclusion.

The complete current-source VS2022/MSVC 14.44 Debug suite passed all 343 tests,
including these publication tests, hierarchy/traversal encapsulation and Canvas
bootstrap failure containment.
Ignored artifacts are under `npm/build/diagnostics/publication-native-*`.
The Linux Release suite also passed all 343 tests against the same source.
Both native artifacts were rebuilt: VS2022/MSVC 14.44 Release for Node and the
Docker/Emscripten optimized Wasm path with XFRAMES_FAST_BUILD=OFF. build:common,
build:node, build:wasm and diagnostics:typecheck passed. build:common includes
generated-renderer verification, host tests and tooling type checks.

The isolated Fabric harness passed 21 scenarios in each development/production
process. It includes callback-only updates, committed event props during Suspense,
reveal, 1,000 discarded prospective candidates, interleaved clone branches, a
same-ID move escaping a removed parent, registration survival and three real
Fabric failure paths: rejected native publication, failed native application
receipt and inconsistent destruction acknowledgment. Unexpected React errors
fail the harness; expected failure scenarios inspect and account for their errors.
All ten XF-LIFE IDs are asserted as executing coverage.

Initial current-source Node/OpenGL and Wasm/WebGPU integration passed three
ordinary lifecycle cycles, streaming semantics, structural workloads and 61
matching shared wire results with populated/final native state. Node used AMD
Radeon integrated OpenGL; browser WebGPU used SwiftShader software rendering.
Each structural repetition requested 240 React updates: 200 completeRoot
publications and 40 actual same-element bailouts. Native revision and structural
call increments matched every accepted publication. All ten original lifetime
fields returned to baseline after newer empty frames. These short runs are
integration evidence, not the required 1,000-cycle or production comparison.

The expanded production runs passed 1,000 ordinary cycles with 1,000 abandoned
public-ID/callback candidates, followed by 1,000 same-ID PlotBar/Table moves in
each process. After acknowledgment and newer empty frames, every original
lifetime delta was zero, as were committed descriptions, in-call staging and
bridge-retained candidates. Parity at that fixture revision matched 62 wire/ownership
results in stress, regular and extended runs. Wasm's actual DOM/Strict Mode wrapper update,
event, saved-handle and teardown check also passed.

Full-App Node development and production smokes passed populated unmount cleanup.
The full-App production browser smoke passed on SwiftShader. Populated diagnostic
and dashboard screenshots from both runtimes were visually inspected: plot data,
table rows, controls and dashboard content were present. A first custom-path Node
screenshot failed because its artifact directory did not exist; creating that
directory and rerunning both renderer modes succeeded. Native build logs retain
existing compiler warnings; browser webpack logs retain bundle-size warnings.
Raw reports, screenshots and binaries remain ignored artifacts.

Current hosted evidence was inspected afresh: Stage 2 run
[34219862696](https://github.com/xframes-project/xframes/actions/runs/34219862696)
passed JavaScript, Windows native and Linux native/Node jobs. Its Wasm build
succeeded, but Chromium failed to create its WebGPU swap-chain shared image and
reported device destruction during the fixture. The full-App browser smoke and
dependent parity job were skipped. These uncommitted Stage 3 changes have not run
on hosted CI.

## Prospective and committed ownership

Host descriptions belong to their renderer and contain separate props and child
arrays. Clones preserve the generated native ID while isolating siblings and
nested JSON props. The bridge retains only its last acknowledged committed map;
there is no global candidate map or render-attempt log. completeRoot traverses
its actual branded child set and freezes the reachable descriptions. Unreachable
attempts never allocate native elements, subjects or widget resources. Root
metadata is preserved on prop removal and cannot change within a native lifetime.

The private-interface seam attaches non-enumerable prospective callback metadata
to attribute payloads. A callback-only diff produces a host clone even when the
upstream native attribute diff is otherwise null. Event targets hold committed
props and host ancestry because the renderer mutates canonical.currentProps
before publication. Original Fiber handles remain associated with the host
lifetime; bridge mappings release destroyed lifetimes after acknowledgment.

One scalar diagnostic record describes the last publication. Counters distinguish
observed creates/clones, accepted publications, structural calls, failures,
committed descriptions, in-call staging, staging high-water and description
create/clone time. Append helpers are not separately timed; requested-update
latency includes the complete Fabric render/commit path. No completed
batch or abandoned candidate is retained in diagnostics. Queued Wasm events stay
bounded and are drained outside native tree locks.

A JS publication failure never installs the candidate or validates success
callbacks. It invalidates all usable bridge targets and preserves the failure
reason; it does not attempt to continue from uncertain native state. A subsequent
empty React teardown only releases renderer ownership and increments
terminalTeardowns; it makes no native publication and cannot report successful
native cleanup. Native runtime failure separately quarantines native readers and
drawing. Full bridge recovery/reinitialization on that surface is not provided.

## Production measurements and comparison

All four runs used current-source Release/optimized native artifacts, production
React, the same AMD Ryzen 7 5700U host and the exact Stage 2 parameters. They ran
sequentially with no concurrent task-owned builds or other runtime checks.
Regular: 1,000 initial rows, 128 points, 3-second streams, 1-second warm-up,
2-second idle, zero stress cycles and three repetitions. Extended: 100,000 rows,
1-second streams, 200-ms warm-up, 1-second idle and three repetitions. Input rates
are 20/60/120 Hz. Every produced update is accounted as observed or coalesced;
none timed out. The workload remains bulk population plus incremental appends,
not repeated replacement/sorting of all 100,000 rows.

The comparable ordinary fixture made 42 structural boundary calls in Stage 2
(16 creates, 14 appends, 10 child assignments and 2 patches) across six
completeRoot calls. Stage 3 makes six structural calls for those six publications,
a reduction of 85.7%. The additional structural workload is excluded from that
comparison. Imperative streaming calls remain unchanged and do not demonstrate
a batching benefit. No historical Stage 2 latency baseline exists for the new
14-node structural workload; its current measurements are reported separately.

Ranges below are per-repetition percentiles, not pooled distributions. All
latencies use one clock each and include polling where indicated. No physical
presentation timing is claimed.

| Runtime | Workload | Input Hz | Achieved Hz | JS p50 ms | JS p95 ms | JS p99 ms | JS maximum ms |
| --- | --- | --- | --- | --- | --- | --- | --- |
| node | regular | 20 | 19.94-19.99 | 15.29-15.51 | 16.31-16.38 | 16.52-32.02 | 32.02 |
| node | regular | 60 | 59.77-59.92 | 15.42-15.58 | 16.96-17.07 | 30.56-31.92 | 31.95 |
| node | regular | 120 | 76.25-78.17 | 16.44-16.53 | 17.54-17.82 | 18.61-19.95 | 32.09 |
| wasm | regular | 20 | 19.86-19.98 | 30.67-31.65 | 39.08-42.38 | 40.53-47.05 | 47.05 |
| wasm | regular | 60 | 59.95-60.00 | 9.84-12.99 | 21.52-22.66 | 22.98-33.18 | 33.18 |
| wasm | regular | 120 | 119.76-119.81 | 7.93-9.23 | 14.42-17.52 | 21.32-26.21 | 26.21 |
| node | extended | 20 | 19.69-19.87 | 15.11-15.57 | 15.97-16.51 | 15.98-42.34 | 42.34 |
| node | extended | 60 | 59.17-59.76 | 15.36-15.86 | 16.56-31.43 | 16.82-32.36 | 32.36 |
| node | extended | 120 | 77.53-85.92 | 16.33-16.57 | 17.41-17.72 | 17.58-18.42 | 18.42 |
| wasm | extended | 20 | 19.94 | 30.24-33.17 | 37.53-283.67 | 40.73-283.67 | 283.67 |
| wasm | extended | 60 | 59.89-59.98 | 13.08-15.63 | 19.75-24.89 | 20.40-26.99 | 26.99 |
| wasm | extended | 120 | 119.88-119.94 | 8.73-9.81 | 17.60-20.79 | 19.61-22.85 | 22.85 |

Each structural repetition requests 240 React updates, producing 200 publications
and 40 bailouts, with 14 mounted nodes. Boundary calls and successful native
revision increments equal actual publications, not requested React updates.
Staging high-water is 14 nodes; each repetition sends 257,839-260,960 UTF-8 bytes,
with the difference due to generated IDs/revisions gaining decimal digits.
Regular-run p95 intervals, in milliseconds:

| Runtime | Description create/clone | Diff | Serialization | Native boundary | completeRoot total |
| --- | --- | --- | --- | --- | --- |
| Node | 0.0304-0.0369 | 0.0717-0.1081 | 0.0280-0.0363 | 0.4144-0.4749 | 0.5535-0.6473 |
| Wasm | 0.0400-0.0650 | 0.0900-0.1500 | 0.0900-0.1250 | 3.0150-3.2850 | 3.3250-3.6400 |

The direct-native microbenchmark runs 100 publications per repetition, each with
four patches and three complete child assignments: 400 patches, 300 assignments,
100 calls and 53,400 UTF-8 bytes in each regular-run repetition. Native validation
includes reachability. Regular p95 native intervals, in microseconds:

| Runtime | Boundary | Parse | Validation/reachability | Application | Visibility lock wait | Visibility lock held |
| --- | --- | --- | --- | --- | --- | --- |
| Node | 185.6-261.0 | 46.4-61.6 | 24.8-32.8 | 10.7-14.7 | 0.1-0.2 | 35.5-51.5 |
| Wasm | 655-1105 | 265-500 | 95-175 | 65-105 | 5 | 155-265 |

Reports contain p50/p95/p99/maximum for all measured intervals, UTF-8 bytes,
per-phase publication counts and staging high-water. Native reachability and
JS append-helper costs are not separately isolated. Browser total CPU/RSS,
hardware WebGPU and physical presentation timing remain unavailable. Stage 2
microbenchmarks used a different envelope/destruction contract, so their numbers
are historical context rather than equivalent-work timing.

Material regressions and unchanged limitations:

- Node regular 120-Hz production reached 76.25-78.17 Hz versus Stage 2's
  80.30-81.36 Hz. The input target remains unmet even though its observation p95
  was 17.54-17.82 ms versus 18.23-18.45 ms. Rendering remains on the existing cadence.
- Node regular idle CPU was 13.26% of one core versus 5.47% in Stage 2. Extended
  idle was 1.60% versus 4.65%. This variation and the lack of an isolated causal
  experiment prevent attributing the regular spike to the publication change.
- Node extended 60-Hz observation p95 reached 31.43 ms versus Stage 2's 18.63-ms
  upper repetition value. Wasm regular 120-Hz p95 increased to 14.42-17.52 ms
  versus 11.55-15.86 ms; the extended 120-Hz p95 was 17.60-20.79 ms versus
  15.15-15.50 ms.
- Wasm's extended 20-Hz maximum was 283.67 ms versus Stage 2's 278.50 ms and
  Stage 1's 217 ms. The large-table observation tail remains unresolved.

The correctness and call-count results do not establish a universal timing win.
Stage 4 should address scheduling and revision/frame observation while preserving
these measured gaps. No updates were dropped to improve reported performance.

Reproduction uses the commands in the diagnostic README and Stage 2 record,
setting a distinct XFRAMES_DIAGNOSTICS_DIR for each run. Current ignored reports:
publication-stress/{node,wasm}/result.json and
publication-measurements/{regular,extended}/{node,wasm}/result.json. The comparison
baseline is transaction-{node,wasm}-{baseline,extended}-final/result.json. Run
npm run diagnostics:report with any of these paths; the report tool also reads
historical Stage 2 data without retaining its mutation engine.

## Canvas bootstrap audit fix and final verification

The final source audit found that QuickJS, Lua and Janet bootstrap errors could
call application script-error handlers from a widget constructor while publication
held native tree locks. Internal bootstrap failure now throws into the publication
failure/quarantine policy. QuickJS allocation/context/exception cleanup and Janet
runtime/GC ownership unwind if construction fails; Lua uses its existing RAII.
Application-script evaluation and render errors retain their onScriptError API.

A new shared-native test injects invalid bootstrap programs through private
constructor overloads for each real Canvas engine. It requires failed publication,
no application callbacks, unchanged successful revision, quarantine, expired
request ownership and successful fresh-runtime Canvas construction/deletion after
the failure. The final Windows and Linux suites passed all 343 tests, including this test.
QuickJS exception text also has RAII cleanup if error-message allocation throws;
that allocation-exhaustion path is source-audited, not claimed as an injected
test. Both native artifacts and packages rebuilt successfully. Refreshed Node and
Wasm integration each passed three ordinary cycles and all 64 shared wire results,
including creation, data delivery and destruction of all three Canvas engines.
The current parity command passed against those reports. Full-App Node development/
production and browser production smokes passed again with the refreshed artifacts;
the final populated dashboards were visually inspected. The last JS root-metadata
type guard passed the rebuilt common package, all 21 isolated scenarios in each
mode and diagnostics type checking; browser smoke also used that build.

The 1,000-cycle and production measurement runs above preceded the isolated Canvas
bootstrap error-path correction and its two extra binding fixtures. Their
PlotBar/Table publication, staging and streaming implementation is unchanged.
Those reports contain 62 results and are evidence for that source/fixture revision;
they are not relabeled as 64-result final-artifact stress runs. The latest comparator
requires the two Canvas fixtures and is intended for the refreshed 64-result reports.
The final native suites also reran their 1,000-cycle publication test.

Final ignored evidence is under `npm/build/diagnostics/`:
`publication-native-{windows,linux}-final.log`, `publication-{common,node,wasm}-final-build.log`,
`publication-lifecycle-final.log`, `publication-typecheck-final.log`,
`publication-final-validation/{node,wasm}/result.json`, and
`publication-smokes/{node-final-development,node-final-production,wasm-final-production}.png`.

## Completed acceptance audit

The audit maps all five deliverables and the acceptance/scope clauses in `goal.txt`
to source and observed results. The artifact revisions and unavailable measurements
are distinguished above. No acceptance requirement depends on preserving schema v1,
the four removed binding exports or the old native test count.

| Requirement | Implementation and verification |
| --- | --- |
| Prospective create, every clone variant and append/child-set helper; stable numeric IDs, independent props/children, root metadata and surface 0 | `nativeFabricUiManager.ts` owns only the acknowledged map and a renderer ownership token. Clone-branch, prop removal/null/root metadata, reorder and unsupported-surface assertions execute in both Fabric modes. Generated renderer snapshots are unchanged. |
| Interrupted, pending, abandoned and revealed work cannot publish or steal committed identity | Ordinary Suspense transition/reveal tests preserve committed events, public IDs and handles. Each of the 1,000 real lifecycle cycles creates and abandons a pending candidate, then resolves its gate to exercise retry. Manual discarded candidates also leave no bridge-owned collection. |
| Actual `newChildSet`, reachable nodes only, one call for each completeRoot including empty/no-op | Both ordinary wrappers use the same collector. Native calls, acknowledgments and revisions match 200 real publications per structural repetition, with 40 separately counted bailouts. Abandoned host observations do not appear as native publications. |
| Acknowledged JS ownership before successful completion, without replacing surviving leases | Exact destruction receipts release removed targets idempotently. Surviving target tokens and registrations remain; effective public IDs update only when changed. Callback-only updates, Strict Mode, public-ID reuse/rebinding, cross-parent remount and stale event/handle tests pass. |
| Explicit JS failure and bounded deferred events | Rejected, failed and inconsistent acknowledgments never install the candidate. Real Fabric failure scenarios account for errors, invalidate all usable mappings and require wrapper completion to fail. Empty terminal teardown does not claim native success. Wasm's bounded microtask drain remains outside native tree locks. |
| One versioned native contract and migrated callers | Parser accepts only v2; actual bindings reject v1 and omit all four old exports. Source searches find no removed queue implementation/caller in supported paths. Types, fake fixtures, native tests, wrappers and current documentation use the final-tree contract; the Python scaffold and historical generated website artifacts are accounted for above. |
| Strict envelope/props/ranges, final ownership and revision authority | Shared preflight covers forward references, duplicate creates/assignments/children, missing nodes, measured leaves, roots, disconnected cycles, reachability, unowned objects, competing writers and ambiguous recreation. Invalid candidates preserve native state and counters. Lossless uint64, exhaustion and diagnostic-reset tests pass. |
| Visibility, lock order and synchronous application/completion | Public dispatch serializes structural writers; hierarchy then element locks cover full preflight/application/revision. Render, synchronous getters and diagnostics participate. Coordinated reader/renderer attempts through actual subject delivery are excluded until the complete state and deletion are visible. |
| Callback and resource failure boundary | Publication constructors/patchers were audited. Map construction initializes metadata/cache; Element initialization and Canvas patching introduce no application callback. Canvas bootstrap failures now throw into quarantine and unwind engine ownership, as tested for all three actual engines. Existing application-script/render event delivery retains Node's thread-safe queue and Wasm's deferred host drain. |
| Final reachability, deterministic deletion and exact same-ID state survival | Native tests compare Element and Yoga identity, both internal subject lifetimes and populated PlotBar/Table data. Partial roots, descendant escape, deep removal, replacement, later-ID reuse and direct populated unmount pass. Old-tree postorder excludes survivors; empty container 0 retains one metadata entry and no Element/Yoga. |
| Unexpected runtime failure, one wake and bounded reactive ownership | Failure quarantines under visibility locks before returning/rethrowing; normal reads/drawing cannot continue as healthy. Successful publications advance once, rejects never advance, and failed application consumes only its sequence. One desktop wake follows tree-lock release; Wasm cadence is unchanged. Weak request expiry and delayed-subject lifetime tests pass; RxJS/ReactivePlusPlus remain. |
| All ten defect gates and migrated historical coverage | All XF-LIFE-001 through 010 execute and pass in 21 scenarios per mode. 004/009 assert no speculative publication, 008 asserts React remount cleanup, and 005 asserts one-publication same-ID survival. Expected-failure handling was removed; old immediate-destruction characterizations are replaced explicitly in the migration table. |
| Supported builds and real binding parity | `fabric:verify`, `build:common`, `diagnostics:typecheck`, `test:lifecycle`, `build:node` and `build:wasm` pass. VS2022 Windows and Docker Linux native suites each pass 343 tests. Refreshed VS2022 Release Node and optimized Docker/Emscripten Wasm artifacts pass 64-result parity, actual state/ownership checks and diagnostics-disabled checks. |
| Real lifecycle, new staging ownership, wrappers and visible application content | Each real runtime passed 1,000 ordinary/abandoned cycles and 1,000 same-ID moves. All ten lifetime deltas and committed/staging/retained candidate counts are zero after acknowledgment and a newer empty frame. Mounted fixture counts/data and real Wasm DOM/Strict Mode lifecycle pass; final full-App smokes and populated screenshots pass on AMD OpenGL and software SwiftShader WebGPU. |
| Comparable production accounting and structural measurements | Exact regular/extended parameters, three repetitions, 20/60/120 Hz and 100,000 rows are retained, with sequential optimized runs and no concurrent task-owned build. Produced updates are fully accounted. Reports include create/clone, diff, serialization, boundary, parse, validation/reachability, application, lock and total distributions, calls, bytes and high-water. The 42-to-6 ordinary-call reduction is comparable; the new structural workload has no historical latency baseline. |
| Honest costs and limitations | Percentile/max ranges, Node's missed 120-Hz target, idle CPU variation and the Wasm large-table tail remain in the record. Reachability and append costs are not separately isolated; browser CPU/RSS, hardware WebGPU and presentation measurements are unavailable. No uniform performance win is claimed. |
| CI, documentation and remaining scope | Application CI invokes all native, publication, abandoned-work, move, lifetime and removed-version/Canvas parity gates, retaining bounded timeouts and failure artifacts. YAML parses; no new skips or error suppression were added. Current hosted failure is reported above. Roadmap, architecture, embedding and diagnostics documentation match the final contract. No lockfile/dependency, generated renderer, scheduling, publication/release or extra-platform work is included. |

Stage 4 scheduling/frame correlation and the outstanding Map/Canvas, hardware
WebGPU, ubx-monitor and comparative benchmark work remain separate. Roadmap
Stage 3 implementation checkboxes reflect delivered evidence; the broader
Stages 0-4 milestone is not complete.
