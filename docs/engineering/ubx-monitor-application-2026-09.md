# ubx-monitor ordinary application qualification

Status: bounded Windows Node/OpenGL application slice passed on 11 September
2026. The executable task is in [goal.txt](../../goal.txt). Broader milestone and
controlled performance qualification remain open.

## Purpose and starting boundary

Validate the complete desktop application with current XFrames packages after
the [Stage 4 scheduler and CI stabilization](fabric-invalidation-2026-09.md).
This advances the [Stages 0–4 application criterion](../../ROADMAP.md#milestone-review-gate--after-stages-04).
It does not close the broader milestone or qualify controlled streaming performance.

The completed CI pass at `fa12b2255fd1fed5a366c137f4003323c69475ee` passed all five
jobs in [run 34623926577](https://github.com/xframes-project/xframes/actions/runs/34623926577).
Its full-App smokes exercise the XFrames demo application. The separate
[ubx-monitor check](fabric-invalidation-2026-09.md#isolated-ubx-monitor-application-validation)
used a custom diagnostic host to mount `SignalStrengthPanel`, with 202 decoded
NAV-SAT messages and native PlotBar updates, including 200 sustained samples over
about 10 seconds. That remains useful panel evidence. It does not establish
ordinary startup, complete application typechecking, combined panel use or the
full application's activity and disposal behavior.

Read-only inspection of `C:/dev/ubx-monitor` on 11 September found revision
`571f556` (`Tab indicators`), with only an untracked `AGENTS.md` in Git status.
Ignored settings and assets remain machine-local. Relevant source facts are:

- `src/index.tsx` calls the public `render(App, assetsBasePath, fontDefs, theme)`.
- The manifest specifies React 18 and older published common/Node packages.
  The previous isolated validation required React 19.2.3, style/config migrations
  and a focused TypeScript check; the original whole-project check did not pass.
- `SignalStrengthPanel` already divides tracked satellites into `<20`, `20–<30`,
  `30–<40` and `>=40` CNO bands, with legend and sorting. A panel rewrite is no
  longer the next task.
- `App` includes the signal PlotBar, message Table, Map with position/trail/accuracy
  overlays, and the sky-view JsCanvas in separate tabs.
- `useDataActivity` publishes a new activity object every second, and
  `useUbxMessages` updates elapsed time and bandwidth. `SkyViewPanel` loads a
  data-driven script without opting out of the Canvas default continuous mode.
  Their full-app rendering consequences need measurement, not an assumed zero-frame
  assertion whenever telemetry pauses.

Recheck these facts at execution time and record any source changes. Do not copy
the original checkout's local serial configuration into the validation setup.

## Deliverable and integration boundary

Prepare an isolated application checkout with locally packed current-source
`@xframes/common` and `@xframes/node`, matching React/types, and a passing
whole-project typecheck. Preserve the original checkout. Retain the necessary
application migrations and setup as a reviewable patch/script or separate
application branch with reproduction instructions; ignored evidence alone is not
the deliverable. Keep the XFrames authoritative workspace lockfile and generated
Fabric snapshots unchanged unless a demonstrated prerequisite requires a change.

Launch the actual App through the ordinary public Node render path, preserving
its event dispatch and disposal. A narrow diagnostic entry may retain the public
render function's disposer and observe native state. It must not replace the
application with a diagnostic composition or initialize a parallel Fabric host.
Use a deterministic serial transport substitute through the real connection,
parser and subscription path. A focused transport seam is allowed; rebuilding the
serial stack or adding a general automation framework is outside this slice.

Keep current component behavior. Fix proven compatibility, lifecycle or redundant
update defects at their owner. The sky canvas may adopt the existing
`setContinuous(false)` contract after checking that its script is data-driven and
that data, resize and resource changes still repaint. Preserve visible stale-time
updates, actual animations and resource completion behavior.

## Required bounded scenario

Use a production Windows Node/OpenGL build with recorded source/package identity.
The minimum workload is 1,200 checksummed NAV-SAT and 1,200 NAV-PVT messages,
scheduled over at least 60 seconds at a requested 20 Hz per message type. Input
delivery must run independently of frame waits. Record elapsed time, actual
delivery rate, decoded counts, application updates and observed frames; account
for deliberate UI/frame coalescing and verify final sample state. Do not silently
discard source messages or infer 20 rendered Hz from 20 input Hz.

Split some packets across transport callbacks and combine others in one callback.
Use synthetic positions and deterministic local map tiles/resources with recorded
identities so that public tile-service availability is not an acceptance dependency.
Exercise real native decoding/upload/render paths. Keep the actual sky-view script.

| Gate | Required evidence |
| --- | --- |
| Integration | Whole-app typecheck and ordinary development startup pass with the packed packages, assets and theme; production scenario uses the same application integration. |
| Full application | Ordinary input visits Signals, Messages, Map and Sky View. Each has semantic populated state and an inspected screenshot; hidden state alone does not prove visible rendering. |
| Interaction | CNO sort or message filtering works through actual event dispatch; map pan/zoom updates the visible view and retains position/trail/accuracy overlays. |
| Sustained telemetry | Both message types traverse transport, actual parser and subscriptions. All scheduled messages are accounted for, retained histories stay within their declared limits, and the final visible data is current. |
| Pause and resume | Rendering settles between genuine status/deadline/resource changes; paused connected stale-time updates remain truthful. Resumed telemetry wakes the application and reaches submitted state. |
| Disconnect and reconnect | Connection state, subscriptions and parser/transport ownership remain correct in the same runtime; repeated connection use does not accumulate listeners or lose resumed input. |
| Disconnected inactivity | After status, input and resource work settles, a 10-second disconnected observation has no continuing constructed/submitted frames. Fix redundant app updates rather than suppressing native invalidations or disabling visible behavior. |
| Disposal | Await the ordinary render disposer and React effect cleanup. Native elements, hierarchy, subjects, JS mappings/registrations, telemetry listeners and scheduler/resource owners return to their documented empty baseline. Late old-transport packets and pending resource completions cannot revive the disposed tree. |

Take cleanup measurements before process exit; forced termination is not disposal
evidence. Use bounded waits and retain errors/artifacts on failure. If the full
application exposes a proven XFrames defect, add a focused regression check and
run the affected shared Node/Wasm gates after repairing it.

## Validation, evidence and stopping rule

Record both repository SHAs and dirty changes, the application migration patch,
packed package identities, native build provenance, React/Node/toolchain versions,
OS/GPU/backend, assets, workload parameters and exact setup/run commands. Retain
semantic reports, screenshots, logs and measurements under ignored diagnostics
output; summarize actual passed/failed/unavailable checks in this record when run.
Report receipt-to-observed-submission p50/p95/p99/maximum with sample and coalescing
coverage when measurable. Do not label that interval physical byte-to-pixel or
presentation latency, or treat shared-host timing as a performance qualification.

Reuse the completed CI evidence for unchanged XFrames code. Run affected package,
Fabric, diagnostics and native checks for changed seams; shared runtime changes
need relevant coverage in both bindings, and workflow changes need affected
hosted validation. Keep the existing 1,000-cycle gates and optional extended mode.
Do not repeat complete builds or benchmark matrices solely to refresh timestamps.

Stop after one reproducible complete application scenario, necessary focused fixes
and evidence. A required blocked gate remains open. Other desktop platforms,
configurable UI update rates, controlled streaming targets, broader Map/Canvas
benchmarks, physical serial, hardware WebGPU, mixed-DPI/presentation and sustained
CI history are later qualification work. Stage 5 replay and Stage 6 general
automation remain deferred until the milestone review.

## Integration artifacts and executed prerequisite checks

The reviewable application changes are retained in
[`ubx-application.patch`](../../packages/dear-imgui/npm/diagnostics/ubx-application.patch).
[`ubx-application-setup.mjs`](../../packages/dear-imgui/npm/diagnostics/ubx-application-setup.mjs)
clones the original application, applies that patch, builds/packs the source
common/Node JavaScript packages around the current Release native binary,
installs the isolated dependency graph, and runs the complete typecheck and
serial lifecycle regression. It refuses to replace an existing checkout and
records original settings/instructions/dependency hashes without copying serial
settings into the integration. No registry release is involved.

The application base is `571f5569bb923c3d4a8f37db8f8ada555323667a`; XFrames is
`5b76c83e5e5267522bed9935959e816c75573304` plus the reviewable working-tree
changes. Local package versions remain common `0.1.7` and Node `0.1.14`; version
numbers alone do not identify these local builds. The setup records tarball
integrities, native SHA-256, repository dirty diffs, font/script hashes and commands.
The original application's status remains only `?? AGENTS.md`.

After validation, the application changes were committed as
`e7de9e25ff3f608e8bb2613e471d0b97f47d58f2` on
`qualification/current-xframes-application`. That branch is also available in
`C:/dev/ubx-monitor`; its checked-out `main`, settings and dependencies remain
unchanged. This qualification branch still requires the locally packed tarballs
prepared by the setup script.

Demonstrated prerequisites and their owners:

- Application: React `19.2.3`, matching React/Node types, Bundler resolution for
  the existing `tsx` extensionless imports, nullable configuration parsing and
  current `colors[ImGuiCol.Text]` styles. Current XFrames does not expose the
  application's `TabItem.indicator`; the migration preserves activity/status
  colors through the supported tab text style. The existing four-series CNO
  feature is retained. The original tracked fonts and sky script are retained.
- Application: a narrow serial transport interface leaves connection state,
  real `UbxParser.feed`, subscriptions and event callbacks intact. A reproduced
  disconnect left three port listeners and ten parser listeners, and a late
  byte callback threw on the cleared parser. Cleanup now detaches owned listeners,
  guards superseded open callbacks and closes late opens. Generic parser messages
  are forwarded once. A five-opening regression covers late bytes, late opens,
  stale closes, reconnect and exact ACK delivery, with zero retained port listeners.
- Application: unchanged data-activity objects no longer trigger a publication
  each second. Message rate decays during a pause and resets on disconnect;
  connected stale seconds continue updating. A visible static sky probe generated
  91 frames in 1.5 seconds with continuous activity, so the panel now explicitly
  selects `setContinuous(false)` for its unmodified data-driven script.
- Application: Console's append-only history was unbounded. Its existing widget
  now rotates lifetime when JS history exceeds 65,536 UTF-16 code units, retaining
  the latest 32,768 for replay. This bounds the native buffer too; the conservative
  UTF-8 bound is 196,608 bytes. Message rows remain capped at 500, position histories
  at 3,000 and the Map trail at 1,000 points.
- XFrames common: the public ImGui color enum had drifted from the native header,
  visibly miscoloring tabs and later theme entries. It now matches the current
  native order and aliases. The ABI regression compares all 65 names/aliases with
  the actual dependency header.
- XFrames native: inactive tab headers used Yoga cursor positioning without
  submitting content, causing the ImGui parent-boundary error. Tab headers now
  retain ImGui's own cursor flow; the regression exercises actual tab frames.
  Bounded native diagnostic summaries expose Map overlays/trail and retained
  PlotLine/PlotScatter/Console sizes so application assertions can inspect native
  state rather than infer storage from command counts.

Completed checks: whole-app typecheck and five-opening serial lifecycle test
in a fresh final reproduction; literal development `npm start` and normal window
close (exit 0); production full-App scenario and awaited disposal; all 380 Windows
native tests in 35 suites, including the two added regressions; 21 bridge lifecycle
scenarios in each development/production mode; common build, Fabric/65-name ImGui
ABI checks and diagnostic typecheck; final rebuilt Node and optimized Wasm short
runtime suites; and 65-result binding parity. The short suites retain their three
streaming rates, three lifecycle cycles, native input, activity, resources and
shutdown checks. Existing hosted 1,000-cycle gates and optional extended mode are
unchanged; this working tree does not claim a new hosted pass.

Early probes are diagnostic evidence, not full acceptance: some mouse clicks
missed while the laptop was in use. Subsequent probes separate pointer movement
from clicking, verify selected tab content and retry missed targets. Inspected
captures show the four populated panels. Desktop runs must be announced because
the PID-scoped Windows helper moves the real cursor and acquires focus.

Evidence is under `packages/dear-imgui/npm/build/diagnostics/ubx-application/`,
with the final independent setup under `ubx-application-reproduction-final/`. The output
directories are ignored; the patch, setup, byte generator and application harness
are repository artifacts. Timing is informational on this actively used laptop.

## Completed production scenario

`production/result.json` reports `passed`, including the ordinary native shutdown
callback; `production.log` records exit 0 with no ImGui errors. This is one complete
App, one public Node renderer and its returned disposer. The harness observes the
real commit/internal-operation paths without replacing them. Its transport emits
wire bytes into the actual SerialManager and UbxParser. It does not inject hook
state. The local map fixture is a deterministic 2-by-2 color PNG, so the rendered
background is a repeated gradient rather than geographic cartography.

| Acceptance | Executed evidence |
| --- | --- |
| Complete visible application | Inspected `signals-sorted.png`, `messages.png`, `map.png`, `map-zoomed.png` and `sky-final.png`, with matching semantic/native JSON. Four quality bands, 500 message rows, loaded local map textures and position/trail/accuracy overlays, and four satellites drawn by the unchanged sky script are populated. |
| Native interaction | Signals checkbox sorts the four bars descending through the real callback; map wheel changes zoom 15 to 16, preserves all three overlay types and updates the displayed zoom. The zoom capture precedes tile completion and shows the loading state; the earlier map capture shows decoded/uploaded tiles. No input retry was required in the production run. |
| Telemetry | 1,200 NAV-SAT plus 1,200 NAV-PVT packets scheduled independently of frame polling over 60,000 ms; delivery finished after 60,015.993 ms, or 19.994670 Hz per type. Warm-up, resume and reconnect add three packets per type: 1,203 decoded each and 2,406 generic messages, with unique sequence IDs. |
| Chunk boundaries | Across the session, 601 packet pairs use a split NAV-SAT prefix followed by the remainder plus NAV-PVT; 602 pairs are coalesced into one callback. Total: 1,804 raw callbacks and 197,292 bytes. No source packet loss or duplicate decoded sequence was accepted. |
| Bounded history | Observed high-water: 500 messages, 1,000 map trail points, 1,200 position/scatter points and 1,200 line points across retained series, and 67,322 native Console bytes. Declared limits are asserted throughout; a focused native test separately crosses the plot and map limits. Parser pending bytes were zero at observation polls; this is not its instantaneous peak during a split callback. |
| Pause/resume and Canvas wakes | After connected activity settled, stale-time text advanced during a 2.2-second observation with two submitted frames and zero continuous owners. Resumed data reached both observed native sequences. The real static sky script repainted for data and resize; a local Canvas texture completion also produced a covering frame with one live texture. |
| Connection lifecycle | Native disconnect/reconnect callbacks changed state in the same App. Old transport/parser listeners reached zero; application listener counts did not grow after reconnect. Both resumed packet types reached native state. |
| Disconnected inactivity | After input, stale status and resource work settled, the 10-second disconnected observation constructed and submitted zero frames. The capture happens after the measured interval. |
| Disposal and late work | Four real HTTP map requests were held pending at unmount. After awaited public disposal/effect cleanup, old ports received late bytes/close events and held resources were released. Native generation stayed unchanged during the settled late-work check; resources and ownership reached the baselines below. Normal window close then completed native shutdown. |

The sustained interval submitted 3,498 frames. Its frame count includes ordinary
input, status and resource activity; it is not a promise of 20 rendered frames
per second. Session operation observations include 1,200 Table data replacements,
1,200 each of map marker/overlay/trail updates and Canvas data updates, 1,201 bar
series changes, 1,200 scatter replacements and 3,600 line appends. React may
coalesce decoded messages before widget publication. `production/stream.json`
retains receipt and native observation records, including the final scheduled
sequence check for both message types.

| Receipt to observed submitted state (ms) | Samples / scheduled | Unobserved or coalesced between polls | p50 | p95 | p99 | Maximum |
| --- | --- | --- | --- | --- | --- | --- |
| NAV-SAT / bars | 1,176 / 1,200 | 24 | 35.711 | 53.652 | 67.507 | 114.698 |
| NAV-PVT / map position | 1,174 / 1,200 | 26 | 33.556 | 50.295 | 65.926 | 114.635 |

Observations poll the last submitted native state every 10 ms; these intervals
include polling delay and do not distinguish every intermediate update coalesced
between polls. They exclude physical serial transport and GPU presentation.
Session process CPU totals were 68.656 s user and 10.922 s system; RSS moved from
252,731,392 to 262,144,000 bytes. These shared-host, single-session observations
are not steady-state CPU, a leak trend or controlled performance evidence.

After public disposal, native elements/subjects were 0, hierarchy entries 1
(the empty root), and scheduler owners 5, matching pre-App baseline. Active
owners/deadlines, queued/active map work and live/retired textures were 0. Four
idle map-worker threads and 13 window callbacks remained owned by the still-live
runtime. JS Fiber IDs, committed descriptions, staged/candidate nodes, pending
events, all forward/reverse/widget registrations, tracked application intervals,
telemetry listeners and both ports' parser/transport listeners were 0; event
subscription and registration service were closed/disposed. All 2,454 Fabric
publications applied, with no failed publication. At ordinary native shutdown,
scheduler owners, platform callbacks and map-worker threads became 0, workers
were stopped, wake delivery detached, and the surface status was `disposed`.

## Reproduction and identity

The production environment was Windows `10.0.26200`, Node `v24.14.0`, React
`19.2.3`, AMD Ryzen 7 5700U / Radeon Graphics, OpenGL `4.6.0 Compatibility Profile
Context 23.19.23.13.250826`. Native targets used VS2022 (developer tools
`17.14.35`) and existing build caches. Wasm reused `xframes-emsdk` with
`XFRAMES_FAST_BUILD:BOOL=OFF`, Emscripten `5.0.2`, and Headless Edge `152.0.0.0`
with SwiftShader; its browser regression is software WebGPU coverage.

| Artifact | SHA-256 |
| --- | --- |
| Retained application patch | `61b1a9749a1ffc522d347a354a60a5c6e370f728c37367b0b7e2d5036236cd38` |
| Release Node binary, also installed in production and reproduction | `d9347b36fa7d95455cc4a18e9cf288b911931108e46d85c5114d13fa11f56380` |
| Production common 0.1.7 tarball | `ce6c9956a5894973ea3fe11f2992b21d905ffd3425ce8590ae01d1b231b7a816` |
| Production Node 0.1.14 tarball | `103d9089affa1817d0de424066ee24b8ae1bfaed14e5f2ae9af888c3aea12d1d` |
| Original sky script | `59759add2610a5ec8b2e9777657631f5ea7ae9d12d8e92f90c87c9e415279df7` |
| Local PNG fixture | `4a93ce814b499287294c61757580c670ca5ee8fc9147beb7d9d9544a827f84f1` |

Reproduction rebuilds and repacks the JavaScript packages, so consult its own
`provenance.json` for tarball integrity and installed lock hash. It records source
revisions, dirty diffs (including new application files), untracked XFrames source
hashes, original AGENTS/manifest/lock/settings preservation, and font/script/native
hashes. `commands.json` retains every exact setup command and exit status. The
final handoff manifest/diffs under `ubx-application/` also capture documentation
updates after the setup snapshot. Required migrations are in the retained patch,
not just the ignored checkout. The first reproduction predates the final isolated
tile-cache path; use `ubx-application-reproduction-final` as the final setup proof.

From `packages/dear-imgui/npm`, with the current Release native target built:

```powershell
node diagnostics/ubx-application-setup.mjs --source=C:/dev/ubx-monitor --output=C:/dev/xframes/packages/dear-imgui/npm/build/diagnostics/ubx-application-reproduction-final
# For another reproduction, select a fresh output directory.
$env:NODE_ENV='production'
$env:TSX_TSCONFIG_PATH='diagnostics/tsconfig.json'
$env:XFRAMES_UBX_APP_DIR='C:/dev/xframes/packages/dear-imgui/npm/build/diagnostics/ubx-application/app'
$env:XFRAMES_DIAGNOSTICS_DIR='C:/dev/xframes/packages/dear-imgui/npm/build/diagnostics/ubx-application/production'
node --import ./common/node_modules/tsx/dist/loader.mjs diagnostics/ubx-application.ts --scenario
```

The production command above identifies the actual measured checkout. To rerun
using the fresh setup, select its `app` directory and a new evidence directory.
For ordinary startup, clear `TSX_TSCONFIG_PATH`, set `NODE_ENV=development`, then
run `npm start` inside that app. `ordinary-startup.ps1`, its stdout/stderr and
`ordinary-startup.json` retain the executed plain-startup check: native window
visible for five seconds, normal `WM_CLOSE`, exit 0, no forced termination.
Announce each desktop command before it takes focus or moves the cursor.

Executed changed-source build/check commands (from npm root except the batch):

```powershell
& 'C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' --build node/build --config Release --target xframes --parallel 4
cmd /c build/diagnostics/ubx-application/build-tests.cmd
& ../cpp/tests/build/Google_Tests_run.exe --gtest_output=xml:build/diagnostics/ubx-application/native-tests-final.xml
docker run --rm --name xframes-ubx-app-wasm-build -v C:/dev/xframes:/src -v xframes-ccache:/ccache xframes-emsdk bash -c 'cd /src/packages/dear-imgui/cpp/wasm && cmake --build build-wasm --target xframes -j4'
$env:XFRAMES_DIAGNOSTICS_DIR='build/diagnostics/ubx-application/lifecycle'
npm run test:lifecycle
npm run diagnostics:typecheck
$env:NODE_ENV='production'
$env:XFRAMES_DIAGNOSTICS_DIR='build/diagnostics/ubx-application/regression-node-final'
npm run diagnostics:node
$env:XFRAMES_DIAGNOSTICS_DIR='build/diagnostics/ubx-application/regression-wasm-final'
npm run diagnostics:wasm
node diagnostics/transaction-parity.mjs build/diagnostics/ubx-application/regression-node-final/result.json build/diagnostics/ubx-application/regression-wasm-final/result.json
```

The setup executes common/package builds, ABI/Fabric tests, package packing and
both application checks. Build/test output is retained in `build-*-final.log`,
`native-tests-final.log/xml`, `lifecycle/`, `diagnostics-typecheck-final.log`,
`regression-*-final/`, and `parity-final.log`. Expected invalid-image fixture
messages occur in the short runtime resource tests; both suites finish passed.

Exploratory failures remain in probe/typecheck/build logs: incompatible original
application APIs/types, the native inactive-tab assertion, stale serial ownership
and missed clicks motivated the focused changes. The first plain-startup wrapper
also failed to retain the Windows process exit code; retaining its process handle
fixed the observer, and the repeated startup returned 0. These failed observations
are not counted as successful acceptance. No final required gate is blocked.

This result qualifies the bounded synthetic Windows application slice. Physical
serial, public tile services, other desktop environments, controlled UI update
rates/performance, hardware WebGPU, mixed DPI, GPU completion/presentation,
equivalent Electron/GPUIX comparisons and consistently green CI over later changes
remain unqualified here. Stages 5–6 and the broader milestone remain open.
