// Offline analysis only: run after the six desktop measurements have stopped.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';

const root = resolve(process.argv[2] ?? 'build/diagnostics/ubx-pacing');
const read = (dir, file) => JSON.parse(readFileSync(resolve(root, dir, `${file}.json`), 'utf8'));
function distribution(values) {
  const sorted = [...values].sort((a, b) => a - b);
  const q = p => sorted[Math.max(0, Math.ceil(p * sorted.length) - 1)] ?? null;
  return { count: sorted.length, p50: q(.5), p95: q(.95), p99: q(.99), max: q(1) };
}
const runs = [];
let identity, retention;
for (let pair = 1; pair <= 3; pair++) for (const mode of ['unpaced', 'paced']) {
  const dir = `quiet-pair-${pair}-${mode}`;
  const r = read(dir, 'result'), stream = read(dir, 'stream'), stages = read(dir, 'publication-stages');
  const sourceIdentity = read(dir, 'source-identity');
  identity ??= sourceIdentity;
  assert.deepEqual(sourceIdentity, identity, `${dir}: source/build/observer identity changed`);
  assert.equal(r.status, 'passed');
  assert.equal(r.input.requestedRateMet, true);
  assert.equal(r.input.messageCount, 14402);
  assert.equal(r.input.rawBytes, 1180964);
  assert.equal(stages.droppedStageObservations, 0);
  assert.equal(stages.after.pending, 0);
  const retained = read(dir, 'retention-comparison-final').report;
  retention ??= retained;
  assert.deepEqual(retained, retention, `${dir}: retained/evicted accounting changed`);
  assert.equal(read(dir, 'history-characterization').exactPositionTail, true);
  const delta = key => stages.after[key] - stages.before[key];
  const cadence = Object.fromEntries(['requests', 'coalesced', 'flushes', 'publications', 'cancelled'].map(k => [k, delta(k)]));
  assert.equal(cadence.requests, cadence.coalesced + cadence.publications);
  const events = stages.events.filter(e => e.flush > stages.before.flushes);
  assert.equal(events.length, cadence.publications);
  const firstByFlush = new Map();
  for (const e of events) if (!firstByFlush.has(e.flush)) firstByFlush.set(e.flush, e.startedAt);
  const starts = [...firstByFlush.values()];
  const intervals = starts.slice(1).map((t, i) => t - starts[i]);
  // Instrumented first-owner start follows the cadence clock by a few microseconds.
  if (mode === 'paced') assert.ok(Math.min(...intervals) >= 49.9, `${dir}: cadence burst`);
  const owners = stages.after.ownerCounts.map((owner, i) => Object.fromEntries(
    Object.entries(owner).map(([key, value]) => [key, key === 'label' ? value : value - stages.before.ownerCounts[i][key]])));
  const labels = {};
  for (const label of new Set(events.map(e => e.label))) {
    const es = events.filter(e => e.label === label);
    const kind = label === 'NAV-SAT' ? 'sat' : ['NAV-PVT', 'position', 'map-trail'].includes(label) ? 'pvt' : null;
    const receipts = kind ? new Map(stream.receipts[kind]) : null;
    const correlated = receipts ? es.filter(e => e.sourceSample / 50 >= 1 && e.sourceSample / 50 <= 7200) : [];
    labels[label] = {
      publications: es.length, coalescedRequests: es.reduce((n, e) => n + e.coalesced, 0),
      firstPendingToStartMs: distribution(es.map(e => e.startedAt - e.firstPendingAt)),
      latestRequestToStartMs: distribution(es.map(e => e.startedAt - e.requestedAt)),
      preparationAndReactEnqueueMs: distribution(es.map(e => e.finishedAt - e.startedAt)),
      preparationAndReactEnqueueTotalMs: es.reduce((n, e) => n + e.finishedAt - e.startedAt, 0),
      receiptToPublicationMs: distribution(correlated.map(e => e.finishedAt - receipts.get(e.sourceSample / 50))),
      uniqueSourceSamplesPublished: new Set(correlated.map(e => e.sourceSample)).size,
      correlation: kind ?? 'unavailable: raw/message owner has no single source sample',
    };
  }
  const observed = {};
  for (const kind of ['sat', 'pvt']) {
    const samples = stream.observations[kind].filter(s => s.sequence >= 1 && s.sequence <= 7200);
    assert.ok(samples.some(s => s.sequence === 7200), `${dir}: missing final ${kind}`);
    observed[kind] = { ...distribution(samples.map(s => s.latencyMs)), eligible: 7200,
      coalescedOrUnobserved: 7200 - samples.length, coveragePercent: 100 * samples.length / 7200 };
  }
  const m = r.measurement;
  runs.push({ dir, pair, mode, metadata: r.metadata, hostConditions: r.hostConditions,
    input: r.input, cadenceThroughFinal: cadence, ownerDeltas: owners,
    flushIntervalMs: { ...distribution(intervals), min: Math.min(...intervals) },
    actualFabricPublicationsThroughFinal: stages.appliedFabricPublications,
    measurement: m, cpuSeconds: (m.cpuMicroseconds.user + m.cpuMicroseconds.system) / 1e6,
    rssMiB: { before: m.rssBefore / 2 ** 20, after: m.rssAfter / 2 ** 20 },
    stageDistributions: labels, observedSubmissionMs: observed,
    sourceLatenessMs: distribution(stream.deadlineLatenessMs), inputRetries: r.inputRetries });
}
const pairs = [1, 2, 3].map(pair => {
  const u = runs.find(r => r.pair === pair && r.mode === 'unpaced');
  const p = runs.find(r => r.pair === pair && r.mode === 'paced');
  const reduction = (a, b) => 100 * (1 - b / a);
  return { pair, cpuReductionPercent: reduction(u.cpuSeconds, p.cpuSeconds),
    snapshotReductionPercent: reduction(u.measurement.operations['di-table:setData'], p.measurement.operations['di-table:setData']),
    fabricReductionPercent: reduction(u.measurement.appliedFabricPublications, p.measurement.appliedFabricPublications),
    frameReductionPercent: reduction(u.measurement.submittedFrames, p.measurement.submittedFrames),
    ownerPublicationReductionPercent: reduction(u.cadenceThroughFinal.publications, p.cadenceThroughFinal.publications) };
});
const summaries = Object.fromEntries(Object.keys(pairs[0]).filter(k => k !== 'pair').map(key =>
  [key, { median: distribution(pairs.map(p => p[key])).p50, min: Math.min(...pairs.map(p => p[key])), max: Math.max(...pairs.map(p => p[key])) }]));
const report = { quantiles: 'nearest rank; no interpolation', evidenceBoundary: 'Exact ordered successful widget API calls plus native submitted-state summaries; no full native-buffer readback or GPU presentation timing',
  timingScope: 'CPU/RSS/operations at source completion; cadence and latency through final sample. Warm-up excluded from deltas and distributions. Same instrumentation, with separately reported overhead; no overhead subtraction.',
  identity, retention, pairs, summaries, runs };
writeFileSync(resolve(root, 'quiet-comparison-summary.json'), JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify({ summaries, runs: runs.map(r => ({ dir: r.dir, cpu: r.cpuSeconds, rssMiB: r.rssMiB,
  frames: r.measurement.submittedFrames, snapshots: r.measurement.operations['di-table:setData'],
  fabric: r.measurement.appliedFabricPublications, cadence: r.cadenceThroughFinal, observed: r.observedSubmissionMs })) }, null, 2));
