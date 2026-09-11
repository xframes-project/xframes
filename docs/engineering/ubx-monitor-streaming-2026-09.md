# ubx-monitor UI update pacing and streaming measurements

Status: bounded application slice completed, 11 September 2026. Configurable
pacing, source-owned bounded histories, final-source functional/lifetime checks,
fresh patch reproduction and three quiet-window cost pairs passed. The
[paired report](ubx-monitor-pacing-cost-2026-09.md) records lower publication work
and CPU with a measured freshness tradeoff. Broader framework performance and
milestone qualification remain open. The executable task is [goal.txt](../../goal.txt).

## Starting point and purpose

The [ordinary application slice](ubx-monitor-application-2026-09.md) is complete
in XFrames `54bc3827c57c8d8e3a02026ce5998c752e0d1783` and ubx-monitor
`e7de9e25ff3f608e8bb2613e471d0b97f47d58f2`. The latter is available on
`qualification/current-xframes-application` in the original application repository.
Its `main`, local serial settings and dependency graph remain untouched.

That slice established real App integration and lifetime correctness with current
local packages. Its 60-second, 20-Hz-per-type stream submitted 3,498 frames on an
actively used Windows laptop. Those frames include input, status and resource work;
the count does not identify the cause of application cost. The timing samples are
informational and include native observation delay.

The delivered application feature is a UI update rate independent of receiver
arrival, with publication and derived-data work paced at the application owner
without losing retained data. The contract and acceptance criteria below are
preserved; executed outcomes follow them.
This advances [Streaming Architecture](../../ROADMAP.md#streaming-architecture).
It does not close the complete Stage 0 benchmark or Stages 0–4 performance gate.

Inspection of the qualified source explains the ownership risk: `useNavSat` and
`useNavPvt` publish React state for each packet; `useUbxMessages` updates rows/rates
on arrival; `usePositionHistory` derives samples/statistics from an effect over
the latest PVT state; Map appends its trail from another effect. Console appends
formatted bytes through its native handle. A generic debounce around the current
hooks would therefore change history semantics and leave some native work unpaced.

## Application contract

Provide an ordinary persisted selector for 10, 20 and 60 UI updates/second,
defaulting to 20; invalid configuration falls back to the documented default.
Explain it as the display update rate. Serial baud and receiver configuration
remain independent. Reuse existing components and configuration ownership.

Use a shared application cadence for pending telemetry presentation work. The
implementation may use bounded stores/hooks, but a second renderer, a general
state-management framework or a native scheduling policy is unnecessary. Pace
expensive snapshot preparation/publication as well as React notification where
measurement shows those costs. Do not merely delay a render after doing all the
same per-message derivations.

| Data or action | Required semantics |
| --- | --- |
| Raw transport and decoded packets | Every valid packet reaches the real parser/subscribers; sequence and byte accounting precede presentation coalescing. |
| Latest satellite/fix/marker/accuracy/sky display | Newest eligible value at each publication; intermediate snapshots may coalesce and are counted. All views converge to the final packet after input stops. |
| Message and Console history | Preserve ordered bounded retention and truthful arrival-based counters, byte rates and offsets. UI cadence must not become the definition of a received message. |
| Position plots, map trail and statistics | Retain each eligible source sample within the declared history window, independent of React effects. Batch or replay the retained tail through existing widget operations; distinguish deliberate old-history eviction from skipped samples. Preserve fix filtering and reset behavior. |
| Rate change | Takes effect without reconnecting, duplicate timers or catch-up bursts. Pending work remains owned and is flushed under the new cadence; test both faster and slower changes. |
| Connection, errors, reset and user interaction | Remain prompt. Pacing telemetry must not postpone controls, keep stale pre-reset work, limit native interaction or hide errors. |
| Pause and trailing data | Deliver the last pending state without requiring another packet or mouse movement. Cancel pending-only pacing work when drained; real stale-time/status deadlines may continue. |
| Disconnect, reconnect and disposal | Clear or finalize pending work under an explicit policy; old connection samples cannot overwrite a new connection or cleared history. Release subscriptions/timers/buffers and reject late callbacks after public disposal. |

Declare limits for all retained data and pending work. Start from existing caps:
500 message rows, 1,000 trail points, 3,000 position/plot samples and the bounded
Console policy. Avoid an unbounded queue behind a low UI rate. A burst may evict
the oldest data under that policy, but the retained tail must match an independent
sequence-based expectation. Counter and summary data must account for all input.
Do not replace every packet with only the last packet before history ingestion.

## Functional acceptance

Use deterministic clock tests for scheduling boundaries, continuous input,
trailing delivery, no-work idle, rate changes with pending work, reset, old
connection callbacks and disposal. Test a burst larger than retention capacity
and the exact retained sequence tail, including derived position results. These
tests must assert the public behavior rather than mirror the implementation.

For an unchanged rate over elapsed time T in milliseconds, pending-only cadence tests permit at
most `1 + floor(T * rate / 1000)` telemetry flushes, including a leading flush.
Status/control publications are recorded separately. A delayed timer publishes
current work once; it must not replay a backlog of missed timer ticks. Under
continuous eligible input, the deterministic test must also prove scheduled
publication and trailing delivery, so passing only an upper bound is insufficient.

Retain the production full-App 20-Hz-per-type, 60-second correctness scenario.
Add a 60-second 120-Hz-per-type session (7,200 packets of each type), switching UI
rates 20 → 10 → 60 → 20 through ordinary native input while delivery continues.
Confirm persisted configuration in the isolated app. Inspect populated Signals,
Messages, Map, Sky, Position and Console state/captures, with a sort or filter,
map navigation and position reset. Reuse the actual Canvas script and local tiles.

Assert all packet sequences, snapshot eligibility/coalescing, exact retained
histories and final native state. At each rate transition and at stream end, use
bounded waits for the relevant sample in a submitted frame, not just a newer
unrelated frame. Include connected pause/stale-time advance, resumed input,
disconnect/reconnect, ten settled disconnected seconds without constructed or
submitted frames, awaited public disposal, late packets and pending resources.
Both application pacing owners and existing JS/native lifetime counts must reach
their documented baselines before ordinary native shutdown.

## Bounded before/after measurement

Compare application `e7de9e2` with the candidate on the same XFrames/native package
build whenever possible. If a necessary runtime fix changes that build, identify
the comparison as combined application/runtime work or add the minimal control
needed to attribute the difference. Preserve the original successful evidence.

First characterize the baseline's retained histories against the same source
oracle. If its existing React coalescing already skips history samples, preserve
that finding; the candidate must satisfy the source-based retention contract.
Do not call those variants equivalent. For a pacing-only cost claim, use a
minimal unpaced control with the candidate's corrected ingestion/history logic
for the paired comparison, and label `e7de9e2` as the historical characterization.
This control may be diagnostic-only; an unpaced product setting is not required.

Run three paired repetitions of the following one comparison cell:

| Parameter | Baseline and candidate |
| --- | --- |
| Receiver source | Checksummed synthetic NAV-SAT and NAV-PVT, 120 Hz each; identical packet contents, splits/coalesced callbacks and sequence ranges. |
| Measured duration | 60 seconds; 7,200 packets of each type, plus separately counted warm-up/lifecycle packets. |
| Presentation policy | Unpaced qualified baseline (or the retention-correct control described above) versus candidate UI rate 20 Hz. |
| Visible state | Signals stays selected during measurement; all ordinary application subscriptions remain mounted. No navigation, screenshots or desktop input inside the measured interval. |
| Preparation | Identical dimensions, fonts, theme, history initialization, local resource/cache policy and declared warm-up. Wait for outstanding resource work to settle before starting. |
| Execution | Optimized Node/OpenGL, production React, same host/adapter and observation settings; sequential runs with no task-owned builds or other tests running concurrently. |

Alternate baseline/candidate ordering across pairs and retain every run, including
interrupted or failed ones. A clean measurement should deliver at least 95% of
requested source rate while accounting for all packets; otherwise identify source
timer lag or application saturation and classify that cell as unmet. Delivery
uses monotonic scheduling independent of frame waits. Do not turn a late source
into apparent success by reporting only the requested rate.

Report source delivery, decoding, logical presentation flushes, actual React
publications/structural calls, native snapshot replacements and history appends
separately. At high input, the candidate must demonstrate lower eligible telemetry
snapshot publication work and the declared cadence with equivalent retained data
in the comparison control. Total native frame count is not the pacing contract.
Do not claim improvement
from dropping a panel, suppressing status or ceasing history ingestion. CPU and
RSS are measured outcomes, not assumed consequences of fewer React publications.

Record p50/p95/p99/maximum for receipt-to-publication and
receipt-to-observed-submission where supported, with sample coverage and separate
coalescing/eviction counts. Record intentional waiting time versus compute/boundary
time where instrumentation can distinguish them. Use sequence-to-publication-to-
submitted-state correlation; do not assign the latest sample's timestamp to older
samples merely because they share a frame. Report observation interval, observer
cost/limits, process CPU per wall-clock interval and comparable RSS checkpoints.
GPU completion and physical presentation remain unavailable unless directly measured.

The [historical framework targets](fabric-baseline-2026-09.md#proposed-targets-separate-from-observations)
remain unchanged: at least 95% of requested 20/60/120-Hz input, observed-frame p95
below 50 ms and p99 below 100 ms at 60/120 Hz, and the stated idle target under
its declared workload. This app comparison uses different histories and deliberate
UI waiting, so it must not close those framework targets. Explain the measured
freshness/cost tradeoff for 20-Hz UI pacing without relabeling batching delay as
renderer latency or claiming a universal rendered frame rate.

The user uses the laptop during work. Announce each desktop run and its approximate
duration; existing authorization permits it without another permission question.
Verify click targets and retry misses. Arrange a quiet comparison window if the
host is in use, while proceeding with independent work. If quiet conditions cannot
be obtained, retain shared-host results as informational and explicitly leave
controlled comparison open. Never infer a quiet window from silence or elapsed time.

## Reproduction, validation and stopping rule

Continue in an isolated checkout from the qualified application branch, preserving
the original repository's checked-out files. `ubx-application-setup.mjs` now
supports `--base=qualified` at `e7de9e2`, an incremental `--patch` and reusable
`--packages`. Its default migration mode remains available for historical
reproduction. Do not apply the old full migration twice. Record both repo
revisions/diffs, command parameters, tarball/native identities and original-file
hashes; use new evidence directories rather than replacing completed reports.

Run whole-app typecheck, ordinary development startup, existing serial lifecycle
and new pacing/history tests, and required production scenarios. Changes to XFrames
source require affected package/Fabric/diagnostic/native checks; a shared runtime
change requires relevant Node and Wasm coverage. Reuse native caches and earlier
valid results for unchanged code. Preserve the required 1,000-cycle gates and
optional extended mode. A changed workflow needs its affected hosted checks;
neither the old `fa12b22` hosted pass nor local checks prove new hosted coverage.

Stop after the rate control, independent bounded history ownership, reproduction,
functional/lifetime checks and bounded measurement report are complete. Fix proven
correctness blockers; do not widen into a generic performance rewrite. Report
unmet rates, unavailable quiet-host measurements or regressions explicitly and
leave their acceptance open. A repeated cell needs a recorded interruption, code
change or specific diagnostic reason; do not repeat until a favorable result occurs.

Stage 5 replay, Stage 6 general automation, a native frame cap, a renderer or serial
stack replacement, new widgets, registry releases, original-checkout migration,
physical hardware validation, public tile-service qualification, a full Node/Wasm
benchmark matrix, hardware WebGPU and Electron/GPUIX comparisons are outside this
slice. The broader milestone and subsequent consistently green CI remain open.

## Delivered implementation and identities

Connection now includes **Display updates/sec: 10 / 20 / 60**, default 20. It
stays usable while connected. Invalid or malformed persisted values fall back
to 20; the existing exit-save configuration policy persists valid choices while
preserving receiver settings. No baud or receiver command changes occur when
selecting a display rate.

`PublicationCadence` owns one pending bit per live presentation owner and at most
one timer. Late timers publish current work once, anchored to actual execution;
an empty queue has no timer. The shared connection-status subscription clears
histories and pending publication on transitions away from connected. Controls,
reset and stale-status deadlines remain independent. The ordinary App registers
13 owners, ten eligible for the SAT/PVT workload. The diagnostic unpaced switch
must be chosen before mount and is absent from persisted product settings.

| Owner | Source semantics and publication |
| --- | --- |
| PVT/SAT/DOP/NAV-STATUS | Bounded latest raw value; selectors and React enqueue run on publication. |
| Position | Every eligible fix enters a 3,000-sample ring. CEP/scatter and mean retained horizontal accuracy derive from that ring; the previously mislabeled latest accuracy is now an actual retained mean. Each line handle replays unseen retained sequence values, including skipped React snapshots. |
| Map | Independent 1,000-point source trail; full retained snapshot plus latest marker/accuracy overlays. No-fix clears live marker/accuracy; reconnect preserves current zoom when recentering. |
| Messages/status bar | Each existing owner retains 500 ordered messages, arrival ordinals and lazily cached immutable rows. Fixed 1,001-ms slots preserve arrival-based byte/message rates; one-second stale/rate-decay status stays separate. |
| Console | Existing formatter and offsets ingest all wire data. 65,536 UTF-16-unit cap rotates to the final 32,768 units; incomplete input is copied into bounded pending storage. Snapshot generations batch native appends and preserve exact text across skipped React updates. |
| Hardware | Existing MON-HW/RF/HW3 filtering, fields and a 3,000-sample jamming ring; source ingestion precedes one cadence owner. |

Position Reset clears only position history. Reconnect starts fresh histories;
late old-port/publication callbacks cannot repopulate them. Rings and pending
ownership are bounded; older source data is evicted explicitly, never replaced
by the latest packet before ingestion. Public render/disposal, native scheduling,
atomic Fabric publication, map resources and the actual non-continuous Sky script
remain in use. No shared runtime/native/workflow source was changed.

The reviewable artifact is
[`ubx-pacing.patch`](../../packages/dear-imgui/npm/diagnostics/ubx-pacing.patch),
SHA-256 `2a2dc7b044427497e4063d2a2339e373450332445d3bf46e901d70018453e8b0`,
based directly on app `e7de9e25ff3f608e8bb2613e471d0b97f47d58f2`.
XFrames base is `54bc3827c57c8d8e3a02026ce5998c752e0d1783` plus the reviewable
diagnostic/documentation changes. The working app is on isolated branch
`qualification/ui-pacing` under `packages/dear-imgui/npm/build/diagnostics/ubx-pacing/app`.
The final fresh reproduction is `build/diagnostics/ubx-pacing-reproduction-final/app`.
The app changes were subsequently committed as
`817f9316e0c14d9bdf8a17c4d8d962d31de4851f` on that isolated
`qualification/ui-pacing` branch. The original application checkout remains
unchanged; the incremental patch is the portable reproduction artifact.
The completion audit verified source equivalence after CRLF normalization:
fresh Git patch application changed line endings in 12 source files and
package.json, while package lock, fonts, native build and diagnostic source
matched byte-for-byte. All six quiet runs have identical raw source identities.

| Reused artifact | SHA-256 |
| --- | --- |
| common 0.1.7 tarball | `ce6c9956a5894973ea3fe11f2992b21d905ffd3425ce8590ae01d1b231b7a816` |
| Node 0.1.14 tarball | `103d9089affa1817d0de424066ee24b8ae1bfaed14e5f2ae9af888c3aea12d1d` |
| Installed/current Release native | `d9347b36fa7d95455cc4a18e9cf288b911931108e46d85c5114d13fa11f56380` |
| Unchanged Sky script | `59759add2610a5ec8b2e9777657631f5ea7ae9d12d8e92f90c87c9e415279df7` |

Node v24.14.0 / React 19.2.3 were used throughout. Setup provenance records font,
package-lock, source/diff, tarball and native identities and verifies the installed
native matches current Release. The original `C:/dev/ubx-monitor` remains on main
`571f5569bb923c3d4a8f37db8f8ada555323667a`, with only its pre-existing untracked
AGENTS.md. Its AGENTS, package.json, package-lock.json and config hashes match
the preserved original record. No original settings or dependencies were migrated.

## Executed correctness and lifetime gates

Fresh final reproduction applied the incremental patch to `e7de9e2` and passed
whole-App `typecheck`, `test:serial` and `test:pacing`. The latter includes
deterministic clocks at all rates, continuous/trailing delivery, live faster/slower
changes, delayed callbacks/no catch-up, owner cancellation/disposal, exact rings,
Console text, position filtering/CEP/mean, hardware jamming and rate-window decay.
The real-parser regression decodes 7,200 pairs plus reconnect, checks exact
tails/bytes/ordinals, lazy-row cache reuse and old-port/late-work rejection.
Config regressions use child process restart/exit to prove defaults, malformed
input and persisted 10 → 60 → 20 while retaining receiver fields.

`ubx-source.test.ts` proves monotonic 20/120-Hz source delivery with simulated
16-ms timers, bounded debt batches (maximum 32 pairs/turn), no early delivery and
exact sequences. The source driver is independent of frame observations. The
diagnostic typecheck passed; unchanged framework/native gates from the prior
application qualification remain valid without another native rebuild.

| Production gate | Input | Result |
| --- | --- | --- |
| `production-20-final` from fresh reproduction | 1,200/type in 60,010.030 ms, 19.996657 Hz/type; 1,203/type with lifecycle, 2,406 messages, 197,292 bytes | Passed all final-state, interaction, history and lifetime assertions; no input retries. |
| `production-120-transitions-03` | 7,200/type in 60,029.341 ms, 119.941347 Hz/type; 7,203/type with lifecycle, 14,406 messages, 1,181,292 bytes | Passed native 20 → 10 → 60 → 20 changes, all populated panels, reset/reconnect and lifetime checks; no input retries. |
| Six `quiet-pair-*` runs | 7,200/type each, 119.948–119.997 Hz/type | Passed equivalent exact histories, trailing delivery and public/native cleanup; [full costs and latency](ubx-monitor-pacing-cost-2026-09.md). |

The high-input native selections occurred at 4.425 seconds (sequence 517, 10 Hz),
8.853 seconds (1,048, 60 Hz) and 14.622 seconds (1,729, 20 Hz), all while packets
continued. Config and cadence agreed, serial status/baud/write counts did not
change, and submitted state reached samples at or after each selection. Final
panel navigation finished after source delivery; its 66.595-second stream-await
wall time is not the source duration.
Its saved application patch differs from the final patch only by the subsequently
added configuration regression and its test script entry; runtime source is the
same. No performance run was repeated for these test-only additions.

The full gates exercised Signals CNO sort, Messages, map wheel/overlays, actual
Sky script/resize, Position and Console, with populated captures inspected.
The final 20-Hz gate retained positions 0–1,200 (1,201), trail 201–1,200 (1,000),
message ordinals 1,903–2,402 (500), and exact Console text (43,119 UTF-16 units,
44,575 bytes, wire offset 196,964). Its evictions were 0/201/1,902 positions,
trail points and messages, plus 1,087,022 Console units. The high gate and paired
runs retained exact 3,000/1,000/500 source tails and Console as detailed in the
cost report. Native API-call history is checked in full against independent
oracles, while submitted summaries corroborate bounds/final values; full native
buffers are not read back.

Position Reset followed by one source sample leaves one position/line value but
preserves the trail/message/Console histories. Reconnect leaves one position,
one trail point, two messages and exactly 941 Console units / 973 bytes at fresh
wire offset 164. Both full gates pass pause/stale-time advance, resumed final
sample, and ten settled disconnected seconds with zero constructed/submitted
frames. In the final 20-Hz run the connected pause submitted three frames and
the sustained scenario submitted 3,098; these functional shared-host counts are
not the paired measurement.

Four local HTTP Map requests were held at unmount and released after awaited
public disposal. Ports/parser/telemetry listeners, application intervals,
presentation owners/pending/timers, JS registrations/Fabric ownership and native
resources returned to asserted baselines. Native terminal scheduler owners,
active owners and deadlines reached zero and wake delivery detached. Ordinary
development `npm start` from the final reproduction opened the native window,
remained visible for five seconds and exited 0 after normal WM_CLOSE, without
diagnostic TSX overrides or forced termination (`ubx-pacing/ordinary-startup.json`).

## Retained attempts and evidence limits

Evidence directories below are under the npm workspace's
`build/diagnostics/ubx-pacing`; none was overwritten or selected away:

- `development-probe`: passed startup/populated views; a listener warning led
  to the shared connection-status multiplexer, subsequently covered by final gates.
- `production-20-01`: failed the held-resource fixture after reconnect reset zoom
  to 15 and reused cached tiles. Map now preserves user zoom on recenter.
- `production-20-02`: incomplete native-shutdown observation despite exit 0.
  The harness had exited before the terminal callback; it now waits for that
  callback. `production-20-03` passed the intermediate source. Later lazy-row
  caching is covered by the final-source runs, not retroactively by this one.
- `production-120-transitions-01`: functional assertions passed but input rate
  was unmet (62.366 Hz/type over 115.448 seconds). A one-pair timer met Windows
  timer granularity limits; the bounded monotonic source driver fixed delivery.
- `production-120-transitions-02`: failed because interspersed panel navigation
  placed the last rate selection after source completion. Consecutive selections
  on Connection fixed the fixture; `-03` passed without weakening assertions.
- `qualified-characterization-01`: complete legacy lifecycle passed but the exact
  altitude source tail lost 1,986 retained samples. This required the equivalent
  retention-correct unpaced control, not a favorable baseline choice.
- `comparison-pair-1-unpaced`: passed shared-host run before the user confirmed
  a quiet window. Retained separately from the six fresh quiet-window runs.

The initial Console test literal had one padding space wrong; its oracle was
corrected without changing the formatter. An ad hoc Console-oracle import probe
failed on CJS/ESM syntax; the corrected probe passed exact 7,201-pair text equality.
Final whole-App evidence carries per-run source identities or saved patch/harness
copies. `ubx-pacing-reproduction-final/provenance.json` and `commands.json`
identify the final reproduction, preserved originals and headless checks.

The bounded goal is complete. The observed-frame framework p95/p99 targets remain
open and are not met by these deliberately batched application observations.
Quiet laptop comparisons do not establish uninstrumented performance, GPU
presentation, physical serial, public tiles, hardware WebGPU, mixed DPI, other
platform backends or future CI stability. Stages 0–4 milestone review and Stage
5/6 deferral remain unchanged. See the [diagnostics reproduction guide](../../packages/dear-imgui/npm/diagnostics/README.md).
