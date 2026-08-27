# Fabric-Compatible Runtime Hardening

- Status: proposed architecture
- Last updated: 28 August 2026
- Related decision: [XFrames and GPUIX assessment](../strategy/gpuix-comparison-2026-08.md)

## Purpose

This document defines an incremental hardening programme for the XFrames React Native Fabric integration and render runtime.

The programme covers six related capabilities:

1. Send one atomic native batch per committed Fabric tree.
2. Make destruction and cross-runtime mapping cleanup explicit.
3. Build lifecycle stress tests.
4. Add deterministic native operation recording and replay.
5. Replace the fixed 30 Hz render loop with invalidation-driven rendering.
6. Use the transaction stream for performance instrumentation and automation synchronization.

All six are compatible with Fabric. They strengthen the existing Fabric, RxJS, ReactivePlusPlus, Yoga, and Dear ImGui architecture rather than replacing it.

## Summary

The central change is to introduce an XFrames-owned, versioned transaction at Fabric's `completeRoot` publication boundary.

Live Fabric execution and recorded replay should feed the same native transaction application path:

```text
                        +----------------------+
                        | Live Fabric commit   |
                        +----------+-----------+
                                   |
                                   v
+-------------------+     +--------+---------+
| Recorded replay   +---->| UI transaction   |
+-------------------+     +--------+---------+
                                   |
                                   v
                         +---------+----------+
                         | Atomic native apply |
                         +---------+----------+
                                   |
                        +----------+-----------+
                        |                      |
                        v                      v
              +---------+---------+  +---------+---------+
              | Record/instrument |  | Invalidate frame |
              +-------------------+  +---------+---------+
                                               |
                                               v
                                      +--------+--------+
                                      | Scheduled draw |
                                      +-----------------+
```

The transaction is the common seam. Batching, destruction, replay, timings, and test synchronization should be built around it.

## Current architecture

### Fabric side

XFrames vendors the React Native 0.74.1 Fabric production renderer. The custom [`nativeFabricUiManager.ts`](../../packages/dear-imgui/npm/common/src/lib/react-native/nativeFabricUiManager.ts) implements the methods expected by that renderer.

The important current methods are:

- `createNode`
- `cloneNodeWithNewProps`
- `cloneNodeWithNewChildrenAndProps`
- `cloneNodeWithNewChildren`
- `createChildSet`
- `appendChildToSet`
- `appendChild`
- `completeRoot`

At present, create, patch, append, and child-set methods call the native binding individually. A single global `cloningNode` temporarily accumulates child IDs for one clone operation.

### JavaScript events and widget operations

Native widget events are delivered to the Fabric event dispatcher through an RxJS `ReplaySubject`. Imperative component handles call `WidgetRegistrationService`, which serializes widget-specific internal operations to the native binding.

Fabric structural operations and imperative widget operations are therefore separate entry paths today.

### C++ side

[`XFrames`](../../packages/dear-imgui/cpp/app/include/xframes.h) owns:

- `m_elements`, the native element registry.
- `m_hierarchy`, the retained parent/child structure.
- `m_elementOpSubject`, the serialized structural-operation subject.
- Per-widget internal operation subjects.
- Mutexes protecting elements and hierarchy against concurrent rendering and updates.

Queue methods parse JSON, publish an operation to the ReactivePlusPlus subject, and call `glfwPostEmptyEvent()` on desktop. Subject handlers apply create, patch, set-children, and append-child changes to retained state.

### Render side

[`ImGuiRenderer::BeginRenderLoop`](../../packages/dear-imgui/cpp/app/src/imgui_renderer.cpp) currently wakes at least every `1 / 30` second on desktop and requests a 30 Hz Emscripten loop in the browser. Every iteration prepares an ImGui frame, traverses XFrames, submits draw data, and presents.

## What Fabric guarantees

Fabric separates construction of a prospective native shadow tree from publication of the completed root.

Calls such as `createNode` and clone methods can occur while React is producing work. With concurrent rendering or Suspense, some work may be abandoned. `completeRoot(container, newChildSet)` is the relevant publication boundary in the renderer used by XFrames.

XFrames should preserve that distinction:

- Node creation and cloning describe prospective state.
- A completed root publishes a coherent state transition.
- Only published transitions become visible to the native renderer and recorder.

No patch to the vendored Fabric reconciler should be required. The behavior belongs in the XFrames `nativeFabricUIManager` and native bindings.

## Goals

- Prevent the renderer from observing a partially applied Fabric commit.
- Avoid native mutations from abandoned React work.
- Reduce JavaScript/native boundary crossings and repeated JSON parsing.
- Make final-tree reachability and destruction unambiguous.
- Preserve total ordering between Fabric commits and imperative widget operations.
- Render only when visual state or time-dependent activity requires a frame.
- Make commit-to-frame latency observable.
- Enable native renderer replay without React.
- Provide stable synchronization points for automation.
- Preserve Node and Wasm behavior.
- Keep Visual Studio 17/2022 as the supported Windows toolchain.

## Non-goals

- Replacing React Native Fabric.
- Replacing RxJS or ReactivePlusPlus.
- Replacing Dear ImGui, ImPlot, or Yoga.
- Providing database-style rollback for arbitrary hostile transaction input.
- Producing bit-identical screenshots across all GPUs and font rasterizers.
- Implementing macOS support as part of this programme.
- Embedding GPUIX or another UI runtime.
- Changing public component APIs unless required for lifecycle correctness.

## Terminology

**Prospective node**: a JavaScript-side Fabric host-node description created while React is working. It is not necessarily committed.

**Commit**: one accepted Fabric root publication through `completeRoot`.

**Transaction**: an XFrames-owned, versioned message that describes one coherent native state transition.

**Imperative operation**: a Table, Plot, Map, Canvas, or other widget command initiated outside Fabric structural reconciliation.

**Revision**: the monotonically increasing native state version after a successful transaction.

**Invalidation**: a request for at least one future rendered frame because visual output may change.

**Stable frame**: a presented frame for a known revision when no immediately pending transaction or required animation deadline remains.

## Target transaction model

### Wire shape

The exact representation should be finalized during implementation. The semantic model should resemble:

```ts
type UiOperation =
    | { op: "create"; id: number; elementType: string; props: Record<string, unknown> }
    | { op: "patch"; id: number; props: Record<string, unknown> }
    | { op: "setChildren"; parentId: number; childrenIds: number[] }
    | { op: "appendChild"; parentId: number; childId: number };

interface CommitBatch {
    schemaVersion: 1;
    sequence: number;
    surfaceId: number;
    operations: UiOperation[];
    rootChildren: number[];
}
```

This is a semantic example, not a requirement to expose exactly these TypeScript types publicly.

JSON is acceptable for the first implementation. It is human-readable, already used throughout XFrames, and ideal for early recording and diagnosis. MessagePack, a typed binary schema, or direct typed Node-API calls should be considered only after profiling demonstrates that JSON remains material after boundary crossings are reduced to one per commit.

### Sequence allocation

Every committed structural transaction and imperative command batch needs a monotonically increasing sequence in one ordering domain.

Possible sources are:

- JavaScript, before calling the native binding.
- Native code, when accepting the message.

Native allocation is safer when operations can originate from both JavaScript and native resource completion. If JavaScript sequence values are useful for correlation, carry both `jsSequence` and authoritative `nativeSequence`.

### Multiple surfaces and interleaving

The current global `cloningNode` assumes one active cloning context. A production transaction collector must not rely on that assumption.

The design must support at least one of:

- Per-surface staging keyed by Fabric container/surface ID.
- Immutable prospective node descriptions followed by a traversal of `newChildSet` at `completeRoot`.
- A native staging tree per surface that is published only by `completeRoot`.

Even if XFrames initially exposes one application window, per-surface correctness avoids coupling the bridge to a global mutable render-in-progress variable.

### Operation coalescing

Safe optional coalescing includes:

- Multiple patches to the same prospective node replaced by their final merged props.
- Redundant intermediate `setChildren` values for the same parent replaced by the final value.
- A node created and then abandoned before publication omitted entirely.

Coalescing must preserve observable widget initialization and event semantics. It should be introduced only after uncoalesced transactions are correct and tested.

## Native transaction application

### Entry point

Introduce one common native function, conceptually:

```cpp
CommitResult ApplyCommit(std::string_view serializedCommit);
```

Existing `setElement`, `patchElement`, `setChildren`, and `appendChild` exports can initially create single-operation transactions and delegate to this function. This gives a reversible migration path and avoids a big-bang binding change.

Node-API and Emscripten should expose the same transaction semantics even when their mechanical bindings differ.

### Apply rules

For each commit:

1. Parse the complete message.
2. Validate schema version, required fields, operation types, IDs, and basic relationships.
3. Determine the final parent/child relationships for affected nodes.
4. Apply creates and patches.
5. Publish the final hierarchy.
6. Compute nodes no longer reachable from the committed root.
7. Destroy unreachable native state and resources.
8. Increment the native revision.
9. Emit the commit result and timings.
10. Invalidate the renderer once.

The renderer must not acquire the tree between steps 4 and 7.

### Atomicity level

The required atomicity is **visibility atomicity**: no rendered frame or concurrent tree reader observes an intermediate transaction state.

Database-style recovery from every possible exception is not initially required because the transaction is produced by trusted XFrames code. The implementation should still parse and validate the entire envelope before mutating live state.

Possible native implementations are:

- Hold the relevant state locks once for the whole apply operation.
- Queue complete transactions and drain them on the render thread before `NewFrame`.
- Apply to staging hierarchy/state and swap the published revision.

Queuing transactions to the render thread simplifies GPU-resource ownership and visibility but changes when a binding call's effects become available. Holding locks is a smaller migration but requires careful lock ordering and removal of nested locking in existing create/patch methods. This is an explicit implementation decision, not something to leave accidental.

### Commit result

The native side should return or asynchronously emit:

```ts
interface CommitResult {
    sequence: number;
    revision: number;
    destroyedIds: number[];
    nativeReceiveTime?: number;
    applyStartTime?: number;
    applyEndTime?: number;
}
```

The exact clock representation is platform-specific. Durations should use a monotonic clock rather than wall time.

## Explicit destruction and mapping cleanup

### Ownership rule

A structural node is live when it is reachable from a committed Fabric surface root. Prospective or abandoned nodes must not enter the native live tree.

When a committed node becomes unreachable, all associated state must eventually be removed from both runtimes.

### Native cleanup

Current recursive C++ cleanup already covers much of the native side:

- Child hierarchy entries.
- Element `unique_ptr` ownership and destructors.
- Yoga nodes through element destruction.
- Per-widget ReactivePlusPlus subjects.
- Image texture mappings.
- Map and canvas resources through their destructors.

The transaction implementation must preserve and extend these invariants.

### JavaScript cleanup

For every `destroyedId`, remove:

- `nativeFabricUIManager.fiberNodesMap` entries.
- Public-ID to native-ID widget registrations.
- Native-ID to public-ID reverse registrations, which should be added if absent.
- Event-handler references associated with destroyed host nodes.
- Any pending imperative operation queue targeting the ID.

Cleanup should be idempotent. Receiving the same destroyed ID twice should be harmless and diagnosable.

### Reparenting

Destruction must be calculated from final transaction reachability, not from the fact that a child disappeared from one parent's intermediate child list.

For example, this is a move, not a deletion:

```text
Before: root -> A -> child
After:  root -> B -> child
```

The current immediate `SetChildren` deletion behavior can destroy `child` if the removal from `A` is observed before insertion under `B`. Applying the complete transaction before reachability cleanup resolves this class of error.

### Events racing with destruction

Native events can be in flight while a node is destroyed. The event bridge must define one behavior:

- Drop events whose target is no longer mapped at dispatch time.
- Attach the target revision and accept only events valid for that revision.

Dropping with a debug counter is sufficient initially. Dispatching `undefined` as a Fiber target is not.

## Lifecycle stress tests

### Test layers

The programme needs three complementary layers.

#### JavaScript bridge tests

Use a fake native module to verify:

- One native transaction per `completeRoot`.
- No native publication for abandoned prospective work.
- Operation order within a transaction.
- Destroyed-ID cleanup from maps and registration services.
- Event behavior for destroyed IDs.
- Per-surface staging isolation.

#### Native state tests

Apply serialized transactions directly to C++ and assert:

- Element and hierarchy contents.
- Yoga ownership.
- Correct final reachability.
- Resource and subject cleanup.
- Reparenting without destruction.
- Rejected malformed batches do not mutate published state.
- The revision increments exactly once per successful commit.

#### End-to-end tests

Run Fabric through Node or Wasm and assert rendered or queried results. Cover:

- Initial mount.
- Repeated mount/unmount cycles.
- Deep subtree deletion.
- Sibling insertion, removal, and reorder.
- Keyed replacement.
- Reparenting.
- Rapid consecutive commits.
- Strict Mode development behavior where applicable.
- Suspense or concurrent abandoned work where supported by the vendored renderer.
- Event dispatch during and after unmount.
- Imperative operation racing with deletion.
- Image, Map, and Canvas resource destruction.
- Application shutdown with queued work.

### Stress criteria

Representative stress tests should perform at least thousands of mount/unmount and reparent operations, then assert:

- Live JavaScript Fiber mapping count equals live host-node count.
- Widget registration counts match live registered widgets.
- Native element and hierarchy counts match committed reachability.
- Per-widget subject counts match live internal-op widgets.
- No callbacks target destroyed IDs.
- Memory reaches a bounded steady state after allocator warm-up.

The tests should report counts and revisions on failure rather than relying only on process memory.

## Deterministic operation recording and replay

### Compatibility with Fabric

Recording is compatible with Fabric when it records committed XFrames transactions, not every speculative Fabric construction call.

`completeRoot` closes a structural transaction. Imperative widget operations, resource completions, window changes, and native inputs form additional ordered runtime messages.

### Recording levels

#### Level 1: native visual-state replay

Record committed native state transitions and replay them without React.

Use cases:

- Renderer crash reproduction.
- Native regression tests.
- Repeatable performance profiling.
- Tree and lifecycle diagnosis.
- Benchmark fixtures independent of application code.

This is the first implementation target.

#### Level 2: full application replay

Also record native-to-JavaScript events, application inputs, external data, resource outcomes, and logical time, then rerun React.

Use cases:

- Application behavior reproduction.
- End-to-end deterministic tests.
- Debugging application state transitions.

This is substantially more complex and should follow successful Level 1 use.

### Trace envelope

A durable trace needs more than a ReactivePlusPlus replay buffer. A conceptual envelope is:

```ts
interface TraceRecord {
    schemaVersion: number;
    sequence: number;
    kind:
        | "fabricCommit"
        | "widgetCommand"
        | "nativeInput"
        | "windowChange"
        | "resourceReady"
        | "clockAdvance"
        | "snapshot";
    logicalTime: number;
    payload: unknown;
}
```

Trace headers should identify:

- XFrames version and commit where available.
- Platform and architecture.
- Trace schema version.
- Initial window size and scale.
- Theme and font definitions or stable hashes.
- Asset manifest or stable hashes.
- Random seeds used by XFrames-owned behavior.

### Snapshots and seeking

Long traces should periodically include a full native state snapshot. Replay can start from the most recent compatible snapshot and apply later records.

Snapshots also provide recovery when the transaction schema evolves. Migration tools can target stable snapshots plus newer transactions rather than replaying an entire historical session.

### Determinism boundary

Initial determinism means:

- Identical transaction order.
- Identical committed native tree and widget state.
- Identical layout inputs.
- Controlled logical time for time-dependent XFrames behavior.
- Identical resource identities or recorded resource results.

Bit-identical pixels across GPU vendors and font stacks are not promised. Screenshot assertions should use tolerances and, where possible, supplement pixels with semantic state, bounds, and draw-command assertions.

## Invalidation-driven rendering

### Compatibility with Fabric

Fabric owns committed UI state, not the native presentation clock. It does not require an unconditional render loop.

Every successful Fabric transaction should invalidate at least one future frame. It should not independently force one frame per operation.

### Invalidation sources

The renderer must be invalidated by:

- A successfully applied Fabric transaction.
- An imperative widget operation that can change pixels or layout.
- Mouse, keyboard, touch, wheel, focus, resize, scale, and window events.
- Animation or cursor-blink deadlines.
- Dragging, kinetic scrolling, or other continuous interaction.
- Image decode and upload readiness.
- Map tile completion and upload readiness.
- Canvas script or texture completion.
- Font atlas changes.
- Screenshot requests.
- Debug-window state changes.

Every asynchronous producer must wake the native loop when it transitions state from clean to dirty.

### Desktop scheduler

A conceptual loop is:

```cpp
while (!ShouldClose()) {
    if (!frameScheduler.IsDirty() && !frameScheduler.HasDeadline()) {
        glfwWaitEvents();
    } else if (!frameScheduler.IsDirty()) {
        glfwWaitEventsTimeout(frameScheduler.TimeUntilDeadline());
    } else {
        glfwPollEvents();
    }

    ApplyPendingTransactions();
    ProcessReadyResources();

    if (frameScheduler.ShouldRenderNow()) {
        BeginFrame();
        RenderXFrames();
        SubmitAndPresent();
        frameScheduler.DidPresent(currentRevision);
    }
}
```

The actual implementation must account for GLFW callbacks installed by the ImGui backend. A simple safe first step is to render after any real GLFW event or XFrames invalidation, then optimize event classification later.

### Avoiding lost wakeups

Use an atomic dirty flag or generation counter. Producers should call `glfwPostEmptyEvent()` when changing from clean to dirty. The render loop should clear dirty state only after it has observed the current invalidation generation.

A generation counter is safer than a Boolean when invalidation can occur during rendering:

```text
read generation N
render revision R
if generation is still N, mark frame stable
otherwise schedule another frame
```

### Time-dependent ImGui behavior

Dear ImGui advances time when `NewFrame` runs. Long idle periods require careful `DeltaTime` handling so resuming does not create a huge animation or scroll step.

The scheduler should:

- Use a monotonic clock.
- Clamp unreasonable frame deltas after long sleeps.
- Continue scheduling while a known time-dependent behavior is active.
- Expose a controlled clock in deterministic test mode.

### Wasm scheduler

The browser should render through `requestAnimationFrame` when dirty or continuously active. Options include:

- Start a request-animation-frame loop while active and stop it when clean.
- Keep a lightweight browser callback but skip ImGui preparation and GPU submission when clean.
- Pause and resume the Emscripten loop around invalidation.

The first correct implementation is more important than minimizing every idle callback. Browser event callbacks and asynchronous resource completions must request a new frame after the renderer becomes clean.

### Frame-rate policy

Invalidation-driven does not mean a fixed low frame rate.

- Idle applications should approach zero rendering work.
- Ordinary commits should produce one timely frame.
- Active interaction should follow the display cadence where possible.
- High-rate data may be coalesced to a configurable presentation rate without dropping the underlying native data.
- Benchmarks should report actual presented cadence and latency rather than a nominal frame-rate setting.

## Performance instrumentation

### Correlation

Every structural commit and imperative operation batch should carry a sequence number. Frames should report the highest native revision they contain.

This permits correlation such as:

```text
JS update -> Fabric completeRoot -> serialization -> native receive
-> native apply -> invalidation -> frame start -> GPU submit -> present
```

### Timing points

Capture where available:

- Application data received.
- React update requested.
- `completeRoot` entered.
- Transaction serialization started and ended.
- Native binding entered.
- Native parse started and ended.
- Native application started and ended.
- Renderer woke.
- ImGui frame started and ended.
- GPU submission completed.
- Buffer presentation requested or confirmed.

Not every platform exposes confirmed presentation timestamps. Metrics must identify whether the endpoint is CPU submit, swap request, or actual presentation.

### Metrics

At minimum report:

- Commit operations and serialized bytes.
- Serialization, parse, and apply duration.
- Pending transaction depth.
- Transactions coalesced per frame.
- Commit-to-frame latency.
- Data-to-frame latency for instrumented domain inputs.
- Frame CPU time and GPU submission time where available.
- Presented frame rate while active.
- Rendered frames and skipped idle opportunities.
- Live element, hierarchy, registration, and subject counts.
- Dropped events and invalid operations targeting destroyed IDs.

Use distributions—p50, p95, p99, and maximum—rather than averages alone.

### Instrumentation overhead

Instrumentation should be low allocation and optional in production. Prefer fixed-size records and monotonic timestamps. Expensive trace serialization should run asynchronously or write from a bounded queue with an explicit drop/back-pressure policy.

## Automation

The transaction stream is the synchronization backbone for automation, but it is not a complete automation API.

### Required query surface

- Find a node by public test ID.
- Query native ID, type, props, and state.
- Query Yoga bounds and clipping.
- Query visibility and enabled/focus state.
- Query text or other widget-specific observable state.
- Report the revision and frame containing the result.

### Required input surface

- Mouse move, click, double-click, drag, and wheel.
- Keyboard press and shortcut chords.
- Text input distinct from physical key events.
- Focus and window activation.
- Resize and scale changes.
- Controlled clock advancement.

### Synchronization primitives

- Wait for transaction sequence or native revision.
- Wait for a frame containing a revision.
- Wait until no transaction is pending and no immediate animation deadline exists.
- Capture a screenshot associated with a stable revision.
- Fail with tree, transaction, and timing diagnostics on timeout.

### Screenshot role

Screenshots are one assertion type, not the automation architecture. They should be paired with state, bounds, and event assertions. The in-progress screenshot implementation is a useful presentation endpoint for this broader interface.

## Reactive stream role

Reactive streams should transport and observe committed transactions and runtime messages. They should not become a second source of truth beside the retained native tree.

Recommended subscribers include:

- Native transaction application.
- Metrics aggregation.
- Optional trace recorder.
- Debug diagnostics.
- Automation wait conditions.

The durable recorder should subscribe to versioned messages and persist them. `serialized_replay_subject` alone is not sufficient because its bounded in-memory replay buffer is neither a full session log nor a stable file format.

## Incremental delivery plan

### Stage 0: characterize current behavior

- Add JavaScript bridge tests with a fake native module.
- Add native tree count and reachability assertions.
- Cover mount, unmount, reorder, replacement, and reparenting.
- Measure current operation counts, JSON bytes, and commit-to-frame latency.

Exit condition: current behavior and known failures are reproducible.

### Stage 1: explicit cleanup

- Add reverse widget-ID mappings.
- Make event dispatch reject missing/destroyed targets.
- Expose or calculate destroyed native IDs.
- Clean `fiberNodesMap` and widget mappings idempotently.
- Add leak-count stress assertions.

This can initially cover current C++ destruction behavior, but final reparent-safe cleanup depends on Stage 3.

### Stage 2: transaction envelope and compatibility API

- Define schema version 1.
- Add sequence and surface IDs.
- Add native `ApplyCommit` entry points for Node and Wasm.
- Make existing native methods delegate to single-operation transactions.
- Add parse, validation, and revision tests.

Exit condition: no public JavaScript behavior changes, but all structural operations can use the common native transaction path.

### Stage 3: batch at Fabric publication

- Stage prospective Fabric operations without live native side effects.
- Flush one coherent transaction from `completeRoot`.
- Apply under one visibility boundary.
- Compute destruction from final reachability.
- Return or emit one commit result.
- Invalidate once.

Exit condition: one native structural call and one native revision per Fabric commit; reparenting is safe; abandoned React work is not published.

### Stage 4: invalidation scheduler and metrics

- Add invalidation generation and frame revision.
- Audit all synchronous and asynchronous invalidation sources.
- Replace the desktop 30 Hz timeout with event/deadline scheduling.
- Replace the browser 30 Hz policy with request-animation-frame scheduling.
- Add transaction/frame timing records and counters.

Exit condition: idle rendering approaches zero, interaction is not capped at 30 Hz, and every commit can be correlated to a frame.

### Stage 5: native trace and replay

- Persist transaction and imperative-operation records.
- Capture initial configuration and resource identities.
- Add native replay without React.
- Add periodic snapshots.
- Use controlled time for replayed animations.

Exit condition: a recorded representative session recreates the same native state and passes tolerant screenshot/state assertions.

### Stage 6: automation API

- Add test-ID query and bounds/state inspection.
- Add input injection.
- Add wait-for-revision and wait-for-stable-frame.
- Integrate screenshot capture.
- Run representative Node and Wasm functional tests in CI.

Exit condition: a test can mount an app, locate a widget, interact with it, wait for a stable revision, and assert state plus screenshot output.

## Size and effort classification

These are rough focused-person estimates for a maintainer familiar with the code, excluding macOS enablement.

| Capability | Classification | Indicative effort |
| --- | --- | ---: |
| Atomic batch per Fabric commit | Medium to large | 2–4 weeks |
| Explicit cross-runtime cleanup | Small to medium | 3–7 days, overlapping batching |
| Lifecycle stress-test foundation | Medium | 1–2 weeks |
| Basic native transaction recording/replay | Medium | 1–2 weeks after batching |
| Full application deterministic replay | Large | 4–8 additional weeks |
| Invalidation-driven Node and Wasm rendering | Medium | 1–3 weeks |
| Core performance instrumentation | Small to medium | 3–7 days after sequencing |
| Query/input/screenshot automation | Large | 3–6 weeks |

The core reliability slice—tests, cleanup, batching, basic metrics, and invalidation—is approximately 4–8 focused person-weeks because the work overlaps. Durable replay and serious automation are a further 4–8 weeks. Full deterministic application replay should be treated as a continuing capability rather than a single bounded feature.

## Acceptance criteria

### Commit correctness

- Exactly one structural native transaction is published for each accepted `completeRoot`.
- No prospective node from abandoned React work enters the live native tree.
- A rendered frame never observes a partial structural transaction.
- Reparenting does not destroy the moved node or its widget state.
- Malformed transactions are rejected before the published revision changes.

### Lifecycle correctness

- JavaScript Fiber mappings equal live committed host nodes after every stable frame.
- Widget ID mappings equal live registered widgets.
- Native elements and hierarchy equal final committed reachability.
- Removed widget resources and subjects are released.
- Events targeting destroyed IDs are safely dropped and counted.

### Scheduling correctness

- Idle windows do not render at a fixed cadence.
- A commit, input event, asynchronous resource completion, or screenshot request produces a frame.
- Active interaction can render at display cadence and is not capped at 30 Hz.
- Invalidation during rendering schedules another frame without a lost wakeup.

### Observability

- Every committed transaction has a unique sequence and resulting native revision.
- Every frame identifies the highest included revision.
- Commit-to-frame latency can be calculated without log-text parsing.
- Live object counts and dropped-event counters are queryable in tests.

### Replay

- A native trace can reconstruct the same retained tree and widget state without React.
- Trace schema incompatibility fails explicitly.
- Controlled-time replay can pause and advance time-dependent XFrames behavior.

## Risks and mitigations

### Fabric implementation coupling

Risk: the adapter relies on details of the vendored Fabric renderer.

Mitigation: use only the `nativeFabricUIManager` surface already required by that renderer, treat `completeRoot` as publication, document the vendored React Native revision, and protect behavior with bridge tests.

### Multiple in-progress roots

Risk: a global transaction buffer mixes work from different surfaces.

Mitigation: stage by surface/root or materialize only from each `newChildSet`. Do not extend the existing global `cloningNode` pattern.

### Locking and deadlock

Risk: wrapping an entire transaction in locks conflicts with existing methods that acquire the same locks internally.

Mitigation: separate public locking entry points from private `...Unlocked` mutation helpers, establish one lock order, or apply transactions on the render thread.

### Imperative operation ordering

Risk: a plot or map command races with structural creation or destruction.

Mitigation: assign all runtime commands authoritative native sequence numbers and reject or defer operations whose target revision is not live.

### Missing invalidation

Risk: a producer changes state without requesting a frame.

Mitigation: centralize state-changing APIs around `Invalidate(reason)`, expose debug counters by reason, and run tests that allow the renderer to go fully idle before each stimulus.

### Trace volume

Risk: high-rate plot or telemetry commands generate excessive logs.

Mitigation: support bounded asynchronous writing, optional payload compression, periodic snapshots, explicit lossless/lossy modes, and domain-aware batch records.

### Pixel nondeterminism

Risk: screenshots differ across drivers or font environments.

Mitigation: pin assets and fonts, record scale and dimensions, use tolerant comparisons, and assert semantic state and bounds alongside pixels.

## Open implementation decisions

- Whether complete transactions are applied on the calling thread under locks or queued to the render thread.
- Whether prospective JavaScript nodes are staged as operations or materialized by traversing the final child set.
- How Fabric surface IDs are derived and represented.
- Whether structural and imperative commands share one wire envelope or only one native ordering domain.
- How much resource data is embedded in traces versus referenced by hash.
- Which ImGui states require continuous frame deadlines.
- How browser frame pausing and restarting is implemented with Emscripten.
- Which presentation timestamp can be measured reliably on each backend.
- Whether full application replay is justified after native visual-state replay is in use.

These decisions should be made from small prototypes and measurements. They do not alter the compatibility conclusion: atomic commits, deterministic replay, and invalidation-driven rendering all fit Fabric and the existing reactive architecture.
