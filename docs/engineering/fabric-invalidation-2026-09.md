# Fabric Stage 4 invalidation scheduling

Status: Stage 4 MVP delivered in commit `1de105f1d8479337e78a083b8c6645416c301c23`
(8 September 2026). Focused CI stabilization completed on 11 September 2026 in
`fa12b2255fd1fed5a366c137f4003323c69475ee`; all five jobs in
[application run 34623926577](https://github.com/xframes-project/xframes/actions/runs/34623926577)
passed, including the required Linux Node/Wasm runtime, inactivity/activity,
App smoke and parity gates. See the [stabilization evidence](#focused-ci-stabilization-11-september-2026).
The shared scheduler, both backend loops, producer ownership and correlation are
implemented. Local Windows/Linux native suites pass 378 tests; local Windows
Node/browser runs pass the complete 1,000-cycle lifetime/resource/move fixture
and 65-result parity.
The focused pass also completed Linux application execution and verified both
1,000-cycle runtime workloads on hosted Linux. Production observations and the
original failed hosted run remain recorded below.
The Stages 0–4 milestone remains open independently of Stage 4 core delivery.
Earlier checkpoints below record their source/evidence boundaries and do not
override later results.

## MVP completion boundary

Delivery uses an MVP acceptance bar. The MVP includes the shared scheduler, both
native backend integrations, publication/imperative/input/resource wake paths,
finite deadlines and Canvas activity control, truthful submitted-frame
correlation, lifetime cleanup, deterministic tests and demonstrated ordinary
Windows Node/browser application use. These features remain fully implemented;
the revised scope defers additional qualification rather than removing behavior.

The original MVP closure reviewed 378 passing native tests on each of
Windows and Linux; both complete 1,000-cycle runtime stress reports; 65 shared
parity results; required package/Fabric/lifecycle gates; Node development and
production App smokes; full-App browser production and wrapper gates; and actual
isolated ubx-monitor telemetry. Both separate 10-second inactivity/activity
reports pass with zero continuing idle frames. `invalidation-mvp-typecheck.log`
records the final successful diagnostic typecheck. Workflow YAML and fixture
options parse, required jobs remain present, and no continue-on-error was added.
The final working-tree whitespace check passes. No generated renderer snapshots
or authoritative npm lockfile changes were introduced.

Linux Node application execution and passing hosted runtime gates were deferred
at MVP closure and are now verified by the focused CI pass below. Remaining
qualification includes reliable desktop input across foreground conditions,
controlled performance targets, mixed-DPI/hardware WebGPU/physical serial and
presentation evidence. The extended Node post-measurement focus failure remains
recorded as a failure. Existing timing observations remain provisional. These are
follow-up qualification items, and the broader Stages 0–4 milestone remains open.
The implementation and original acceptance record are committed in `1de105f`.

Performance interpretation was clarified during this validation: this is a shared
host with variable background load and foreground-window conditions. Requested
120-Hz input is a useful workload, but is not a defensible machine-independent
acceptance threshold here. Matched runs record achieved input, observation latency
and stage costs as provisional observations. They do not establish a renderer
ceiling or justify extending implementation work to chase the number. Controlled
performance qualification remains a separate milestone evidence gap.

This follows the [publication record](fabric-publication-2026-09.md),
[transaction record](fabric-transactions-2026-09.md),
[cleanup record](fabric-cleanup-2026-09.md) and
[baseline](fabric-baseline-2026-09.md). Reproduction uses the authoritative
[npm workspace guide](../../packages/dear-imgui/npm/diagnostics/README.md).

## Shared ordering and ownership

`FrameScheduler` owns one monotonic invalidation generation, one covered
generation, one constructing frame and one latest submitted frame. Generation 1
requests the initial frame. Every accepted publication, including empty/no-op,
advances generation once under the same hierarchy/element locks as its native
revision. Rejected preflight does neither. Imperative subject handlers advance
generation under the element lock without changing structural revision. Native
clipped-text append now participates in the same boundary.

Draw construction captures `(frameId, nativeRevision, generation, reasons)` under
the tree locks before traversal. Successful backend submission completes that
exact ticket. Completion never rereads the live revision/generation. Work arriving
after capture remains dirty, including work that happens to become visible during
later resource processing. Skipped or replaced draw data abandons its ticket and
never advances coverage. Frame identity and generation travel as decimal uint64
strings. Expensive tree snapshots are independent of this ordering.

The numeric diagnostic `frame` counter has been removed. `frameId` is the native identity,
including frames while diagnostics are disabled. `nativeRevision` and
`coveredGeneration` describe the sampled draw data. The always-readable `scheduler`
object reports current pending state and the latest submitted ticket even when
expensive diagnostics are disabled. Neither endpoint is GPU completion or physical
presentation. Ordinary queries do not invalidate; changing diagnostics from off to
on explicitly requests one sample without resetting ordering.

Lock order remains dispatch → serialized subject → hierarchy → elements →
scheduler. The scheduler never obtains tree/dispatch locks. Producers publish work
and advance generation under their visibility boundary, then call `Notify`.
Backend wake callbacks must only post an event or arrange an asynchronous callback;
they cannot reenter the scheduler, acquire tree locks, or invoke application JS.
Attach/detach and wake execution share the scheduler mutex so detach excludes a
callback using a destroyed backend. Terminal status rejects frame completion,
releases activity ownership and preserves the last submitted correlation honestly.
Runtime disposal detaches its wake and cancels owners without resetting counters.

The desktop sleep handoff implements this explicit protocol:

1. Drain platform events, then call `TakeOpportunity`.
2. The scheduler mutex atomically checks pending generations and real deadlines
   and arms waiting if no frame is needed.
3. Enter the platform's queued-event wait without another intervening event drain.
4. A producer preceding step 2 is observed there; one arriving after step 2 posts
   a latched event. Repeated notifications coalesce until the next opportunity.
5. After submission, take another opportunity. Only the captured generation was
   covered, so concurrent invalidation still requests a frame.

The Win32 GLFW source posts a message to its helper window. The
[GLFW event API](https://www.glfw.org/docs/3.4/group__window.html)
provides the backend event operations. The actual loop/callback ordering and real
runtime races must be verified in addition to deterministic scheduler tests.

Reasons use a fixed enum and fixed arrays. At most 4,096 native activity owners
may register; exhaustion throws rather than silently disabling animation. Owners
are movable RAII leases with unique runtime tokens, never reused widget IDs. Each
owns at most one continuous-activity bit and one monotonic, one-shot deadline.
Cancellation releases ownership and notifies an armed waiter to revise its timer.
Late leases use weak state and cannot revive a deleted registration/runtime.
Same-ID moves preserve the owning native object and lease. Correlation storage is
at most two scalar records; there is no completed-tree history/replay subject.
Metric counters saturate with overflow accounting; ordering exhaustion is terminal
and never wraps or produces a false completion. Suspended surfaces retain work
without consuming deadlines until renderable again.

## Producer inventory

This inventory comes from the current native/binding/widget source, not an
assumption that publication already wakes every producer. The table describes
the delivered implementation; the following gate mapping records verification
and its remaining platform limits.

| Producer/consumer and source | Reason and lifetime | Implemented work/visibility boundary |
| --- | --- | --- |
| `XFrames::ApplyCommitOperations`, real structural subject | publication; runtime | Generation advances with revision under both tree locks; rejected/abandoned work does not create successful-publication invalidation. |
| `QueueElementInternalOp` and actual per-widget handlers | imperative; verified native subject lifetime | Visible Plot/Table/control/Map/Canvas commands share the handler boundary. Generation advances after handling under the element lock; asynchronous upload completion has its own resource invalidation. |
| `AppendTextToClippedMultiLineTextRenderer` | imperative; live target | Append invalidates under the element lock and notifies after release. |
| GLFW cursor/enter/buttons/scroll/keys/char/focus | input; renderer/window | Chained callbacks preserve ImGui handling and invalidate the scheduler. |
| Refresh/exposure, resize, framebuffer/content scale, iconify/restore/close; binding `resizeWindow` | window; renderer | Desktop window work is marshalled to the render thread; zero-sized/hidden surfaces retain pending generations. |
| `SetDebug`, `ShowDebugWindow`, `PatchStyle`, initial font/style setup | diagnostics/style; runtime | Debug focus is queued to the render thread; style patches stage and publish under visibility locks. Initial fonts are renderer-owned. |
| `RequestScreenshot` / backend capture | screenshot; renderer request | At most 32 requests; each carries its target generation. Requests complete after a covering submission or fail explicitly while unavailable/terminal/stopped. Node unavailable and populated capture gates passed at the loop checkpoint. |
| desktop `Image::RequestImage` / `PrepareFrame` | resource; Image lifetime | Widget-owned pending load replaces global ID queue/map. All jobs, including failures and clipped Images, drain before NewFrame. |
| Wasm `Image::RequestImage` completion | resource; Image lifetime | Owned cancellable fetch publishes into a weak mailbox before Resource invalidation/notification. Decode failure clears the job; replacement/removal expires owned work. |
| `MapView::FetchMissingTiles`, `CompleteTile`, `PrepareFrame` | resource; Map lifetime | Four bounded runtime workers or owned browser fetches publish bytes under a mailbox lock. Preparation drains all completed jobs, including clipped Maps. |
| Map prefetch, cache stats/progress | resource/map; Map lifetime | Shared cache/stats ownership; cumulative progress queues outside-tree delivery with a late Node JS-thread lifetime check. Both backends pump bounded owned prefetch work. |
| Map zoom debounce / fetch in `Render` | map deadline; Map lifetime | A visible Map owns the nearest 150-ms zoom deadline; preparation clears clipped activity. Failed tile attempts settle without retries until another view request. |
| QuickJS/Lua/Janet script/data operations and pending script/texture queues | imperative/resource; each Canvas lifetime | `CanvasResources` owns cancellable script/texture mailboxes and render-thread uploads. Reload/unload supersede pending work and retire replaced GPU handles. |
| Canvas per-frame script execution | canvas activity; Canvas lifetime | Default continuous execution preserves animation. `setContinuous(false)` permits settling; `redraw()` requests one explicit frame. Clipped or cleared scripts have no active owner. |
| ImGui input trickling, cursor blink, repeat, tooltip stationary/hover delays, navigation/drag/scroll and layout settling | bounded runtime owners/deadlines | Activity reflects actual pending work and timers. DeltaTime after unbounded idle/suspension is capped at 100 ms; active/deadline intervals retain monotonic elapsed time. |
| `SetDiagnosticsEnabled` / `GetDiagnosticsFrame`, state/liveness/query getters | diagnostics; runtime | Off→on requests one frame; reads/off stay pure. Scalar scheduler telemetry remains enabled independently. Fixture waits use revision/generation/sample coverage. |

Verification of these boundaries:

| Boundary | Executing verification |
| --- | --- |
| Publication, subject visibility, generation capture and later completion | `frame_scheduler_test.cpp` controlled-clock/latch tests; `xframes_test.cpp` real-subject races; ordinary Fabric lifecycle; 65-result wire parity |
| Visible imperative handlers, data preservation, style/debug and clipped text | Native tests and `transactions.ts`; ordinary PlotBar/Table streams account for all updates and final samples |
| Initial frame, queries/off/on, resize and zero framebuffer | `runtime.tsx`, `scheduling.ts`; unavailable screenshots fail explicitly |
| Input callback chaining, text, cursor/repeat and Map zoom deadline | `input.ts` with actual Windows/CDP input; Linux X11 execution pending |
| Minimize/restore, exposure and idle close | `visibility.ts`, `shutdown.ts`; pending hidden publication retains and later covers its revision |
| Image, Map, Canvas script/texture completion and decode/fetch failure | `resources.ts` controlled local responses/files, including clipped resources and all three engines |
| Static/continuous Canvas, explicit redraw, finite settling | `transactions.ts` all three engines; `activity.ts` separately measures actual moving-rectangle execution |
| Removal, replacement, abandonment, same-ID pending resources and late completion | `resource-stress.tsx`, both complete 1,000-cycle runs; immediate browser cancellation additionally covered in `resources.ts` |
| Screenshot, runtime/platform ownership and DOM listeners | Populated native/CDP captures, terminal `shutdown.ts`, actual DOM audit and Wasm Strict Mode wrapper reuse |
| Content scale and all remaining native input callbacks | Source callback audit; physical mixed-DPI monitor transition and every drag/navigation gesture are not independently driven by this fixture |

GLFW callbacks are installed before ImGui installs its chaining callbacks. The
vendored ImGui Emscripten resize callback remains installed. This slice does not
claim physical DPI/display or presentation measurements unavailable on this host.

## Historical verification and pre-change control

The following checkpoints describe intermediate sources and runs from 8 September
2026. Their pending work and build status apply to those checkpoints. The
[MVP completion boundary](#mvp-completion-boundary) and
[current acceptance audit](#current-acceptance-audit) describe delivered status.

Before changing native sources, rebuilt the Stage 3 Release Node addon using
VS2022/MSVC 14.44, then ran a separate production control with:

```powershell
$env:XFRAMES_DIAGNOSTICS_DIR = 'build/diagnostics/invalidation-control-node-idle'
$env:XFRAMES_DIAGNOSTICS_OPTIONS = '{"durationMs":1000,"warmupMs":200,"idleMs":10000,"cycles":0}'
npm run diagnostics:node -- --baseline
```

The run passed nine streaming repetitions and the current 64-result fixture. It
recorded 220 continuing idle frames over 10,002.66 ms and 5.7785% of one CPU core.
Host: Windows 10.0.26200, Ryzen 7 5700U, Node 24.14.0, production React, hardware
AMD OpenGL, 900×700 surface, local Roboto 16. Source was committed Stage 3
`2cbb37387aeab3e425867ae756f59c5e9a80dc29`; the dirty flag at build/run entry was
the user-owned updated `goal.txt`. No concurrent task-owned build ran during the
measurement. Unrelated system services were not isolated. This short-stream,
10-second-idle control is separate from the required exact regular/extended
three-repetition production comparisons; it does not replace them.

Ignored evidence: `npm/build/diagnostics/invalidation-control-node-build.log`,
`invalidation-control-node-idle.log` and
`invalidation-control-node-idle/result.json`.

The current core source passed all 362 tests in the existing VS2022/MSVC 14.44
Debug native test directory. This retains the complete 343-test Stage 3 suite and
adds 15 deterministic scheduler tests plus four real-subject ordering tests.
Coordinated latches/barriers cover publication after draw capture and concurrent
completion/invalidation; a controllable monotonic clock covers deadlines without
timing sleeps. Tests also cover repeated wait transitions, terminal state, owner
bounds, counter limits, suspended surfaces, skipped submission, pure queries and
diagnostics-disabled imperative coverage. The existing real-subject publication
visibility test and 1,000-cycle native publication test still execute. No native
unit test is being used as proof of backend submission or actual application
inactivity. Logs/XML: `invalidation-native-core.{log,xml}`; build logs:
`invalidation-native-build.log`, `invalidation-native-build-final-core.log`.

`fabric:verify`, `build:common`, `diagnostics:typecheck`, `test:lifecycle`,
`build:node` and `build:wasm` passed from the authoritative npm workspace.
The lifecycle suite still runs 21 isolated scenarios in each development and
production process, with all ten XF-LIFE gates executing. Logs use
`invalidation-core-<command>.log`. Node/Wasm package build success here is
JavaScript/package evidence: optimized addons containing the new core and the
actual backend/runtime gates have not yet been rebuilt/run. Linux native
verification also remains open for this source.

Hosted status was inspected afresh for
[Stage 3 run 34245396897](https://github.com/xframes-project/xframes/actions/runs/34245396897).
The run is now completed with failure. JavaScript, Linux native/Node and Windows
native/addon build jobs passed. Optimized Wasm build passed,
but the browser fixture failed with `Device lost (2): Device was destroyed` and
Chromium reporting no shared-image backing factory for the 900×700 WebGPU swap
buffer. Full-App browser smoke and the dependent binding parity job were skipped. Saved job log:
`npm/build/diagnostics/invalidation-stage3-hosted-wasm.log`. This repeats the prior
Stage 2 failure class; the actual cause/fix and successful hosted coverage remain open.

The external application checkout remains available. Its `AGENTS.md` was read
and `git status` confirms the untracked guidance file; no application files or
settings were changed. Current dependency declarations are React `^18.2.0`,
`@xframes/common ^0.1.6` and `@xframes/node ^0.1.13`. The real validation path is
`SerialManager` NAV-SAT events → `useNavSat` → `SignalStrengthPanel` → four
quality-band series in `PlotBar.setSeriesData`. The panel already performs the
four-band split (the older roadmap checkbox is not current application evidence).
An isolated harness with matching React/current-source XFrames, synthetic NAV-SAT
input, sustained/idle/resume and listener cleanup remains to be executed. This
source inspection is not application or physical receiver validation.

## Historical backend integration checkpoint

The desktop loop now waits indefinitely while clean and inactive, or until a real
owned deadline. Dirty/active frames use the existing vsync swap cadence. GLFW
callbacks are installed before ImGui installs its chained handlers. Browser work
uses one pending RAF and at most one deadline timeout. `PlanNext` arms browser
notifications without counting planning as an executed callback. Visibility
return explicitly restarts a callback cancelled while hidden. Both backends
retain dirty state when unavailable. Recoverable acquisition failure has three
delayed retries (16/64/250 ms), then terminal failure. Failed submissions never
complete their captured ticket. Device/error callbacks use weak scheduler state.

Capture precedes backend and per-element resource preparation under visibility
locks. Canvas queues drain even when clipped. Activity clears before traversal
and is enabled by each visible scripted canvas. QuickJS/Lua/Janet retain default
continuous animation; public refs now provide `setContinuous(false)` and
`redraw()`. Same-ID moves retain the policy and owner. Pending Canvas fetch and
Image/Map lifetime repairs remain open. Current ImGui activity covers input
trickling, layout, drag/navigation, fades, cursor blink, key repeat, hover and
wheel expiry. Interaction styles selected after drawing request a settling frame.
Idle resumption caps ImGui DeltaTime at 100 ms; active/deadline callbacks preserve
elapsed time so long hover/cursor deadlines do not drift. Deadlines use monotonic wall time. Complete real
input and hidden-tab acceptance remains open.

Screenshots have a 32-request limit and requested generations. Only a covering
submission flushes a request. Unavailable/terminal/stopped states fail requests
outside tree locks. Window sizes use a render-thread mailbox; zero suspends
drawing. Node now exposes the existing `resizeWindow` operation as Wasm does.
Style patches stage an ImGuiStyle copy before applying/invalidation under the
visibility locks. Debug focus requests are consumed on the render thread.

Common exports shared diagnostics/scheduler/correlation types. Fixture waits
capture target generation and revision after an operation and accept already
covered unchanged state. Measurements subtract lossless counters with checked
numeric conversion. New runtime gates require zero construction/submission after
settling, pure queries, suspension/publication/restoration, screenshot failure,
and continuous/static/redraw behavior for all three Canvas engines.

The checkpoint suite passed **367 native tests** (343 original, 19 scheduler,
five additional real-subject/Canvas cases). Evidence:
`invalidation-loops-native.{log,xml}`, `invalidation-loops-build-final.log`.
This preceded subsequent style/debug and hover/interaction-settling changes;
a fresh full run is required for the final source. The lifecycle suite again
passed 21 isolated development and 21 production scenarios, with all ten XF-LIFE
gates (`invalidation-loops-lifecycle.log`). Common build and diagnostic typecheck
passed after migration. Optimized Node/Wasm runtime checks are in progress. The
first Wasm build exposed Dawn's noncapturing uncaptured-error callback constraint;
it was corrected and rebuilt, not reported as a pass.

The next checkpoint passed **370 native tests**, including actual ImGui input
queue tests for finite hovered-button deadlines, deadline-only cursor blinking,
and key-repeat cancellation. Logs/XML:
`invalidation-loops-native-current.{log,xml}` and
`invalidation-loops-native-activity-build.log`. The optimized Node addon passed
the expanded ordinary fixture (three lifecycle cycles, 65 wire results), including
Canvas activity, screenshot rejection while unavailable, suspended publication,
and resize recovery. The first browser attempt exited the Emscripten runtime
when `main` returned from the paused loop; an explicit keepalive push/pop now
follows the renderer scheduling lifetime. Rebuilt Wasm then passed the same
65-result fixture and Strict Mode wrapper gate. Current checkpoint artifacts:
`invalidation-node-integration`, `invalidation-wasm-integration-current`, with
matching `*.log` files and `invalidation-loops-parity.log`. Browser idle observed
zero constructions/submissions over 511.315 ms.

A separate production desktop checkpoint used three streaming repetitions,
1,000-ms streams, 200-ms warm-ups, zero stress cycles and a 10,000-ms idle interval,
matching the earlier control's parameter choices. It passed all 65 wire results
and nine streams. Over **10,014.109 ms**, it constructed/submitted **zero frames**
and used **0.7689% of one CPU core**, below the declared 1% idle target. The prior
control recorded 220 frames and 5.7785%. This is a scheduling checkpoint before
Image/Map/Canvas lifetime integration, not the required final-source regular or
extended production baseline. No XFrames build or browser measurement ran during
this interval. Logs/report: `invalidation-node-idle-checkpoint.log` and
`invalidation-node-idle-checkpoint/result.json`.

The next resource checkpoint passed **376 native tests**. Image and all three
Canvas engines own their pending scripts/bytes/textures. `PrepareFrame` drains
completed work even for clipped elements; superseded textures are retired to the
renderer and released after older draw data has submitted, before constructing
the next frame. Always-readable `resourceState` reports owned texture counts,
retired handles, queued progress events and desktop worker activity.

Map now uses weak completion mailboxes and four runtime-owned download workers
with a 2,048-job queue. Each Map permits at most 64 pending ordinary tile loads
and four active prefetch requests. Prefetch accepts at most 65,536 uncached tiles
per call and retains the remaining accepted work until actual completions drain
it. Queued cancellation removes work immediately; at most four native HTTP calls
may finish after cancellation, bounded by the existing ten-second HTTP timeout.
They retain no widget/runtime pointer and cannot publish into a closed mailbox.
Browser Image/Canvas/Map requests use owned cancellable fetch groups. Cancellation
marks a request closed before `emscripten_fetch_close`, whose synchronous error
callback would otherwise recursively close the same allocation in Emscripten
5.0.2. Map no longer deletes textures from a mutation thread or its own draw list.

Map zoom uses an owned 150-ms deadline. A finite failed viewport attempt settles
without completion-driven retries; another view request permits retry. Prefetch
progress is cumulative and coalesced per Map/frame. It reaches application
callbacks after the tree locks are released, with a second lifetime check on the
Node JS thread. Same-ID moves preserve the Map and its requests; removal expires
both queued events and late resource completions. The native tests coordinate an
actual Map completion after frame capture, verify pending coverage, exercise
removal/ID reuse, and fill/cancel the worker queue with latches.

Evidence: `invalidation-image-canvas-native.{log,xml}` passed 373 tests;
`invalidation-map-native-final.{log,xml}` passed 376. The expanded common build and
diagnostics typecheck passed (`invalidation-resource-types-common.log`,
`invalidation-resource-types.log`). Optimized backend rebuilds and the new actual
resource fixture are still pending verification. The fixture uses a local server
with explicit held-response release after native inactivity, deterministic PNG
and script bytes, expected HTTP failures, cancellation and replacement lifetimes.
Its server handoff smoke passed independently; that is not native/backend evidence.
The first optimized resource Wasm compile/link succeeded
(`invalidation-resources-wasm-build.log`). Source inspection then found an older
decode path using uninitialized dimensions after `stbi_load_from_memory` failed;
it now returns failure before creating a WebGPU texture. The browser Map default
also leaves User-Agent to the browser instead of setting a forbidden XHR header.
An incremental Wasm rebuild is required for these two corrections. The fixture
includes malformed-image responses and narrowly recognizes only its own expected
HTTP 404s in the browser smoke; unrelated runtime/network errors still fail it.

The first actual resource Node run completed all ten resource observations,
including four tiles, failed requests, removal before completion, reused IDs and
prefetch callbacks. Textures, pending worker jobs/calls and queued progress events
returned to zero; the runtime retained its fixed four idle worker threads. It also
printed an ImGui cursor-boundary error for an Image with no texture. This is an
intermediate result, not accepted final evidence. Failed/pending Images now submit
their Yoga layout box as an ImGui placeholder item, and both runners fail explicit
`[imgui-error]` reports even when the native library logs rather than asserts.

The first browser resource run exposed a fixture deadlock: six held Canvas HTTP
requests exhausted the browser's connection limit for the same origin, preventing
the release-control request until the downloads timed out. The control endpoint
now has a separate port while sharing the bounded server state. Both corrected
fixtures are being rerun. Intermediate reports/logs are
`invalidation-resources-node-integration` and
`invalidation-resources-wasm-integration`; final checkpoint names use
`invalidation-resources-<runtime>-current`. These correctness runs overlapped
backend builds and are not performance baselines.

The corrected Node checkpoint passed 65 wire results, three ordinary lifecycle
cycles and ten resource observations, with no ImGui errors. Its 502.167-ms idle
interval constructed/submitted zero frames. The updated native suite also passed
all 376 tests, including the Image placeholder assertion
(`invalidation-resources-native-current.{log,xml}`). Script-file failure checks
for all three Canvas engines have since been added to the actual resource fixture.
Browser connection-limit diagnosis is consistent with Chromium's
[normal socket pool limit](https://chromium.googlesource.com/chromium/src/+/refs/tags/145.0.7572.5/net/socket/client_socket_pool_manager.cc).

The completed resource checkpoint now passes on both optimized backends:
`invalidation-resources-node-current/result.json` contains **11 resource gates**;
`invalidation-resources-wasm-current/result.json` contains **12**, including actual
browser cancellation/replacement. Each passed 65 shared wire results, three
streaming runs and three ordinary lifecycle cycles. The browser also passed the
Strict Mode wrapper lifecycle. Matching parity passed in
`invalidation-resources-parity.log`. Idle construction/submission remained zero
over 515.979 ms (Node) and 515.985 ms (browser). Both covering empty frames left
zero live/retired widget textures and queued progress events; Node had zero active
or queued Map calls/jobs and its fixed four idle worker threads. No ImGui errors
were reported. These are correctness checkpoints, with concurrent-build timing
limits as stated above; full resource stress, input/deadline and runtime teardown
gates remain open.

Runtime teardown work follows this checkpoint and is not yet in its backend
artifacts. `XFrames::Dispose` closes subjects, widget mailboxes/owners, pending
events and worker submission under the dispatch/tree boundary, preserving the
last native sequence/revision. Diagnostics/commit state distinguish `disposed`;
later publications reject with `runtime_disposed`. Renderer cleanup is now
polymorphic and idempotent, closes live resources before backend/context/window
destruction on both platforms, and releases the browser keepalive only after
synchronous fetch cancellation has finished. Wasm exit calls this cleanup before
its existing force-exit endpoint. Native disposal/cleanup tests and current
backend teardown gates are being verified; completed resource gates above do not
yet prove this newer shutdown path.

All **378 native tests** now pass at the disposal checkpoint
(`invalidation-disposal-native-current.{log,xml}`), including idempotent disposal,
rejected post-disposal publication, preserved revision/generation, expired Map
completion/events and polymorphic ImGui/ImPlot cleanup. Common build and diagnostic
types passed (`invalidation-disposal-common.log`,
`invalidation-disposal-typecheck.log`). The first cleanup test uncovered its own
unconditional second ImPlot destruction in fixture teardown; teardown now checks
whether the tested cleanup already released that context. Actual backend teardown
verification remains pending.

## Historical input, platform lifetime and resource-stress checkpoints

The actual input fixture now drives native window events on Node and Chromium
CDP events on Wasm. Both pass text application callbacks, cursor blink deadlines,
one held Backspace transition with ImGui repeat deadlines, and Map wheel zoom
whose debounce deadline starts new local tile requests without rescue input.
Windows input is scoped to the fixture PID and checks foreground focus after
flushing its asynchronous activation message. Initial fixture failures involved
a click below the input box, an event predicate matching earlier typing, and a
foreground activation race; those failed runs are retained as intermediate evidence.

`invalidation-input-node-focus-flush/result.json` and
`invalidation-input-wasm/result.json` passed input and actual terminal cleanup
with four outstanding Map requests. Cleanup preserved native revision/sequence,
released all scheduler owners, textures and queued events, and constructed no
unsolicited final frame. Native workers finished after the controlled responses
were released. Wasm preserves bounded terminal counters before its existing
force-exit because exports cannot be queried afterward.

`invalidation-platform-node-current/result.json` additionally passes real
minimize/restore and exposure after inactivity. A publication while minimized
remains pending and receives coverage after restoration. Platform diagnostics
count registered GLFW callbacks, browser RAF/deadline handles, the visibility
listener and pending native screenshots; all are zero at terminal cleanup.
The Windows suite passed **378 tests** again in
`invalidation-platform-native.{log,xml}`. Current common build, diagnostic types,
Fabric verification, all 21 development and 21 production lifecycle scenarios
(all ten XF-LIFE gates), and the Node library build passed in the corresponding
`invalidation-platform-*` and `invalidation-current-*` logs.

The first browser platform run passed minimize/restore, but its DOM audit found
one listener retained by Emscripten 5.0.2's `Browser.init`: `pointerlockchange`
on `document`. This is a failed teardown gate, not a passing browser checkpoint
(`invalidation-platform-wasm`). A narrow owned Wasm JS library now captures the
at-most-two pointer-lock registrations during synchronous `Browser.init` and
removes them during exit. It restores the temporary registration interception
immediately after initialization. The generated module and upstream sources are
not edited. The browser fixture audits actual DOM registrations with a bounded
256-entry list and distinguishes React's document listener from native ownership.
This fix is being verified in `invalidation-lifetime-wasm-current`.

That browser checkpoint subsequently passed: native DOM registrations fell from
25 to zero, with only React's independently owned selection-change listener
remaining. The three-cycle resource/move, input, visibility, wrapper and shutdown
gates passed, as did the 65-result Node/Wasm parity comparison
(`invalidation-lifetime-parity.log`).

Every ordinary stress cycle now additionally mounts resources through Fabric,
waits for four held Map requests, starts all three Canvas engines and texture
loads, then unmounts and checks late completion, texture/event cleanup and return
to inactivity. A separate direct-publication move gate preserves those resource
owners across same-ID moves, then proves pending Map completion still reaches the
surviving owner. Three-cycle Node verification passed in
`invalidation-resource-moves-node-current`; this is **not yet** the required
1,000-cycle evidence. The resource server reuses bounded groups with explicit
drain/reset. Linux CI's input/visibility gates now use an owned Xvfb/Openbox
session and a graceful window-manager close request. Current Linux native build
and browser resource/lifetime verification remain in progress.

`invalidation-node-stress-current/result.json` now passes all **1,000 ordinary
cycles, 1,000 abandoned candidates, 1,000 resource/activity removal cycles,
1,000 original same-ID moves and 1,000 resource/activity moves**. Original lifetime
fields returned to baseline; each resource cycle also checked scheduler/platform
ownership, textures and late completion. This correctness run overlapped builds
and is not a production performance baseline.

The first full browser stress run failed around resource cycle 350 with an
out-of-bounds callback (`invalidation-wasm-stress-current`). Inspection of
Emscripten 5.0.2 revealed that `LOAD_TO_MEMORY` alone first checks IndexedDB and
returns a fetch with ID zero during that asynchronous lookup. `fetch_close`
rejects that ID, leaving a later completion after the widget request is destroyed.
The shared Wasm loader now also requests `EMSCRIPTEN_FETCH_REPLACE`, which starts
the cancellable XHR immediately. Existing Map caches remain owned by the Map
loader; no IndexedDB cache is requested by these loaders. A same-JS-task
Image/Canvas cancellation gate was added. Current-source rebuild and stress
verification of this repair remain pending; the failed browser run is not
1,000-cycle evidence.

## Isolated ubx-monitor application validation

The available original `C:/dev/ubx-monitor` checkout was preserved, including its
untracked `AGENTS.md` and machine-local settings. An ignored clone at
`npm/build/diagnostics/ubx-current/app` installed locally packed current-source
common/Node packages with React 19.2.3. Narrow isolated migrations changed three
text-color styles to `colors[ImGuiCol.Text]`, handled nullable config parsing,
and used a focused Bundler-resolution TypeScript check with matching Node types.
The original application's whole-project Node16 typecheck failed on its existing
extensionless imports and dependency/type mismatches; the actual signal panel's
transitive import graph passed the focused check after those changes.

`diagnostics/ubx-telemetry.ts` passed **202 decoded NAV-SAT messages and 202 native
PlotBar updates**. Synthetic checksummed receiver bytes were split across the
actual SerialManager data listener and actual UbxParser, then flowed through the
application's `useNavSat` subscription and `SignalStrengthPanel` into all four CNO
quality bands. It verified 200 sustained samples at 20 Hz, one second of inactivity
with zero continuing frames, resumed updates, ordinary unmount, zero retained
telemetry listeners, and no invalidation from a late packet. This is actual
application code with synthetic input, not physical serial/hardware validation.

Evidence: `ubx-current/evidence/result.json`, `telemetry.png`, `run-current.log`,
and `signal-typecheck-final.log`. The sustained interval was 10,007.987 ms;
observed-frame latency p50/p95/p99/max was 16.175/31.300/32.484/60.334 ms. This
correctness run overlapped native builds, so these timings are **not** an isolated
production performance baseline. An initial harness assertion checked React's
passive-effect cleanup too early; the corrected bounded listener wait passed.

The current optimized Wasm cancellation repair passed its short complete fixture
(`invalidation-fetch-cancel-wasm-current`), including the new same-task fetch
cancellation gate. Linux GCC 13.3 Release passed all **378 native tests**
(`invalidation-linux-platform/native.xml` and `build-tests.log`). Browser full
stress and Linux application execution are being verified separately.

## Final production observations

Runs used optimized current-source artifacts, production React 19.2.3, three
repetitions and the exact regular/extended row/point/rate/duration/warm-up/idle
parameters required by the Stage 3 comparison. Workloads ran sequentially; the
task-owned Linux build was paused for each measurement group and resumed between
groups. Other host services and foreground conditions were not isolated.
Host: Ryzen 7 5700U, Windows 10.0.26200, Node 24.14.0; Node used AMD hardware
OpenGL and browser used SwiftShader WebGPU. Source: dirty Stage 4 working tree
based on `2cbb37387aeab3e425867ae756f59c5e9a80dc29`.

| Runtime/workload | Input Hz | Achieved Hz | Observation p95 ms | p99 ms | Maximum ms |
| --- | --- | --- | --- | --- | --- |
| Node regular | 20 | 19.92–19.99 | 16.20–16.25 | 16.38–16.69 | 16.69 |
| Node regular | 60 | 59.88–59.95 | 16.52–30.82 | 17.23–31.55 | 31.55 |
| Node regular | 120 | 95.88–96.36 | 16.63–16.92 | 16.97–17.53 | 31.05 |
| Wasm regular | 20 | 19.95–19.97 | 21.27–23.51 | 23.75–35.89 | 35.89 |
| Wasm regular | 60 | 59.81–59.99 | 27.10–28.65 | 36.27–42.55 | 44.80 |
| Wasm regular | 120 | 82.81–100.01 | 28.14–34.60 | 39.44–44.14 | 44.91 |
| Node extended (partial run) | 20 | 19.70–19.92 | 15.88–16.32 | 16.43–46.79 | 46.79 |
| Node extended (partial run) | 60 | 59.30–59.90 | 17.07–31.46 | 30.97–31.80 | 31.80 |
| Node extended (partial run) | 120 | 92.42–98.95 | 16.61–16.83 | 16.78–17.38 | 17.38 |
| Wasm extended | 20 | 19.32–19.91 | 33.35–539.41 | 36.36–539.41 | 539.41 |
| Wasm extended | 60 | 58.85–59.72 | 31.69–42.94 | 36.45–56.92 | 56.92 |
| Wasm extended | 120 | 69.01–75.77 | 33.49–52.20 | 54.39–67.17 | 67.17 |

Ranges describe separate repetition percentiles, not pooled samples. All nine
streams per workload account for every produced update, including final samples,
with zero timed-out updates. Node extended subsequently failed editor focus and
remains an overall failed run. Other three comparison runs passed completely.
Reports preserve p50, native state age, stage costs, observed/coalesced sample
counts, requested React updates, publications/native calls/revisions and frames.
The 14-node structural workload still accounts for 240 requests, 200 publications
and 40 bailouts per repetition; scheduling does not replace that batching proof.

Separate 10-second idle/activity runs passed completely on both backends. Node
constructed/submitted zero idle frames over 10,014.22 ms at 0.1498% of one core;
five real platform opportunities constructed no frames. Browser constructed and
submitted zero frames over 10,008.70 ms, with zero opportunities, RAF handles or
deadline timers. Browser CPU is unavailable. Active moving-rectangle Canvas
submission was 59.35/59.97 Hz on Node and 60.05/60.06 Hz in browser with expensive
diagnostics off/on. Node active CPU was 17.64%/19.05% of one core. These ordered
observations quantify cost without establishing an isolated causal difference.

The pre-change Node control constructed 220 idle frames over 10 seconds at 5.78%
of one core. Zero continuing frames is established; CPU/throughput comparisons
remain subject to shared-host variation. Relative to Stage 3, Node regular
120-Hz input increased from 76–78 to about 96 Hz, while Wasm throughput decreased
and its extended maximum increased from 283.67 to 539.41 ms. There is no uniform
timing win. In the Node regular run, requested producer sleeps were usually zero
but actual awaited delays had p50 around 14.4–14.6 ms, while three imperative
calls and diagnostic queries were sub-millisecond at p95. In Wasm's extended
outlier, the three synchronous imperative calls themselves took 520.55 ms of
the 539.41-ms observation. These stage measurements locate substantial costs
outside waiting for a new scheduler frame; they do not isolate OS timer, host
load, allocation or table costs causally. Per the user's clarification, no further
optimization was pursued to force a 120-Hz result in these conditions.

Evidence is under `npm/build/diagnostics/invalidation-production-{node,wasm}-`
`{regular,extended,idle-activity}-current/result.json`, with driver logs and
screenshots. The initial regular reports and Node extended report named the
constructed-to-submitted interval `capturedToSubmitted`; its calculation starts
at `constructedAtMs`, after construction. The fixture label is corrected to
`constructedToSubmitted`; these older raw fields must be interpreted accordingly.

## Current acceptance audit

This table records the original MVP acceptance evidence. Its Linux application
and hosted CI limits are superseded by the focused stabilization results below.

| Requirement group | Result and evidence |
| --- | --- |
| Shared model, coherent capture, sleep/completion races, rejected work, terminal states, counters and bounded ownership | Implemented in shared `frame_scheduler` and real-subject visibility boundaries. Controlled-clock/latch tests execute in both 378-test native suites. Ordering is independent of optional snapshots. |
| Desktop event/deadline loop and browser paused RAF loop | Both current optimized runtimes pass initial/idle/wake, zero surface, retained hidden publication, restore, input, deadline and idle-close gates. No periodic rescue frame. |
| Publication and visible imperative data | Ordinary Fabric and direct parity retain schema 2, surface 0, synchronous final reachability cleanup, same-ID survival, committed ownership and quarantine. All streamed updates/final samples are accounted independently from frames. |
| Image/Map/Canvas completion and lifetime | Controlled actual files/HTTP, upload/decode/failure, clipped resource queues, prefetch callback, Map zoom, Canvas static/continuous/redraw, removal/replacement and late completion pass both runtimes. |
| Correlation, diagnostics and measurement boundaries | Decimal frame/revision/generation, current pending state, bounded reasons/counters and pure reads are exposed by both bindings. Coverage/sample waits replace newer-frame polling. CPU/presentation limitations are explicit. |
| Required JS/package gates | Fabric verification, common build, 21 development + 21 production lifecycle scenarios (all ten XF-LIFE IDs), diagnostic types, Node library and final Wasm library builds pass. Final diagnostic-only additions pass `invalidation-mvp-typecheck.log`. |
| Current optimized backends and native tests | VS2022 Release Node and Docker/Emscripten 5.0.2 optimized Wasm builds pass. Windows Debug native suite and Linux GCC 13.3 Release suite each pass 378 tests. Linux actual Node application build was stopped at the user's wrap-up instruction; execution is unverified. |
| Complete runtime stress and parity | `invalidation-node-stress-current` and `invalidation-fetch-cancel-wasm-stress` pass 1,000 ordinary cycles, abandoned candidates, resource removals, original moves and resource/activity moves each. `invalidation-stress-parity.log`: 65 matched results. No GC/process-exit cleanup substitute. |
| Ordinary applications and wrapper | Node development/production current App smokes and full-App browser production smoke pass; populated screenshots inspected. Actual Wasm Strict Mode wrapper lifecycle passes. Browser native DOM listener count returns from 25 to zero at terminal disposal. |
| External application | Isolated actual ubx-monitor parser/subscription/CNO chart path passes sustained synthetic telemetry, idle/resume and cleanup; original checkout preserved. Hardware/physical serial validation unavailable. |
| Comparable and additional measurements | Regular Node/Wasm, extended Wasm and both separate 10-second idle/activity runs pass. Extended Node completed measurements but failed its later editor-focus gate. Shared-host timing is provisional per the clarified acceptance interpretation. |
| CI and documentation | Workflow runs shared/native and real producer/input/visibility/lifetime/parity gates, plus separate inactivity/activity and full-App modes. Failure artifacts/timeouts and extended commands remain. Architecture, embedding, diagnostics and roadmap describe the implemented boundary; hosted evidence is separately limited below. |

The final Wasm fix passes the complete 1,000-cycle run; the earlier out-of-bounds
run remains failed historical evidence. The initial full-App Node smoke loaded a
stale `src/lib` addon after a manual CMake build; copying the current artifacts
resolved it. A first browser production smoke rebuilt during another package
build and timed out waiting for its bundle; sequential current execution passed.

The first provisional Node regular run completed its nine streams but failed the
subsequent foreground-focus check. The helper now temporarily joins both target
and foreground input queues and still verifies real foreground ownership. A
separate input/activity check and the final regular run passed. The Node extended
run completed all nine streams, then failed editor-click focus; its timing data
are partial-run evidence and its overall result remains **failed**. Desktop
interaction conditions are a remaining fixture qualification limit; these results
must not be silently relabeled as complete runtime passes.

### Hosted and milestone limits

The starting hosted evidence reviewed on 11 September 2026 was
[Stage 4 run 34275033947](https://github.com/xframes-project/xframes/actions/runs/34275033947),
head `1de105f1d8479337e78a083b8c6645416c301c23`, completed **failure**.

| Hosted job | Result |
| --- | --- |
| JavaScript | Passed Fabric verification, common build, diagnostic types and lifecycle checks. |
| Windows native | Passed native tests, Node addon compilation and package build. |
| Linux native/Node | Native tests, addon compilation and package build passed. The runtime fixture failed before launch because `xprop` was missing; its X11 window-manager readiness check timed out. Inactivity/activity and full-App smokes were skipped. |
| Wasm/WebGPU | Optimized native build, package build and diagnostic types passed. The runtime fixture failed at the initial empty-frame observation with `terminal renderer backendFailed`; inactivity/activity and full-App smoke gates were skipped. Its cause was diagnosed during the focused pass below. |
| Node/Wasm parity | Skipped because prerequisite runtime jobs failed. |

The earlier [Stage 3 run 34245396897](https://github.com/xframes-project/xframes/actions/runs/34245396897)
also failed its browser gate. These runs remain failed historical evidence. The
focused pass below fixes the fixture prerequisites and graphics setup and
establishes successful hosted runtime coverage on its final code revision.

### Focused CI stabilization, 11 September 2026

The fresh artifact review of run `34275033947` located the first browser failure
in `wasm/browser.log`: Chromium could not find a `SharedImageBackingFactory` for
the WebGPU canvas swapchain, then failed to create that shared image. The device
lost message in `page.log` (`A valid external Instance reference no longer
exists.`) and the scheduler's terminal `backendFailed` followed that failure.
Adapter/device creation and the native ready callback had already succeeded.

A small, plain JavaScript WebGPU canvas reproduction in Ubuntu 24.04 Docker,
Chrome `153.0.8010.36`, reproduced the same shared-image error without XFrames or
Wasm. Selecting Vulkan SwiftShader and ANGLE SwiftShader together fixed canvas
submission and a captured red square. Vulkan-only and ANGLE-only attempts failed.
The passing configuration also worked without Xvfb, `--enable-gpu` or a GPU
blocklist override. The browser harness therefore adds only
`--enable-features=Vulkan`, `--use-vulkan=swiftshader`, `--use-angle=swiftshader`
and `--enable-unsafe-swiftshader` for its Linux SwiftShader mode. This is a fixture
graphics-setup correction; no renderer fallback, recovery, polling, retry or
submitted-frame semantics changed. Original stderr/page/result artifacts remain
preserved by the existing harness and workflow.

Linux startup now explicitly installs `x11-utils` and `xauth`. The owned X11
wrapper checks all seven required commands before starting either process and
prints an actionable Ubuntu install command. A deliberately missing `xprop`
failed immediately, and the real Xvfb/Openbox readiness path passed.

Once Linux Node could run, its input gate exposed another fixture defect:
Ubuntu's `xdotool 3.20160805.1` hangs on repeated window-relative
`mousemove --sync` at the same position. A separate owned-X11 reproduction
timed out that command after two seconds and completed the identical move
without `--sync`. The input helper retains ordered motion/click requests on one
X connection, its command watchdog, and all native event/focus/frame assertions;
it removes only that tool's motion wait. No artificial extra motion is injected.

Local evidence under `npm/build/diagnostics/`: `ci-probe-*` records the browser
comparison and screenshots; `ci-xdotool-probe.log` records the repeated-motion
reproduction; `ci-linux-build.log` records completion of the cached current-source
Linux addon/package build. `ci-wasm-linux-current/result.json` passes the full
short fixture with three lifecycle/resource/move cycles, nine production streams,
all producer/input/visibility/publication/shutdown gates and the real wrapper.
Its populated screenshot was inspected. It reused the existing optimized
current-source Wasm artifact. The root-only local container uses a temporary
Chrome `--no-sandbox` wrapper; hosted execution uses Chrome directly.

`ci-node-input-current/result.json` also passes the complete short production
fixture after the input-helper fix: three lifecycle/resource/move cycles, nine
streams, all 11 resource gates, input, visibility and terminal shutdown. The
browser report contains 13 resource gates and passes its wrapper lifecycle.
Both report zero continuing idle frames. The existing parity command passes
65 shared binding results against these two reports. Diagnostic typechecking,
browser-script syntax, shell syntax and whitespace checks pass.

The initial local browser attempt timed out while gathering Git metadata across
the Windows-mounted submodules. An affected Node metadata scan was stopped.
Those are failed/aborted local setup attempts, not runtime passes; configuring
the reproduction container's Git to omit unchanged submodule scans allowed the
current runs to start. The first executing Node run remains failed at the
repeated-motion command (`ci-node-linux-current`).

The four-file code patch is committed as
`fa12b2255fd1fed5a366c137f4003323c69475ee` on
`codex/stage4-ci-stabilization`. Its
[application run 34623926577](https://github.com/xframes-project/xframes/actions/runs/34623926577)
completed **success** on 11 September 2026. All five jobs and every required
runtime gate executed and passed on that revision:

| Hosted job/gate | Executed result |
| --- | --- |
| JavaScript | Fabric verification, common build, diagnostic types and all 21 development + 21 production lifecycle scenarios passed, covering all ten XF-LIFE defect IDs. |
| Windows native/addon/package | All 378 native tests passed with zero failures, disabled tests or errors. VS2022 addon compilation and Node package build passed. |
| Linux native/addon/package | All 378 native tests passed with zero failures, disabled tests or errors. GCC addon compilation and Node package build passed. |
| Linux Node/Mesa runtime | Scheduling, all 11 resource gates, real input, visibility, Fabric publication, lifetime/moves, diagnostics-disabled behavior and terminal cleanup passed. |
| Wasm/Chromium runtime | Optimized Docker/Emscripten 5.0.2 build (`XFRAMES_FAST_BUILD=OFF`), package build and types passed. Chromium 152 with SwiftShader passed scheduling, all 13 resource gates, input, visibility, publication, lifetime/moves, the ordinary wrapper and terminal DOM-listener cleanup. |
| Separate Node and Wasm inactivity/activity | Both passed: zero constructed/submitted idle frames over 10,002.64 ms and 10,001.83 ms respectively. Each also executed two 10-second continuous-Canvas intervals, with diagnostics off and on, then returned to inactivity. |
| Full-App smokes | Linux Node development, Linux Node production and browser production all passed. Their populated screenshots were inspected, as were both runtime fixture screenshots. |
| Node/Wasm parity | Executed against this run's native-Linux and Wasm artifacts; all 65 shared results, populated widget/hierarchy/Yoga state and final native destruction state matched. |

Both primary runtime reports contain **1,000 ordinary lifecycle cycles, 1,000
abandoned candidates, 1,000 resource/activity removal cycles, 1,000 original
same-ID moves and 1,000 resource/activity moves**. All ten lifetime deltas are
zero. Each report accounts for nine production streams. The separate activity
reports submitted 3,330/3,298 Node frames and 438/431 browser frames with expensive
diagnostics off/on. These shared-host software-rendering observations are
informational; no throughput or presentation qualification is inferred.

No required gate failed or was skipped. Existing platform conditions skipped the
Linux-specific setup/tests/addon/rendering steps in the Windows job and the
Windows-specific unit/addon steps in the Linux job. The optional extended steps
were skipped because this was a normal push run; the manual extended mode and
the mandatory 1,000-cycle workloads are preserved.

The runtime reports identify source SHA `fa12b2255fd1fed5a366c137f4003323c69475ee`.
The Node reports retain their post-build `sourceDirty: true` metadata; the
existing native CMake build regenerates and copies the tracked Janet core image.
Wasm records `sourceDirty: false`. Checkout logs and fresh addon/Wasm build steps
establish the revision provenance; dependency caches do not supply application
binaries. No implementation changes followed the validated code commit.

Authoritative artifacts on the run are `javascript-lifecycle`,
`native-windows-2022`, `native-ubuntu-24.04` and `wasm-webgpu`, plus the parity job
log. Downloaded evidence is under `npm/build/diagnostics/ci-34623926577/`:
`run.json`, `run.log`, native XML, the four runtime `result.json` reports and
screenshots. `audit.json` records the requirement checks against those artifacts,
including matching source revisions, actual step outcomes, stress counts,
inactivity/activity intervals and parity. This completes the focused CI pass.

The Stages 0–4 milestone remains open for controlled performance qualification,
consistently green hosted CI and broader hardware/application evidence. The CNO
application gate does not establish combined Plot/Table/Map/Canvas delivery value
or physical byte-to-pixel latency. Mixed-DPI hardware transitions, every native
drag/navigation gesture, hardware WebGPU and presentation are not independently
measured here. Resolve these evidence gaps before expanding into Stage 5 replay
or Stage 6 general automation.

## Completed bounded application qualification slice

The [ordinary ubx-monitor application record](ubx-monitor-application-2026-09.md)
extends the isolated CNO evidence to the complete App through its public Node
render/disposal entry. It covers current-package integration and whole-project
typechecking, sustained synthetic NAV-SAT/NAV-PVT input, visible
Plot/Table/Map/Canvas interaction, connection lifecycle, disconnected inactivity
and cleanup. The bounded Windows Node/OpenGL slice passed on 11 September 2026
with synthetic receiver bytes, deterministic local tiles and preserved original
application settings. The record retains the reproducible patch/setup and results.
Controlled performance, broader hardware evidence and sustained CI history remain
separate milestone gaps; the completed CI stabilization is not being reopened.

The subsequent [UI pacing slice](ubx-monitor-streaming-2026-09.md), committed in
XFrames `880506e` with isolated app `817f931`, adds persisted 10/20/60 display
rates and source-owned histories. Final production rate-change/cleanup checks
and [three quiet-window cost pairs](ubx-monitor-pacing-cost-2026-09.md) passed.
The comparison reports lower snapshot work and measured CPU with higher observed
latency. It changes no shared runtime or native scheduler behavior and does not
close the separate framework latency, hardware or ongoing CI criteria above.
