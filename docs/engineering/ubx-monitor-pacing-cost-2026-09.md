# ubx-monitor pacing: three paired cost measurements

Executed 11 September 2026 on a user-confirmed quiet laptop window. All six runs
passed input-rate, exact retention, trailing sample and public/native teardown
checks. This is one Windows Node/OpenGL application comparison, with instrumented
costs and incomplete submission observation; it does not qualify the broader
framework performance targets.

## Workload and equivalent control

The qualified `e7de9e2` application was characterized first. Although all 7,200
scheduled packets of each type arrived at 119.997 Hz/type, its 3,000-point altitude
history omitted **1,986** samples from the expected final tail (4,201–7,200).
The retained sequence started at 156. Evidence is
`packages/dear-imgui/npm/build/diagnostics/ubx-pacing/qualified-characterization-01`.
That baseline is not an equivalent retained-data workload.

The paired control therefore uses the candidate's corrected ingestion and bounded
histories with diagnostic-only immediate publication. Both modes run the same
source, including cached message-row formatting; the only selected policy is
unpaced versus the ordinary 20-Hz UI cadence. This control is selected before App
mount and is neither a persisted setting nor a product option.

Each run sends checksummed NAV-SAT and NAV-PVT at 120 Hz/type for 60 seconds:
7,200 scheduled pairs, plus one separately counted warm-up pair. Identical packets
alternate split and coalesced transport callbacks. Every run decoded 14,402
messages from 10,801 callbacks and 1,180,964 bytes, with no sequence gaps. Actual
rates were 119.948–119.997 Hz/type, exceeding the 95% acceptance threshold.

Signals remains visible at 1280 × 900; all normal App subscriptions stay mounted.
Each process uses a fresh local tile cache, one warm-up pair, 4.5 seconds of
settling and verified resource/native inactivity before measurement. No desktop
helpers, captures, builds or tests execute in the measured interval. Order was
U1, P1, P2, U2, U3, P3. The earlier shared-host
`comparison-pair-1-unpaced` run remains retained and is excluded from these pairs
because the user subsequently provided a quiet window, not because of its result.

Host: AMD Ryzen 7 5700U / AMD Radeon Graphics, Windows 10.0.26200, OpenGL 4.6
driver 23.19.23.13.250826, Node v24.14.0, React 19.2.3, production mode. Quiet
conditions were user-confirmed; OS scheduling, GC and thermal variation were not
eliminated. Every `source-identity.json` is identical across the six runs, including
application source, package lock, fonts, native/common binaries and diagnostic
hashes. The [streaming record](ubx-monitor-streaming-2026-09.md) lists reproduction
and package identities.
The measured diagnostics and app changes were subsequently committed in XFrames
`880506e` and isolated ubx-monitor `817f931`. Historical run identities retain
the pre-commit base revisions and diffs; these are the same tested sources.

## Counts and process costs

U = retention-correct unpaced; P = 20-Hz pacing. These counters end at **source
completion**, before trailing publication, screenshot and teardown. CPU is total
process user + system time, so it can exceed 60 seconds on this multicore host.
RSS is a pair of process endpoints, including diagnostic storage, not a peak or
application-only memory measurement.

| Run | Hz/type | CPU seconds | RSS MiB before → after | Applied Fabric publications | Each snapshot operation | Submitted frames |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| U1 | 119.948 | 83.048 | 198.09 → 556.80 | 2,104 | 2,104 | 3,601 |
| P1 | 119.997 | 58.406 | 198.52 → 451.21 | 1,031 | 988 | 3,531 |
| U2 | 119.984 | 81.532 | 197.30 → 535.07 | 2,155 | 2,154 | 3,600 |
| P2 | 119.986 | 59.578 | 197.55 → 463.71 | 1,046 | 984 | 3,544 |
| U3 | 119.968 | 82.407 | 199.00 → 549.63 | 2,135 | 2,135 | 3,602 |
| P3 | 119.997 | 61.734 | 198.68 → 445.69 | 1,004 | 951 | 3,555 |

“Each snapshot operation” means each of Table rows, Map trail, Map markers, Map
overlays, Sky data, position scatter and Signals bar-series replacement; they
have equal counts within a run. Console append calls also equal this column.
Applied Fabric publications are acknowledged atomic React/Fabric publications,
not a count of React component renders. Status and Console rotation can create
additional structural work independently of telemetry snapshots.

Three line plots append retained source samples rather than decimating them.
Their combined source-boundary append counts were U1/P1 21,585/21,582,
U2/P2 21,582/21,591, U3/P3 21,591/21,585. The few final appends after this boundary
are included in the exact-tail check. No line reset occurred during measurement;
the Map render and Sky script/continuous-mode setup counts were zero during it.

| Paired reduction | Pair 1 | Pair 2 | Pair 3 | Median |
| --- | ---: | ---: | ---: | ---: |
| Process CPU | 29.67% | 26.93% | 25.09% | 26.93% |
| Snapshot operations | 53.04% | 54.32% | 55.46% | 54.32% |
| Applied Fabric publications | 51.00% | 51.46% | 52.97% | 51.46% |
| Submitted frames | 1.94% | 1.56% | 1.30% | 1.56% |

Pacing reduces snapshot work and measured CPU in every pair, with higher observed
latency and fewer observed intermediate states. It does not produce a 20-FPS
renderer. Frame count remains near 60/sec under this continuously active workload.

## Eligibility, batching and retained data

Counts here extend **through the final sample** and subtract settled warm-up.
Each run has 111,600 owner requests: Console 10,800; each of two message owners
25,200; each of three PVT owners 7,200; each of two SAT owners 7,200; position and
trail owners 7,200 each. Hardware, DOP and NAV-STATUS owners remain mounted but
receive no corresponding packets in this cell. Requests include both raw and
decoded notifications, so they are not packet counts.

| Mode/run | Logical flushes | Owner publications | Coalesced owner requests | Cancelled |
| --- | ---: | ---: | ---: | ---: |
| Each U | 111,600 immediate owner flushes | 111,600 | 0 | 0 |
| P1 | 989 shared flushes | 9,890 | 101,710 | 0 |
| P2 | 985 shared flushes | 9,850 | 101,750 | 0 |
| P3 | 952 shared flushes | 9,520 | 102,080 | 0 |

Unpaced immediate owner flushes are not shared timer ticks. The paced mode has
ten eligible owners per flush, one shared pending timer, and zero pending work
after final delivery. Observed paced median flush intervals were
61.992/61.992/62.107 ms: actual publication is below the configured maximum because
timers and synchronous work can delay callbacks. No missed-tick replay occurs.
First-owner instrumentation minima were 50.005/49.987/49.996 ms (small clock-read
offsets relative to the cadence anchor); deterministic clock tests prove the
strict 50-ms scheduling bound and continuous/trailing delivery.

All six runs retain exactly positions 4,201–7,200 (3,000 samples), trail
6,201–7,200 (1,000 points), message arrival ordinals 13,903–14,402 (500 rows), and
57,234 Console UTF-16 code units / 59,170 UTF-8 bytes at wire offset 1,180,964.
Declared evictions are 4,201 positions, 6,201 trail points, 13,902 messages and
6,718,907 Console code units. The position/scatter/three-line values, ordered trail,
message ordinals and complete retained Console text match independent oracles.
This proves ordered data sent through successful native widget API calls;
submitted native summaries corroborate bounds and final values. It does not
claim a full native-buffer readback. Source timestamps remain arrival times.

## Freshness and observation coverage

Receipt-to-observed-submission includes application batching, React/native work
and polling delay. SAT is correlated from submitted bar values and PVT from the
submitted Map marker. Each type has 7,200 eligible source sequences. Missing
observations combine intentionally coalesced snapshots and states skipped between
polls; they are not decoded-packet loss. The final sequence is observed in all runs.
Quantiles use nearest rank without interpolation; units are milliseconds.

| Run | SAT observed / 7,200 | SAT p50 / p95 / p99 / max | PVT observed / 7,200 | PVT p50 / p95 / p99 / max |
| --- | ---: | --- | ---: | --- |
| U1 | 1,924 | 49.582 / 76.010 / 90.430 / 105.842 | 1,796 | 34.175 / 51.843 / 65.762 / 104.206 |
| P1 | 989 | 63.683 / 89.611 / 110.206 / 168.569 | 986 | 49.421 / 68.023 / 79.282 / 91.020 |
| U2 | 1,962 | 50.237 / 71.571 / 84.981 / 117.818 | 1,794 | 32.655 / 50.153 / 62.868 / 87.286 |
| P2 | 985 | 63.825 / 89.941 / 118.568 / 144.966 | 983 | 50.438 / 73.543 / 88.028 / 105.665 |
| U3 | 1,971 | 48.990 / 73.961 / 85.839 / 109.064 | 1,846 | 33.952 / 51.158 / 64.342 / 73.022 |
| P3 | 952 | 63.774 / 110.005 / 121.976 / 171.784 | 952 | 50.220 / 90.220 / 98.843 / 105.931 |

The separate publication observer records preparation start/end and React enqueue,
not React commit. SAT publication covers 7,200 unique samples per U run versus
989/985/952 per P run, with two owners per sample. PVT has three owners per
published sample; position and trail each have one. Receipt-to-publication for
the selected latest sample differs from the time the oldest pending work waits:

| Run | SAT receipt → enqueue p50 / p95 / p99 / max | PVT receipt → enqueue p50 / p95 / p99 / max |
| --- | --- | --- |
| U1 | 0.010 / 0.023 / 0.036 / 0.363 | 0.007 / 1.470 / 2.428 / 18.652 |
| P1 | 13.971 / 16.920 / 18.639 / 21.458 | 14.217 / 17.781 / 19.695 / 23.666 |
| U2 | 0.010 / 0.022 / 0.035 / 0.240 | 0.007 / 1.409 / 2.235 / 7.177 |
| P2 | 13.985 / 16.928 / 19.131 / 34.305 | 14.269 / 17.714 / 20.059 / 36.017 |
| U3 | 0.009 / 0.022 / 0.036 / 0.434 | 0.007 / 1.444 / 2.465 / 12.748 |
| P3 | 13.538 / 16.368 / 18.336 / 23.428 | 13.728 / 17.306 / 19.225 / 24.858 |

Paced position first-pending-to-start p50/p95/p99/max were
58.989/62.453/64.639/69.479, 58.928/62.571/67.531/93.538 and
59.860/75.006/83.018/89.374 ms. This includes deliberate cadence waiting and timer
lateness; it is not renderer latency. Position preparation plus React enqueue
totals fell from 7.068/6.753/7.008 seconds to 1.204/1.222/1.371 seconds. Per-call
p95 rose from 1.975/1.858/1.983 to 2.622/2.580/3.889 ms. Message-owner totals fell
from 2.373/2.408/2.301 seconds to 0.675/0.694/0.683 seconds, despite larger batches.
Full per-label p50/p95/p99/max, eligibility, latest-request wait and preparation
distributions are retained in `quiet-comparison-summary.json`; raw/message owners
have no invented single-packet receipt correlation.

## Instrumentation cost and limits

All intervals include instrumentation. Explicit observer bookkeeping, fixture
preparation and polling totals are reported separately from measured native-call
boundary time; they are not subtracted from CPU. Observer record retention is
bounded at 250,000 and dropped zero records. U runs retain roughly 111,614 stage
events versus about 9,530–9,900 for P, contributing to unequal RSS and observation
cost even though instrumentation policy is identical.

| Run | Stage bookkeeping ms | Widget bookkeeping ms | Fixture preparation ms | Polling ms / count | Native commit / internal / Console boundary ms |
| --- | ---: | ---: | ---: | --- | --- |
| U1 | 23.346 | 3,342.148 | 1,036.037 | 2,261.527 / 2,307 | 1,986.160 / 28,484.156 / 107.060 |
| P1 | 4.427 | 2,128.594 | 1,266.233 | 3,227.315 / 2,621 | 1,160.498 / 19,574.493 / 126.129 |
| U2 | 29.964 | 3,407.511 | 1,032.346 | 2,183.787 / 2,322 | 1,986.304 / 28,894.813 / 107.265 |
| P2 | 5.915 | 2,177.777 | 1,332.266 | 3,283.001 / 2,561 | 1,216.422 / 19,783.931 / 119.974 |
| U3 | 20.658 | 3,371.908 | 1,041.026 | 2,239.117 / 2,330 | 2,029.707 / 28,686.032 / 110.871 |
| P3 | 4.732 | 2,220.807 | 1,350.075 | 3,125.031 / 2,490 | 1,163.251 / 20,717.919 / 117.603 |

Polling is nominally 10 ms but only executes about 38–44 times/sec under this
workload; actual per-poll intervals were not recorded. Its query/parse/bounds
checks are included above. Native boundary durations include blocking and
scheduling, not just compute. Uninstrumented production CPU, peak memory, GPU
completion and physical presentation latency are unavailable. No subtraction of
quantiles or assignment of a latest timestamp to older retained samples is used.

These results do not meet or close the separate framework observed-frame
p95 < 50 ms / p99 < 100 ms targets. The application deliberately batches, the
observed population differs by mode, and paced SAT p99 exceeds 100 ms in every
pair. The bounded application publication/retention comparison is complete;
broader performance qualification remains open.

## Recompute and inspect

All run directories are under
`packages/dear-imgui/npm/build/diagnostics/ubx-pacing/quiet-pair-{1,2,3}-{unpaced,paced}`.
They contain source identities/diffs, independent source delivery, raw receipts
and observations, publication stages, source-boundary measurement, exact retained
data/oracle reports, final capture and terminal result. These generated directories
are retained locally and ignored by Git. This report and the analysis script are
committed; a fresh clone must obtain the retained dataset or execute fresh runs
before it can recompute a summary. With the dataset present, from the npm workspace:

```powershell
node diagnostics/ubx-comparison-report.mjs
```

This offline script verifies identical source/build/observer identities, rates,
counts, exact retention reports, zero lost stage records and trailing samples;
it writes `build/diagnostics/ubx-pacing/quiet-comparison-summary.json`. It never
launches an app or repeats a measurement. Reproduction commands for the App and
all harness modes are in the [diagnostics guide](../../packages/dear-imgui/npm/diagnostics/README.md).
